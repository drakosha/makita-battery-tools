/*
 * Makita Battery Reader - Serial Output Functions
 */

#include "makita_print.h"
#include "makita_commands.h"
#include "makita_data.h"

void printSeparator() {
  Serial.println(F("========================================"));
}

void printHeader() {
  Serial.println();
  printSeparator();
  Serial.println(F("       MAKITA BATTERY INFORMATION"));
  printSeparator();
}

void printModel() {
  byte data[64];
  char model[16];
  bool got_model = false;

  memset(data, 0, 64);
  memset(model, 0, 16);

  if (model_cmd(data)) {
    if (data[0] == 'B' && data[1] == 'L') {
      memcpy(model, data, 6);
      model[6] = '\0';
      got_model = true;
    }
  }

  if (!got_model) {
    f0513_model_cmd(data);
    if (!(data[0] == 0xFF && data[1] == 0xFF)) {
      sprintf(model, "BL%02X%02X", data[1], data[0]);
      got_model = true;
    }
  }

  // Use cached MSG data instead of new try_charger() call
  if (!got_model && g_battery.valid) {
    int cap = get_capacity_for_model(g_battery.msg[16]);
    int type = SWAP_NIBBLES(g_battery.msg[11]);

    if (type == 14) {
      sprintf(model, "BL3626");
    } else if (SWAP_NIBBLES(g_battery.msg[25]) < 0xC) {
      sprintf(model, "BL14%02d", cap);
    } else {
      sprintf(model, "BL18%02d", cap);
    }
    got_model = true;
  }

  Serial.print(F("Model:           "));
  Serial.println(got_model ? model : "Unknown/Not detected");
}

void printBatteryInfo() {
  // Use cached data from g_battery
  if (!g_battery.valid) {
    Serial.println(F("ERROR: Cannot read battery info"));
    return;
  }

  const byte* rom = g_battery.rom;
  const byte* msg = g_battery.msg;

  // ROM ID
  Serial.print(F("ROM ID:          "));
  printHexArray(rom, 8);
  Serial.println();

  // Manufacturing date (from ROM)
  Serial.print(F("Mfg Date:        "));
  Serial.print(rom[2]);
  Serial.print('-');
  if (rom[1] < 10) Serial.print('0');
  Serial.print(rom[1]);
  Serial.print(F("-20"));
  if (rom[0] < 10) Serial.print('0');
  Serial.println(rom[0]);

  // Charge count
  int raw_count = ((int)SWAP_NIBBLES(msg[27])) | ((int)SWAP_NIBBLES(msg[26])) << 8;
  Serial.print(F("Charge Count:    "));
  Serial.println(raw_count & 0x0fff);

  // Error code (nybble 40 = byte 20 low nibble per protocol docs)
  uint8_t error_code = msg[20] & 0x0F;
  Serial.print(F("Error Code:      0x"));
  printHex(error_code);
  if (error_code == 0) Serial.println(F(" OK"));
  else if (error_code == 1) Serial.println(F(" Overloaded"));
  else if (error_code == 5) Serial.println(F(" Warning"));
  else Serial.println(F(" ERROR"));

  // Lock status - check error code AND checksums (required for charger!)
  bool locked = (error_code != 0 && error_code != 5) || !verifyMsgChecksums(msg);
  Serial.print(F("Status:          "));
  Serial.println(locked ? F("LOCKED") : F("OK"));

  // Design capacity
  Serial.print(F("Design Capacity: "));
  Serial.print(get_capacity_mah(msg[16]));
  Serial.println(F(" mAh"));

  // Battery type
  Serial.print(F("Battery Type:    "));
  Serial.println(SWAP_NIBBLES(msg[11]));

  // Calculate health metrics per protocol documentation
  // Overdischarge: p = -5*x + 160 (type5/type6 per docs)
  int overdis_raw = SWAP_NIBBLES(msg[24]);
  int undervoltage_percent = -5 * overdis_raw + 160;
  if (undervoltage_percent < 0) undervoltage_percent = 0;
  if (undervoltage_percent > 100) undervoltage_percent = 100;

  // Overload: p = 5*x - 160 (type5/type6 per docs)
  int overload_raw = SWAP_NIBBLES(msg[25]);
  int overload_percent = 5 * overload_raw - 160;
  if (overload_percent < 0) overload_percent = 0;
  if (overload_percent > 100) overload_percent = 100;

  // Health estimate from cycle count (raw_count defined above)
  int health_raw = 100 - (int)((raw_count & 0x0FFF) / 8.96f);
  uint8_t health_percent = (health_raw > 100) ? 100 : ((health_raw < 0) ? 0 : health_raw);

  if (has_health()) {
    health_percent = health();
    undervoltage_percent = overdischarge();
    overload_percent = overload();
  }

  Serial.print(F("Overload:        "));
  Serial.print(overload_percent);
  Serial.println(F("%"));

  Serial.print(F("Overdischarge:   "));
  Serial.print(undervoltage_percent);
  Serial.println(F("%"));

  Serial.print(F("Health:          "));
  Serial.print(health_percent);
  Serial.print(F("% "));
  Serial.println(has_health() ? F("(BMS)") : F("(est)"));

  // Show charge level if voltage data available
  if (g_battery.cell_count > 0) {
    float min_v = g_battery.voltages[0];
    for (int i = 1; i < g_battery.cell_count && i < 5; i++) {
      if (g_battery.voltages[i] < min_v) min_v = g_battery.voltages[i];
    }
    uint8_t soc = voltage_to_soc(min_v);
    Serial.print(F("Charge (SOC):    "));
    Serial.print(soc);
    Serial.println(F("%"));
  }
}

void printVoltages() {
  // Use cached voltage data
  if (!g_battery.valid || g_battery.cell_count == 0) {
    Serial.println(F("ERROR: Cannot read voltage data"));
    return;
  }

  int voltage_count = g_battery.cell_count;
  bool is_bl36 = g_battery.is_bl36;
  const float* data = g_battery.voltages;

  printSeparator();
  Serial.println(F("         VOLTAGE & TEMPERATURE"));
  printSeparator();

  Serial.print(F("Pack Voltage:    "));
  Serial.print(data[6], 2);  // Pack voltage is always at index 6
  Serial.println(F(" V"));

  Serial.print(F("Cell Difference: "));
  Serial.print(data[5], 3);  // Diff is always at index 5
  Serial.println(F(" V"));

  Serial.println();
  Serial.println(F("Temperature:"));

  if (!is_bl36) {
    if (data[7] > -900) {
      Serial.print(F("  Cell:    "));
      Serial.print(data[7], 1);
      Serial.println(F(" C"));
    }
    if (data[8] > -900 && data[8] > 0) {
      Serial.print(F("  MOSFET:  "));
      Serial.print(data[8], 1);
      Serial.println(F(" C"));
    }
  } else {
    Serial.print(F("  Pack:    N/A"));
    Serial.println();
  }

  Serial.println();
  Serial.println(F("Individual Cell Voltages:"));

  for (int i = 0; i < voltage_count && i < 5; i++) {
    Serial.print(F("  Cell "));
    Serial.print(i + 1);
    Serial.print(F(":       "));
    Serial.print(data[i], 3);
    Serial.println(F(" V"));
  }

  // Balance status
  Serial.println();
  float diff = data[5];
  if (diff < 0.02) {
    Serial.println(F("Balance Status:  GOOD (< 20mV)"));
  } else if (diff < 0.05) {
    Serial.println(F("Balance Status:  OK (< 50mV)"));
  } else if (diff < 0.15) {
    Serial.println(F("Balance Status:  FAIR (< 150mV)"));
  } else {
    Serial.println(F("Balance Status:  POOR (> 150mV) - Balancing needed!"));
  }
}

void printRawData() {
  printSeparator();
  Serial.println(F("         DEBUG DATA DUMP"));
  printSeparator();

  // Use cached data for consistency
  if (!g_battery.valid) {
    Serial.println(F("  No cached data - run option 1 first"));
    return;
  }

  Serial.println(F("\n[1] Voltage data:"));
  if (g_battery.cell_count > 0) {
    Serial.print(F("  Protocol: "));
    Serial.println(g_battery.is_bl36 ? F("BL36 (40V)") : F("Standard (18V)"));
    Serial.print(F("  Cells: "));
    Serial.println(g_battery.cell_count);
    for (int i = 0; i < g_battery.cell_count && i < 5; i++) {
      Serial.print(F("  Cell ")); Serial.print(i + 1);
      Serial.print(F(": ")); Serial.print(g_battery.voltages[i], 3);
      Serial.println(F(" V"));
    }
  } else {
    Serial.println(F("  Voltage read failed"));
  }

  Serial.println(F("\n[2] charger_cmd (0xF0) + MSG:"));

  const byte* rom = g_battery.rom;
  const byte* msg = g_battery.msg;

  Serial.print(F("  ROM: "));
  printHexArray(rom, 8);
  Serial.println();

  Serial.println(F("  MSG hex:"));
  for (int i = 0; i < 32; i++) {
    if (i % 16 == 0) Serial.print(F("    "));
    printHex(msg[i]);
    Serial.print(' ');
    if (i % 16 == 15) Serial.println();
  }

  Serial.println(F("\n  Key fields (per protocol docs):"));
  Serial.print(F("    [11] Type:      ")); Serial.println(SWAP_NIBBLES(msg[11]));
  Serial.print(F("    [16] Capacity:  ")); Serial.print(get_capacity_mah(msg[16])); Serial.println(F(" mAh"));

  // Error code is nybble 40 = byte 20 low nibble
  uint8_t err = msg[20] & 0x0F;
  Serial.print(F("    [20] Error:     0x")); Serial.print(err, HEX);
  if (err == 0) Serial.println(F(" OK"));
  else if (err == 1) Serial.println(F(" Overloaded"));
  else if (err == 5) Serial.println(F(" Warning"));
  else Serial.println(F(" <-- ERROR!"));

  // Checksums at nybbles 41-43 (bytes 20-21)
  Serial.print(F("    [20-21] Chksum: 0x"));
  printHex(msg[20] >> 4); printHex(msg[21] & 0x0F); printHex(msg[21] >> 4);
  Serial.println();

  // Overdischarge/overload raw values
  Serial.print(F("    [24] Overdis:   ")); Serial.print(SWAP_NIBBLES(msg[24]));
  Serial.print(F(" -> ")); Serial.print(-5 * SWAP_NIBBLES(msg[24]) + 160); Serial.println(F("%"));
  Serial.print(F("    [25] Overload:  ")); Serial.print(SWAP_NIBBLES(msg[25]));
  Serial.print(F(" -> ")); Serial.print(5 * SWAP_NIBBLES(msg[25]) - 160); Serial.println(F("%"));

  int raw_count = ((int)SWAP_NIBBLES(msg[27])) | ((int)SWAP_NIBBLES(msg[26])) << 8;
  Serial.print(F("    [26-27] Cycles: ")); Serial.println(raw_count & 0x0FFF);
}

void printDiagnosis() {
  printSeparator();
  Serial.println(F("           DIAGNOSIS"));
  printSeparator();

  if (!g_battery.valid) {
    Serial.println(F("Status: No data available"));
    return;
  }

  // Use cached data - error code is nybble 40 = byte 20 low nibble
  bool error_set = (g_battery.msg[20] & 0x0F) != 0;
  int voltage_count = g_battery.cell_count;
  const float* voltage_data = g_battery.voltages;

  // Check for problems
  bool undervoltage = false;
  bool imbalance = false;
  bool overtemp = false;

  if (voltage_count > 0) {
    for (int i = 0; i < voltage_count && i < 5; i++) {
      if (voltage_data[i] < 3.0f) undervoltage = true;
    }
    imbalance = (error_set && voltage_data[5] > 0.15f);  // diff is at index 5
    overtemp = (voltage_data[7] > 40.0f);
  }

  if (!undervoltage && !imbalance && !overtemp && !error_set) {
    Serial.println(F("Status: No problems detected"));
    return;
  }

  if (is_f0513()) {
    Serial.println(F("Status: F0513 chip - Error reset unsupported"));
    return;
  }

  if (undervoltage) {
    Serial.println(F("Problem: Cell undervoltage detected"));
    Serial.println(F("  - Charge low cell(s) individually"));
  }
  if (imbalance) {
    Serial.println(F("Problem: Cells out of balance"));
    Serial.println(F("  - Balance cells manually"));
  }
  if (overtemp) {
    Serial.println(F("Problem: Battery overheated"));
    Serial.println(F("  - Let battery cool down"));
  }
  if (error_set && !undervoltage && !imbalance) {
    Serial.println(F("Problem: Chip error"));
    Serial.println(F("  - Try resetting the battery"));
  }
}

void printMenu() {
  Serial.println();
  printSeparator();
  Serial.println(F("            MAIN MENU"));
  printSeparator();
  Serial.println(F("  1 - Read battery data"));
  Serial.println(F("  2 - Reset errors (quick)"));
  Serial.println(F("  3 - Unlock battery (aggressive)"));
  Serial.println(F("  4 - LED ON     5 - LED OFF"));
  Serial.println(F("  6 - Debug dump (raw + MSG)"));
  Serial.println(F("  7 - Check lock status"));
  printSeparator();
  Serial.println(F("  s - Save MSG   d - Compare MSG"));
  Serial.println(F("  v - Clone saved MSG to battery"));
  Serial.println(F("  a - Advanced menu"));
  Serial.println(F("  h - Show this menu"));
  printSeparator();
}

/*
 * Makita Battery Reader - Unlock and Reset Functions
 */

#include "makita_unlock.h"
#include "makita_comm.h"
#include "makita_commands.h"
#include "makita_data.h"
#include "makita_print.h"

// Clear error code and recalculate checksums (the right way to unlock!)
static void clearErrorWithChecksum(byte* msg) {
  msg[20] &= 0xF0;  // Clear error code (nybble 40)
  recalcMsgChecksums(msg);
}

// ============== MSG storage ==============

static byte saved_msg[32];
static bool msg_saved = false;

void saveMSG() {
  byte data[48];
  memset(data, 0, 48);

  if (!try_charger(data)) {
    Serial.println(F("ERROR: Cannot read battery data"));
    return;
  }

  memcpy(saved_msg, data + 8, 32);
  msg_saved = true;

  Serial.println(F("MSG saved."));
  Serial.print(F("  err=0x")); Serial.print(saved_msg[20] & 0x0F, HEX);
  Serial.print(F(" chksum=")); Serial.println(saved_msg[21] >> 4, HEX);
  int cycles = ((int)SWAP_NIBBLES(saved_msg[27])) | ((int)SWAP_NIBBLES(saved_msg[26])) << 8;
  Serial.print(F("  cycles=")); Serial.println(cycles & 0x0FFF);
}

void compareMSG() {
  if (!msg_saved) {
    Serial.println(F("No saved MSG. Use 's' first."));
    return;
  }

  byte data[48];
  memset(data, 0, 48);

  if (!try_charger(data)) {
    Serial.println(F("ERROR: Cannot read battery data"));
    return;
  }

  byte* msg = data + 8;

  printSeparator();
  Serial.println(F("  MSG COMPARISON (Saved vs Current)"));
  printSeparator();

  int changes = 0;
  for (int i = 0; i < 32; i++) {
    if (saved_msg[i] != msg[i]) {
      Serial.print(i < 10 ? " " : "");
      Serial.print(i);
      Serial.print(F(": 0x"));
      printHex(saved_msg[i]);
      Serial.print(F(" -> 0x"));
      printHex(msg[i]);
      if (i == 19) Serial.print(F(" ERR"));
      else if (i == 20) Serial.print(F(" LOCK"));
      else if (i == 26 || i == 27) Serial.print(F(" CYC"));
      Serial.println();
      changes++;
    }
  }

  Serial.println(changes ? F("") : F("No changes"));
}

void cloneMSG() {
  if (!msg_saved) {
    Serial.println(F("No saved MSG. Use 's' first with working battery."));
    return;
  }

  printSeparator();
  Serial.println(F("  CLONE SAVED MSG"));
  printSeparator();
  Serial.println(F("This writes saved MSG to current battery."));
  Serial.println(F("Press 'y' to confirm:"));

  while (!Serial.available()) delay(10);
  char c = Serial.read();
  while (Serial.available()) Serial.read();

  if (c != 'y' && c != 'Y') {
    Serial.println(F("Cancelled"));
    return;
  }

  // Create clone with cleared error and recalculated checksums
  byte clone_msg[32];
  memcpy(clone_msg, saved_msg, 32);
  clearErrorWithChecksum(clone_msg);

  Serial.println(F("Writing with valid checksums..."));
  write_msg_to_eeprom(clone_msg);

  // Verify
  byte data[48];
  if (try_charger(data)) {
    byte* msg = data + 8;
    Serial.print(F("Result: err=0x"));
    Serial.print(msg[20] & 0x0F, HEX);
    Serial.print(F(" chksum="));
    Serial.println(msg[21] >> 4, HEX);
  }
  Serial.println(F("Done."));
}

// ============== Reset operations ==============

void resetBatteryErrors() {
  Serial.println(F("Resetting errors..."));
  for (int i = 0; i < 3; i++) {
    delay(300);
    testmode_cmd();
    reset_error_cmd();
    Serial.print('.');
  }
  Serial.println(F("\nReset complete."));
}

void unlockBattery() {
  printSeparator();
  Serial.println(F("     AGGRESSIVE BATTERY UNLOCK"));
  printSeparator();

  // Phase 1: Standard reset commands
  Serial.println(F("\nPhase 1: Standard reset..."));
  for (int cycle = 0; cycle < 5; cycle++) {
    Serial.print(F("  Cycle "));
    Serial.print(cycle + 1);

    trigger_power();
    for (int i = 0; i < 5; i++) {
      delay(200);
      testmode_cmd();
      reset_error_cmd();
      Serial.print('.');
    }
    Serial.println();

    if (!isBatteryLocked()) {
      Serial.println(F("\n*** SUCCESS: Battery unlocked! ***"));
      return;
    }
  }

  // Phase 2: Clear error with checksum recalculation (per protocol docs)
  Serial.println(F("\nPhase 2: Clearing EEPROM with checksum fix..."));

  byte charger_data[48];
  memset(charger_data, 0, 48);

  if (try_charger(charger_data)) {
    byte* raw_msg = charger_data + 8;

    // Clear error code and recalculate checksums (the correct way!)
    clearErrorWithChecksum(raw_msg);

    Serial.print(F("  New checksums: "));
    Serial.print(raw_msg[20] >> 4, HEX);
    Serial.print(F("/"));
    Serial.print(raw_msg[21] & 0x0F, HEX);
    Serial.print(F("/"));
    Serial.println(raw_msg[21] >> 4, HEX);

    for (int attempt = 0; attempt < 3; attempt++) {
      Serial.print(F("  Write "));
      Serial.print(attempt + 1);

      write_msg_to_eeprom(raw_msg);

      // Full power cycle to commit EEPROM
      Serial.print(F(" power cycle..."));
      set_enablepin(false);
      delay(2000);
      set_enablepin(true);
      delay(1000);

      if (!isBatteryLocked()) {
        Serial.println(F("\n*** SUCCESS: Battery unlocked! ***"));
        return;
      }
      Serial.println(F(" still locked"));
    }
  }

  // Phase 3: Extended power cycling
  Serial.println(F("\nPhase 3: Power cycling..."));
  for (int cycle = 0; cycle < 3; cycle++) {
    set_enablepin(false);
    delay(2000);
    set_enablepin(true);
    delay(1000);

    for (int i = 0; i < 10; i++) {
      testmode_cmd();
      delay(100);
      reset_error_cmd();
      delay(100);
    }

    if (!isBatteryLocked()) {
      Serial.println(F("\n*** SUCCESS: Battery unlocked! ***"));
      return;
    }
  }

  Serial.println(F("\nUnlock failed. May need cell charging or PCB replacement."));
}

void factoryResetBattery() {
  Serial.println(F("\nFactory Reset: 1=minimal, 2=0xC1, 3=0x94, 0=cancel"));

  while (!Serial.available()) delay(10);
  char opt = Serial.read();
  while (Serial.available()) Serial.read();

  if (opt == '0') { Serial.println(F("Cancelled")); return; }

  byte data[48];
  if (!try_charger(data)) { Serial.println(F("Read failed")); return; }
  byte* msg = data + 8;

  // Apply template modifications
  if (opt == '2') { msg[8] = msg[9] = 0xC1; msg[24] = 0x92; }
  else if (opt == '3') { msg[8] = msg[9] = 0x94; msg[24] = 0x02; }

  // Clear error and recalculate all checksums
  clearErrorWithChecksum(msg);

  Serial.print(F("Checksums: "));
  Serial.print(msg[20] >> 4, HEX);
  Serial.print(F("/"));
  Serial.print(msg[21] & 0x0F, HEX);
  Serial.print(F("/"));
  Serial.println(msg[21] >> 4, HEX);

  write_msg_to_eeprom(msg);

  if (try_charger(data)) {
    msg = data + 8;
    Serial.print(F("Result: err=0x"));
    Serial.print(msg[20] & 0x0F, HEX);
    Serial.print(F(" chksum="));
    Serial.println(msg[21] >> 4, HEX);
  }
  Serial.println(F("Done."));
}

void resetCycleCount() {
  byte data[48];

  if (!try_charger(data)) {
    Serial.println(F("ERROR: Cannot read battery"));
    return;
  }

  byte* msg = data + 8;

  // Show current cycle count
  int old_count = ((int)SWAP_NIBBLES(msg[27])) | ((int)SWAP_NIBBLES(msg[26])) << 8;
  old_count &= 0x0FFF;
  Serial.print(F("Current cycles: "));
  Serial.println(old_count);

  // Ask for new value
  Serial.println(F("Enter new cycle count (0-4095), or 'c' to cancel:"));

  // Read number from serial
  char buf[8];
  int idx = 0;
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == 'c' || c == 'C') {
        Serial.println(F("Cancelled"));
        return;
      }
      if (c == '\r' || c == '\n') {
        if (idx > 0) break;
      } else if (c >= '0' && c <= '9' && idx < 7) {
        buf[idx++] = c;
        Serial.print(c);
      }
    }
  }
  buf[idx] = '\0';
  Serial.println();

  int new_cycles = atoi(buf);
  if (new_cycles > 4095) new_cycles = 4095;

  Serial.print(F("Setting cycles to: "));
  Serial.println(new_cycles);

  // Encode cycle count (swap nibbles format)
  msg[26] = SWAP_NIBBLES((new_cycles >> 8) & 0xFF);
  msg[27] = SWAP_NIBBLES(new_cycles & 0xFF);

  // Write to EEPROM (safe write recalculates checksums including chk5 for cycle count)
  write_msg_safe(msg);

  // Verify
  if (try_charger(data)) {
    msg = data + 8;
    int verify_count = ((int)SWAP_NIBBLES(msg[27])) | ((int)SWAP_NIBBLES(msg[26])) << 8;
    verify_count &= 0x0FFF;
    Serial.print(F("Verified: "));
    Serial.println(verify_count);
  }
  Serial.println(F("Done."));
}

void resetHandshakeState() {
  byte rsp[48];

  printSeparator();
  Serial.println(F("  RESET HANDSHAKE STATE"));
  printSeparator();

  Serial.println(F("\n[1] Power cycle (3s)..."));
  set_enablepin(false);
  delay(3000);
  set_enablepin(true);
  delay(1000);

  Serial.println(F("[2] Reset sequence..."));
  for (int i = 0; i < 10; i++) {
    Serial.print('.');
    testmode_cmd();
    delay(50);
    reset_error_cmd();
    delay(50);
    if (i % 3 == 2) {
      set_enablepin(false);
      delay(200);
      set_enablepin(true);
      delay(300);
    }
  }
  Serial.println();

  Serial.println(F("[3] Clear EEPROM with checksum fix..."));
  memset(rsp, 0, 48);
  if (try_charger(rsp)) {
    byte* msg = rsp + 8;
    clearErrorWithChecksum(msg);
    write_msg_to_eeprom(msg);
  }

  Serial.println(F("[4] Final power cycle..."));
  set_enablepin(false);
  delay(2000);
  set_enablepin(true);
  delay(1000);

  Serial.println(F("\nTry Makita charger now."));
}

void lockBatteryForTest() {
  printSeparator();
  Serial.println(F("  LOCK BATTERY (TEST)"));
  printSeparator();
  Serial.println(F("  1 - Bad checksum (silent)"));
  Serial.println(F("  2 - err=1 Overloaded"));
  Serial.println(F("  3 - err=5 Warning"));
  Serial.println(F("  4 - err=F Dead"));
  Serial.println(F("  0 - Cancel"));

  while (!Serial.available()) delay(10);
  char opt = Serial.read();
  while (Serial.available()) Serial.read();

  if (opt == '0') {
    Serial.println(F("Cancelled"));
    return;
  }

  if (opt < '1' || opt > '4') {
    Serial.println(F("Invalid option"));
    return;
  }

  byte data[48];
  if (!try_charger(data)) {
    Serial.println(F("Read failed"));
    return;
  }

  byte* msg = data + 8;

  Serial.print(F("Current err=0x"));
  Serial.print(msg[20] & 0x0F, HEX);
  Serial.print(F(" chk="));
  Serial.print(msg[20] >> 4, HEX);
  Serial.print(F("/"));
  Serial.print(msg[21] & 0x0F, HEX);
  Serial.print(F("/"));
  Serial.println(msg[21] >> 4, HEX);

  if (opt == '1') {
    // Corrupt checksum3 (nybble 43) - flip bits
    msg[21] ^= 0xF0;
    Serial.println(F("Corrupting checksum..."));
    write_msg_to_eeprom(msg);  // Raw write, no recalc
  } else {
    // Set error code based on option
    byte err_code = 0;
    switch (opt) {
      case '2': err_code = 0x01; break;  // Overloaded
      case '3': err_code = 0x05; break;  // Warning
      case '4': err_code = 0x0F; break;  // Dead
    }
    msg[20] = (msg[20] & 0xF0) | err_code;
    Serial.print(F("Setting error=0x"));
    Serial.print(err_code, HEX);
    Serial.println(F("..."));
    write_msg_safe(msg);  // Recalc checksums with error set
  }

  // Full power cycle to activate error indication
  Serial.println(F("Power cycling..."));
  set_enablepin(false);
  delay(2000);
  set_enablepin(true);
  delay(1000);

  // Try to activate LED indication
  leds_on_cmd();
  delay(100);

  // Verify
  if (try_charger(data)) {
    msg = data + 8;
    Serial.print(F("Result: err=0x"));
    Serial.print(msg[20] & 0x0F, HEX);
    Serial.print(F(" chk3=0x"));
    Serial.print(msg[21] >> 4, HEX);
    Serial.print(F(" locked="));
    Serial.println(isBatteryLocked() ? F("YES") : F("NO"));
  }
  Serial.println(F("Done. Try pressing battery button."));
}

void advancedResetMenu() {
  printSeparator();
  Serial.println(F("      ADVANCED RESET"));
  printSeparator();
  Serial.println(F("  1 - Factory reset"));
  Serial.println(F("  2 - Reset handshake"));
  Serial.println(F("  3 - Set cycle count"));
  Serial.println(F("  4 - LOCK battery (test)"));
  Serial.println(F("  0 - Cancel"));

  while (!Serial.available()) delay(10);
  char opt = Serial.read();
  while (Serial.available()) Serial.read();

  switch (opt) {
    case '1': factoryResetBattery(); break;
    case '2': resetHandshakeState(); break;
    case '3': resetCycleCount(); break;
    case '4': lockBatteryForTest(); break;
    default: Serial.println(F("Cancelled")); break;
  }
}

// ============== Charger diagnostics ==============

void diagnoseChargerHandshake() {
  byte rsp[48];

  printSeparator();
  Serial.println(F("  CHARGER HANDSHAKE TEST"));
  printSeparator();

  Serial.println(F("\n[1] Battery Info:"));
  memset(rsp, 0, 48);
  if (try_charger(rsp)) {
    Serial.print(F("  Error: 0x"));
    Serial.print(rsp[27] & 0x0F, HEX);
    Serial.print(F("  Lock: 0x"));
    Serial.println(rsp[28] & 0x0F, HEX);
  } else {
    Serial.println(F("  FAILED!"));
  }

  Serial.println(F("\n[2] Temperature:"));
  makita.reset();
  delay(100);
  cell_temperature();  // Warm-up
  delay(50);

  float t_cell = cell_temperature();
  float t_mosfet = mosfet_temperature();

  Serial.print(F("  Cell:   "));
  if (t_cell > -900) {
    Serial.print(t_cell, 1);
    Serial.println((t_cell < 0 || t_cell > 50) ? F("C BAD!") : F("C OK"));
  } else {
    Serial.println(F("NO RESPONSE!"));
  }

  Serial.print(F("  MOSFET: "));
  if (t_mosfet > -900) {
    Serial.print(t_mosfet, 1);
    Serial.println(F("C"));
  } else {
    Serial.println(F("NO RESPONSE"));
  }

  Serial.println(F("\n[3] Voltage Data:"));
  memset(rsp, 0, 32);
  read_data_request(rsp);
  Serial.println((rsp[0] == 0xFF) ? F("  FAILED (F0513?)") : F("  OK"));

  Serial.println(F("\n[4] Battery Type:"));
  Serial.println(has_health() ? F("  NEW (has_health)") : F("  OLD"));

  printSeparator();
  bool temp_ok = (t_cell > 0 && t_cell < 50);
  Serial.println(temp_ok ? F("All checks PASSED") : F("Temperature issue detected"));
  printSeparator();
}

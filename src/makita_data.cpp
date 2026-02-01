/*
 * Makita Battery Reader - Data Functions
 */

#include "makita_data.h"
#include "makita_comm.h"
#include "makita_commands.h"

// ============== Utility ==============

int round5(int in) {
  static signed char cAdd[5] = { 0, -1, -2, 2, 1 };
  return in + cAdd[in % 5];
}

// Calculate SOC from cell voltage (Li-ion curve)
uint8_t voltage_to_soc(float voltage) {
  if (voltage >= 4.20f) return 100;
  if (voltage <= 3.00f) return 0;
  // Linear approximation between 3.0V (0%) and 4.2V (100%)
  // More accurate for mid-range, slight error at extremes
  return (uint8_t)((voltage - 3.0f) * 83.33f);
}

// ============== Capacity helpers ==============

bool is_new_capacity_format(byte cap_byte) {
  byte swapped = SWAP_NIBBLES(cap_byte);
  return (swapped > 60 && cap_byte <= 8 && cap_byte > 0);
}

uint32_t get_capacity_mah(byte cap_byte) {
  if (is_new_capacity_format(cap_byte)) {
    return (uint32_t)cap_byte * 1000L;
  } else {
    return (uint32_t)SWAP_NIBBLES(cap_byte) * 100L;
  }
}

int get_capacity_for_model(byte cap_byte) {
  if (is_new_capacity_format(cap_byte)) {
    return cap_byte * 10;
  } else {
    return round5(SWAP_NIBBLES(cap_byte));
  }
}

// ============== Health and status ==============

bool has_health() {
  byte rsp[4];
  memset(rsp, 0, 4);
  byte cmd_params[] = { 0xD4, 0xBA, 0x00, 0x01 };
  cmd_and_read_cc(cmd_params, 4, rsp, 2);
  return rsp[1] == 0x06;
}

byte overload() {
  byte rsp[16];
  memset(rsp, 0, 16);
  byte cmd_params[] = { 0xD4, 0x8D, 0x00, 0x07 };
  cmd_and_read_cc(cmd_params, 4, rsp, 8);
  return (rsp[5] & 0xf0) >> 4 | (rsp[6] & 0x70);
}

byte overdischarge() {
  byte rsp[4];
  memset(rsp, 0, 4);
  byte cmd_params[] = { 0xD4, 0xBA, 0x00, 0x01 };
  cmd_and_read_cc(cmd_params, 4, rsp, 2);
  if (rsp[0] == 0xFF) return 0;
  int val = rsp[0] << 1;
  return (val > 100) ? 100 : val;
}

byte health() {
  byte rsp[4];
  memset(rsp, 0, 4);
  byte cmd_params[] = { 0xD4, 0x50, 0x01, 0x02 };
  cmd_and_read_cc(cmd_params, 4, rsp, 3);
  if (rsp[1] == 0xFF || rsp[1] < 10) return 100;
  int val = 14 * (rsp[1] - 10);
  return (val > 100) ? 100 : ((val < 0) ? 0 : val);
}

// ============== Temperature ==============

float cell_temperature() {
  byte rsp[4];
  memset(rsp, 0, 4);
  byte cmd_params[] = { 0xD7, 0x0E, 0x00, 0x02 };
  cmd_and_read_cc(cmd_params, 4, rsp, 3);
  if (rsp[0] == 0xFF && rsp[1] == 0xFF) return -999.0f;
  return (((rsp[0]) | ((int32_t)rsp[1]) << 8) / 10.0f) - 273.15f;
}

float mosfet_temperature() {
  byte rsp[4];
  memset(rsp, 0, 4);
  byte cmd_params[] = { 0xD7, 0x10, 0x00, 0x02 };
  cmd_and_read_cc(cmd_params, 4, rsp, 3);
  if (rsp[0] == 0xFF && rsp[1] == 0xFF) return -999.0f;
  return (((rsp[0]) | ((int32_t)rsp[1]) << 8) / 10.0f) - 273.15f;
}

// ============== Voltage info ==============

bool get_voltage_info(float output[]) {
  bool f0513_mode = false;
  uint8_t data[128];
  memset(data, 0, 128);

  read_data_request(data);

  float t_cell = 0;
  float t_mosfet = 0;

  if (data[0] == 0xff && data[1] == 0xff) {
    memset(data, 0xff, 32);
    f0513_mode = true;
  } else {
    t_cell = cell_temperature();
    t_mosfet = mosfet_temperature();
  }

  if (f0513_mode) {
    f0513_vcell_cmd(0x31, data + 2);
    f0513_vcell_cmd(0x32, data + 4);
    f0513_vcell_cmd(0x33, data + 6);
    f0513_vcell_cmd(0x34, data + 8);
    f0513_vcell_cmd(0x35, data + 10);
    f0513_temp_cmd(data + 14);
    int temp_raw = ((int)data[14] | ((int)data[15]) << 8);
    float temp_100 = temp_raw / 100.0f;
    float temp_256 = temp_raw / 256.0f;
    t_cell = (temp_100 > 45.0f) ? temp_256 : temp_100;
    t_mosfet = 0;
  }

  if (data[2] == 0xff && data[3] == 0xff) return false;

  // Read raw voltage values
  float v_cell1 = ((int)data[2] | ((int)data[3]) << 8) / 1000.0f;
  float v_cell2 = ((int)data[4] | ((int)data[5]) << 8) / 1000.0f;
  float v_cell3 = ((int)data[6] | ((int)data[7]) << 8) / 1000.0f;
  float v_cell4 = ((int)data[8] | ((int)data[9]) << 8) / 1000.0f;
  float v_cell5 = ((int)data[10] | ((int)data[11]) << 8) / 1000.0f;

  // Old chips return doubled voltages - correct if > 5V
  if (v_cell1 > 5.0f || v_cell2 > 5.0f || v_cell3 > 5.0f ||
      v_cell4 > 5.0f || v_cell5 > 5.0f) {
    v_cell1 /= 2.0f;
    v_cell2 /= 2.0f;
    v_cell3 /= 2.0f;
    v_cell4 /= 2.0f;
    v_cell5 /= 2.0f;
  }

  float max_v = v_cell1, min_v = v_cell1;
  if (v_cell2 > max_v) max_v = v_cell2;
  if (v_cell3 > max_v) max_v = v_cell3;
  if (v_cell4 > max_v) max_v = v_cell4;
  if (v_cell5 > max_v) max_v = v_cell5;
  if (v_cell2 < min_v) min_v = v_cell2;
  if (v_cell3 < min_v) min_v = v_cell3;
  if (v_cell4 < min_v) min_v = v_cell4;
  if (v_cell5 < min_v) min_v = v_cell5;

  output[0] = v_cell1;
  output[1] = v_cell2;
  output[2] = v_cell3;
  output[3] = v_cell4;
  output[4] = v_cell5;
  output[5] = max_v - min_v;
  output[6] = v_cell1 + v_cell2 + v_cell3 + v_cell4 + v_cell5;
  output[7] = t_cell;
  output[8] = t_mosfet;
  return true;
}

// ============== Checksum functions ==============

// Calculate checksum for nybble range: min(sum, 0xff) & 0x0f
static byte calcNybbleSum(const byte* msg, int startByte, int endByte, bool lastLowOnly) {
  int sum = 0;
  int end = lastLowOnly ? endByte - 1 : endByte;
  for (int i = startByte; i <= end; i++) {
    sum += (msg[i] & 0x0F) + (msg[i] >> 4);
  }
  if (lastLowOnly) {
    sum += (msg[endByte] & 0x0F);
  }
  return (sum > 255 ? 255 : sum) & 0x0F;
}

bool verifyMsgChecksums(const byte* msg) {
  if (msg[20] == 0xFF && msg[21] == 0xFF) return false;

  // Primary checksums (control lock status)
  byte chk1 = calcNybbleSum(msg, 0, 7, false);   // nybbles 0-15
  byte chk2 = calcNybbleSum(msg, 8, 15, false);  // nybbles 16-31
  byte chk3 = calcNybbleSum(msg, 16, 20, true);  // nybbles 32-40

  bool primary_ok = (chk1 == (msg[20] >> 4)) &&
                    (chk2 == (msg[21] & 0x0F)) &&
                    (chk3 == (msg[21] >> 4));

  // Secondary checksums (for data integrity)
  byte chk4 = calcNybbleSum(msg, 22, 23, false); // nybbles 44-47
  byte chk5 = calcNybbleSum(msg, 24, 30, false); // nybbles 48-61

  bool secondary_ok = (chk4 == (msg[31] & 0x0F)) &&
                      (chk5 == (msg[31] >> 4));

  return primary_ok && secondary_ok;
}

void recalcMsgChecksums(byte* msg) {
  // Primary checksums (nybbles 41-43) - these control lock status
  byte chk1 = calcNybbleSum(msg, 0, 7, false);   // nybbles 0-15
  byte chk2 = calcNybbleSum(msg, 8, 15, false);  // nybbles 16-31
  byte chk3 = calcNybbleSum(msg, 16, 20, true);  // nybbles 32-40

  msg[20] = (msg[20] & 0x0F) | (chk1 << 4);
  msg[21] = (chk2 & 0x0F) | (chk3 << 4);

  // Secondary checksums (nybbles 62-63) - for cycle count etc.
  byte chk4 = calcNybbleSum(msg, 22, 23, false); // nybbles 44-47
  byte chk5 = calcNybbleSum(msg, 24, 30, false); // nybbles 48-61

  msg[31] = (chk4 & 0x0F) | (chk5 << 4);
}

// ============== Lock status ==============

bool isBatteryLocked() {
  byte data[64];
  memset(data, 0, 64);
  if (!try_charger(data)) return true;

  byte* msg = data + 8;
  byte err = msg[20] & 0x0F;

  // Error code check (0=OK, 5=Warning are acceptable)
  if (err != 0 && err != 5) return true;

  // Verify checksums
  return !verifyMsgChecksums(msg);
}

// ============== Cached data read ==============

bool readAllBatteryData() {
  // Clear previous data
  memset(&g_battery, 0, sizeof(g_battery));
  g_battery.valid = false;

  // Warm up the battery first
  warmup_battery();

  // Read charger data (ROM + MSG) - this is the most important
  byte charger_data[48];
  memset(charger_data, 0, 48);

  if (!try_charger(charger_data)) {
    return false;
  }

  // Copy ROM and MSG
  memcpy(g_battery.rom, charger_data, 8);
  memcpy(g_battery.msg, charger_data + 8, 32);

  // IMPORTANT: After 0x33 commands, first 0xCC commands fail
  // Do warm-up reads before voltage reading
  makita.reset();
  delay(100);
  cell_temperature();  // discard
  delay(50);
  cell_temperature();  // discard
  delay(50);

  // Try to read voltages (5-cell standard)
  if (get_voltage_info(g_battery.voltages)) {
    g_battery.cell_count = 5;
    g_battery.is_bl36 = false;
    g_battery.valid = true;
    return true;
  }

  // Try BL36 (10-cell 40V)
  float bl36_data[13];
  if (bl36_voltages(bl36_data)) {
    memcpy(g_battery.voltages, bl36_data, sizeof(g_battery.voltages));
    g_battery.cell_count = 10;
    g_battery.is_bl36 = true;
    g_battery.valid = true;
    return true;
  }

  // Charger data is valid even without voltages
  g_battery.valid = true;
  g_battery.cell_count = 0;
  return true;
}

/*
 * Makita Battery Reader - Protocol Commands
 */

#include "makita_commands.h"
#include "makita_comm.h"
#include "makita_data.h"

// ============== F0513 chip commands ==============

void f0513_second_command_tree() {
  byte cmd[] = { 0x99 };
  byte rsp[16];
  memset(rsp, 0, 16);
  cmd_and_read_cc(cmd, 1, rsp, 0);
  makita.reset();
  delayMicroseconds(310);
}

void f0513_model_cmd(byte rsp[]) {
  f0513_second_command_tree();
  makita.write(0x31);
  delayMicroseconds(90);
  rsp[0] = makita.read();
  delayMicroseconds(90);
  rsp[1] = makita.read();
}

void f0513_version_cmd(byte rsp[]) {
  f0513_second_command_tree();
  makita.write(0x32);
  delayMicroseconds(90);
  rsp[0] = makita.read();
  delayMicroseconds(90);
  rsp[1] = makita.read();
}

void f0513_vcell_cmd(byte cmd_byte, byte rsp[]) {
  byte cmd_params[] = { cmd_byte };
  cmd_and_read_cc(cmd_params, 1, rsp, 2);
}

void f0513_temp_cmd(byte rsp[]) {
  byte cmd_params[] = { 0x52 };
  cmd_and_read_cc(cmd_params, 1, rsp, 2);
}

bool is_f0513() {
  byte data[32];
  f0513_model_cmd(data);
  return !(data[0] == 0xFF && data[1] == 0xFF);
}

// ============== Standard battery commands ==============

bool model_cmd(byte rsp[]) {
  byte cmd_params[] = { 0xDC, 0x0C };
  for (int i = 0; i < 10; i++) {
    if (cmd_and_read_cc(cmd_params, 2, rsp, 10)) return true;
  }
  return false;
}

void read_data_request(byte rsp[]) {
  byte cmd_params[] = { 0xD7, 0x00, 0x00, 0xFF };
  cmd_and_read_cc(cmd_params, 4, rsp, 29);
}

bool charger_33_cmd(byte rsp[]) {
  byte cmd_params[] = { 0xF0, 0x00 };
  return cmd_and_read_33(cmd_params, 2, rsp, 32);
}

bool try_charger(byte rsp[]) {
  for (int i = 0; i < 20; i++) {
    if (charger_33_cmd(rsp)) return true;
  }
  return false;
}

// ============== Control commands ==============

void testmode_cmd() {
  byte cmd_params[] = { 0xD9, 0x96, 0xA5 };
  cmd_and_read_33(cmd_params, 3, g_buf, 29);
}

void exit_testmode_cmd() {
  byte cmd_params[] = { 0xD9, 0xFF, 0xFF };
  cmd_and_read_33(cmd_params, 3, g_buf, 1);
}

// Unified DA command - saves ~100 bytes Flash
void send_da_cmd(byte sub_cmd) {
  byte cmd_params[] = { 0xDA, sub_cmd };
  cmd_and_read_33(cmd_params, 2, g_buf, 9);
}

// ============== EEPROM operations ==============

bool read_msg_cmd(byte rsp[]) {
  byte cmd_params[] = { 0xAA, 0x00 };
  return cmd_and_read_33(cmd_params, 2, rsp, 40);
}

void store_cmd_direct(byte data[]) {
  byte rsp[16];

  // Reset and prepare
  for (int i = 0; !makita.reset(); i++) {
    if (i == 5) return;
    delay(100);
  }
  delayMicroseconds(310);

  // Send ROM read command first
  makita.write(0x33);
  makita.read_bytes(rsp, 8);

  // Write command: 0x0F 0x00 + 32 bytes data
  makita.write(0x0F);
  makita.write(0x00);
  makita.write_bytes(data, 32);

  delay(500);  // Wait for scratchpad write

  // Commit to EEPROM - try multiple times
  for (int retry = 0; retry < 3; retry++) {
    for (int i = 0; !makita.reset(); i++) {
      if (i == 5) break;
      delay(100);
    }
    delayMicroseconds(310);

    makita.write(0x33);
    makita.read_bytes(rsp, 8);

    makita.write(0x55);
    makita.write(0xA5);

    delay(500);  // EEPROM write time (10ms per byte * 32 = 320ms min)
  }
}

// Combined EEPROM write sequence (raw - caller must ensure valid checksums)
void write_msg_to_eeprom(byte* msg) {
  testmode_cmd();
  delay(100);
  charger_33_cmd(g_buf);  // Dummy read
  delay(100);
  store_cmd_direct(msg);
  delay(500);
  exit_testmode_cmd();  // Exit testmode to commit changes!
  delay(200);
  trigger_power();
  delay(300);
}

// Safe EEPROM write - recalculates all checksums before writing
void write_msg_safe(byte* msg) {
  recalcMsgChecksums(msg);
  write_msg_to_eeprom(msg);
}

// ============== BL36 (40V) commands ==============

bool bl36_testmode() {
  byte cmd[] = { 0x10, 0x21 };
  return cmd_and_read_cc(cmd, 2, cmd, 0);
}

static inline float code_to_voltage_u16(uint16_t raw16) {
  static const float COUNTS_PER_VOLT = 11916;
  static const float C_INTERCEPT = 5.5;
  return C_INTERCEPT - (float)raw16 / COUNTS_PER_VOLT;
}

bool bl36_voltages(float voltages[]) {
  byte rsp[64];
  float max_v = -5;
  float min_v = 5;
  float vTotal = 0;

  memset(rsp, 0, 64);

  if (!(bl36_testmode() && cmd_and_read(0xd4, rsp, 0, rsp, 20))) {
    return false;
  }

  for (int i = 0; i < 10; i++) {
    voltages[i] = code_to_voltage_u16((int)rsp[i * 2] | ((int)rsp[i * 2 + 1]) << 8);
    vTotal += voltages[i];
    if (voltages[i] > max_v) max_v = voltages[i];
    if (voltages[i] < min_v) min_v = voltages[i];
  }

  voltages[10] = max_v - min_v;
  voltages[11] = vTotal;
  voltages[12] = 0;

  return true;
}

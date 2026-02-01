/*
 * Makita Battery Reader - Low-level Communication
 */

#include "makita_comm.h"

// Global OneWire instance
OneWire makita(ONEWIRE_PIN);

// Shared buffer - saves ~200 bytes RAM vs local arrays
byte g_buf[SHARED_BUF_SIZE];

// Global cached battery data
BatteryData g_battery;

void set_enablepin(bool high) {
  digitalWrite(ENABLE_PIN, high ? HIGH : LOW);
}

void trigger_power() {
  set_enablepin(false);
  delay(200);
  set_enablepin(true);
  delay(500);
}

bool cmd_and_read(uint8_t initial, uint8_t *cmd, uint8_t cmd_len, byte *rsp, uint8_t rsp_len) {
  int offset = (initial == 0x33 ? 8 : 0);
  memset(rsp, 0xff, rsp_len + offset);

  for (int i = 0; !makita.reset(); i++) {
    if (i == 5) {
      trigger_power();
      return false;
    }
    delay(500);
  }

  delayMicroseconds(310);

  if (offset) {
    // 0x33 command - read ROM ID first, then send command, then read response
    makita.write(initial);
    makita.read_bytes(rsp, offset);
    makita.write_bytes(cmd, cmd_len);
  } else {
    // 0xCC command - skip ROM, send command
    makita.write(initial);
    makita.write_bytes(cmd, cmd_len);
  }

  makita.read_bytes(rsp + offset, rsp_len);

  if (rsp_len < 3 || !(rsp[offset] == 0xFF && rsp[1 + offset] == 0xFF && rsp[2 + offset] == 0xff)) {
    return true;
  } else {
    trigger_power();
    return false;
  }
}

bool cmd_and_read_33(uint8_t *cmd, uint8_t cmd_len, byte *rsp, uint8_t rsp_len) {
  return cmd_and_read(0x33, cmd, cmd_len, rsp, rsp_len);
}

bool cmd_and_read_cc(uint8_t *cmd, uint8_t cmd_len, byte *rsp, uint8_t rsp_len) {
  return cmd_and_read(0xcc, cmd, cmd_len, rsp, rsp_len);
}

// Warm-up sequence - stabilizes communication before real reads
void warmup_battery() {
  byte dummy[16];

  // Trigger power cycle to wake battery
  trigger_power();
  delay(200);

  // Do several dummy reads to stabilize
  for (int i = 0; i < 3; i++) {
    makita.reset();
    delay(100);

    // Dummy temperature read (lightweight command)
    byte cmd[] = { 0xD7, 0x0E, 0x00, 0x02 };
    cmd_and_read_cc(cmd, 4, dummy, 3);
    delay(50);
  }

  // Final reset before real operations
  makita.reset();
  delay(100);
}

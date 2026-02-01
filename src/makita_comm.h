/*
 * Makita Battery Reader - Low-level Communication
 */

#ifndef MAKITA_COMM_H
#define MAKITA_COMM_H

#include "config.h"

// Power control
void set_enablepin(bool high);
void trigger_power();

// Low-level OneWire commands
bool cmd_and_read(uint8_t initial, uint8_t *cmd, uint8_t cmd_len, byte *rsp, uint8_t rsp_len);
bool cmd_and_read_33(uint8_t *cmd, uint8_t cmd_len, byte *rsp, uint8_t rsp_len);
bool cmd_and_read_cc(uint8_t *cmd, uint8_t cmd_len, byte *rsp, uint8_t rsp_len);

// Warm-up sequence for stable communication
void warmup_battery();

#endif

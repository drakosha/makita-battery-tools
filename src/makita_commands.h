/*
 * Makita Battery Reader - Protocol Commands
 */

#ifndef MAKITA_COMMANDS_H
#define MAKITA_COMMANDS_H

#include "config.h"

// F0513 chip commands (older batteries)
void f0513_second_command_tree();
void f0513_model_cmd(byte rsp[]);
void f0513_version_cmd(byte rsp[]);
void f0513_vcell_cmd(byte cmd_byte, byte rsp[]);
void f0513_temp_cmd(byte rsp[]);
bool is_f0513();

// Standard battery commands
bool model_cmd(byte rsp[]);
void read_data_request(byte rsp[]);
bool charger_33_cmd(byte rsp[]);
bool try_charger(byte rsp[]);

// Control commands
void testmode_cmd();
void exit_testmode_cmd();  // Exit test mode - required after EEPROM write!
void send_da_cmd(byte sub_cmd);  // Unified DA command
inline void reset_error_cmd() { send_da_cmd(0x04); }
inline void leds_on_cmd() { send_da_cmd(0x31); }
inline void leds_off_cmd() { send_da_cmd(0x34); }

// EEPROM operations
bool read_msg_cmd(byte rsp[]);
void store_cmd_direct(byte data[]);
void write_msg_to_eeprom(byte* msg);  // Raw write (caller ensures checksums)
void write_msg_safe(byte* msg);       // Safe write (auto-recalculates checksums)

// BL36 (40V) commands
bool bl36_testmode();
bool bl36_voltages(float voltages[]);

#endif

/*
 * Makita Battery Reader - Data Functions
 */

#ifndef MAKITA_DATA_H
#define MAKITA_DATA_H

#include "config.h"

// Utility
int round5(int in);
uint8_t voltage_to_soc(float voltage);

// Capacity helpers
bool is_new_capacity_format(byte cap_byte);
uint32_t get_capacity_mah(byte cap_byte);
int get_capacity_for_model(byte cap_byte);

// Health and status
bool has_health();
byte overload();
byte overdischarge();
byte health();

// Temperature
float cell_temperature();
float mosfet_temperature();

// Voltage info (output array: [0-4]=cells, [5]=diff, [6]=pack, [7]=t_cell, [8]=t_mosfet)
bool get_voltage_info(float output[]);

// Checksum functions (per protocol docs)
// Calculates checksum for MSG: min(sum(nybbles), 0xff) & 0x0f
bool verifyMsgChecksums(const byte* msg);
void recalcMsgChecksums(byte* msg);

// Lock status check
bool isBatteryLocked();

// Read all battery data into g_battery cache
bool readAllBatteryData();

#endif

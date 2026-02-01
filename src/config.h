/*
 * Makita Battery Reader - Configuration
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Pin configuration
#define ONEWIRE_PIN 6
#define ENABLE_PIN 8

// Utility macro
#define SWAP_NIBBLES(x) ((x & 0x0F) << 4 | (x & 0xF0) >> 4)

// Shared buffer to reduce RAM usage (~200 bytes saved)
// Used by functions that don't call each other
#define SHARED_BUF_SIZE 64
extern byte g_buf[SHARED_BUF_SIZE];

// Cached battery data - read once, use everywhere
struct BatteryData {
  byte rom[8];           // ROM ID
  byte msg[32];          // MSG data from charger command
  float voltages[9];     // [0-4]=cells, [5]=diff, [6]=pack, [7]=t_cell, [8]=t_mosfet
  bool valid;            // Data successfully read
  bool is_bl36;          // 40V battery (10 cells)
  uint8_t cell_count;    // 5 or 10
};
extern BatteryData g_battery;

// Helper functions for HEX output
inline void printHex(byte b) {
  if (b < 0x10) Serial.print('0');
  Serial.print(b, HEX);
}

inline void printHexArray(const byte* arr, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    printHex(arr[i]);
  }
}

// External OneWire instance (defined in makita_comm.cpp)
#include <OneWire2.h>
extern OneWire makita;

#endif

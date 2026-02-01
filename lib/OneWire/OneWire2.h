#ifndef OneWire_h
#define OneWire_h

#ifdef __cplusplus

#include <stdint.h>

#if defined(__AVR__)
#include <util/crc16.h>
#endif

#if ARDUINO >= 100
#include <Arduino.h>
#else
#include "WProgram.h"
#include "pins_arduino.h"
#endif

#ifndef ONEWIRE_SEARCH
#define ONEWIRE_SEARCH 1
#endif

#ifndef ONEWIRE_CRC
#define ONEWIRE_CRC 1
#endif

#ifndef ONEWIRE_CRC8_TABLE
#define ONEWIRE_CRC8_TABLE 1
#endif

#ifndef ONEWIRE_CRC16
#define ONEWIRE_CRC16 1
#endif

#include "util/OneWire_direct_regtype.h"

class OneWire
{
  private:
    IO_REG_TYPE bitmask;
    volatile IO_REG_TYPE *baseReg;

#if ONEWIRE_SEARCH
    unsigned char ROM_NO[8];
    uint8_t LastDiscrepancy;
    uint8_t LastFamilyDiscrepancy;
    bool LastDeviceFlag;
#endif

  public:
    OneWire() { }
    OneWire(uint8_t pin) { begin(pin); }
    void begin(uint8_t pin);

    uint8_t reset(void);
    void select(const uint8_t rom[8]);
    void skip(void);
    void write(uint8_t v, uint8_t power = 0);
    void write_bytes(const uint8_t *buf, uint16_t count, bool power = 0);
    uint8_t read(void);
    void read_bytes(uint8_t *buf, uint16_t count);
    void write_bit(uint8_t v);
    uint8_t read_bit(void);
    void depower(void);

#if ONEWIRE_SEARCH
    void reset_search();
    void target_search(uint8_t family_code);
    bool search(uint8_t *newAddr, bool search_mode = true);
#endif

#if ONEWIRE_CRC
    static uint8_t crc8(const uint8_t *addr, uint8_t len);

#if ONEWIRE_CRC16
    static bool check_crc16(const uint8_t* input, uint16_t len, const uint8_t* inverted_crc, uint16_t crc = 0);
    static uint16_t crc16(const uint8_t* input, uint16_t len, uint16_t crc = 0);
#endif
#endif
};

#ifdef IO_REG_TYPE
#undef IO_REG_TYPE
#endif

#endif // __cplusplus
#endif // OneWire_h

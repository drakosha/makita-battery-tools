# Makita Battery Diagnostic Tool

Arduino Nano firmware for diagnosing, reading, and resetting Makita LXT lithium-ion batteries via a custom OneWire-like protocol.

## Overview

This tool allows you to:
- **Read battery information**: model, capacity, charge cycles, health status
- **Monitor cell voltages**: individual cell voltages with SOC (State of Charge) calculation
- **Check temperatures**: cell and MOSFET temperature sensors
- **Unlock locked batteries**: reset error codes and clear lock flags in EEPROM
- **Clone battery configuration**: copy MSG data from a working battery to a faulty one
- **Diagnose charger issues**: test DC18RC charger handshake protocol
- **Control LEDs**: turn battery indicator LEDs on/off for testing

## Credits

Based on research and code from:
- [open-battery-information](https://github.com/mnh-jansson/open-battery-information) by Martin Jansson
- [m5din-makita](https://github.com/no-body-in-particular/m5din-makita) by no-body-in-particular
- [makita-lxt-protocol](https://codeberg.org/rosvall/makita-lxt-protocol) by rosvall

## Hardware Requirements

### Components
- Arduino Nano (ATmega328P)
- 4.7kΩ resistor (pullup for data line)
- Wires for connection

### Wiring Diagram

```
Arduino Nano              Makita Battery
============              ==============
Pin D6 (Data)  ─────────── Data
     │
     ├── 4.7kΩ ── 5V       (pullup resistor)

Pin D8 (Enable) ────────── Enable
GND ────────────────────── GND
```

**Important**: The data line REQUIRES a 4.7kΩ pullup resistor to 5V for reliable communication.

### Battery Connector Pinout

Looking at the battery from the front (contacts facing you):

```
┌─────────────────────────┐
│  ○   ○   ○   ○   ○   ○  │
│  1   2   3   4   5   6  │
└─────────────────────────┘

Pin 1: V+ (Battery positive)
Pin 2: Data
Pin 3: Enable
Pin 4: GND
Pin 5: GND (or NTC on some models)
Pin 6: V+ (Battery positive)
```

## Installation

### Option 1: PlatformIO (Recommended)

```bash
# Clone or download this project
cd MakitaBatterySerial

# Build firmware
pio run

# Upload to Arduino Nano
pio run --target upload

# Open serial monitor (9600 baud)
pio device monitor
```

### Option 2: Arduino IDE

1. Open `arduino/MakitaBatteryDiagnostic/MakitaBatteryDiagnostic.ino` in Arduino IDE
2. Select **Tools → Board → Arduino Nano**
3. Select **Tools → Processor → ATmega328P** (or "Old Bootloader" if upload fails)
4. Select your COM port in **Tools → Port**
5. Click **Upload**
6. Open **Tools → Serial Monitor** at 9600 baud

The Arduino IDE version is a single-file sketch with all code combined, including the modified OneWire library with Makita-specific timings.

### Option 3: Pre-compiled Firmware (No Build Required)

If you don't want to compile the code yourself, use the pre-compiled `.hex` file from the `firmware/` folder:

**Using Arduino IDE (avrdude):**

1. Find your Arduino IDE's avrdude location:
   - Windows: `C:\Program Files (x86)\Arduino\hardware\tools\avr\bin\avrdude.exe`
   - macOS: `/Applications/Arduino.app/Contents/Java/hardware/tools/avr/bin/avrdude`
   - Linux: `/usr/share/arduino/hardware/tools/avr/bin/avrdude`

2. Find avrdude.conf in the same folder

3. Upload via command line (replace COM3 with your port):

```bash
# Windows
avrdude -C "C:\Program Files (x86)\Arduino\hardware\tools\avr\etc\avrdude.conf" -v -p atmega328p -c arduino -P COM3 -b 115200 -D -U flash:w:firmware\makita_battery_nano328.hex:i

# Linux/macOS
avrdude -C /usr/share/arduino/hardware/tools/avr/etc/avrdude.conf -v -p atmega328p -c arduino -P /dev/ttyUSB0 -b 115200 -D -U flash:w:firmware/makita_battery_nano328.hex:i
```

**Using avrdude directly (if installed separately):**

```bash
avrdude -p atmega328p -c arduino -P COM3 -b 115200 -D -U flash:w:firmware/makita_battery_nano328.hex:i
```

**Note for old bootloader:** If upload fails, try `-b 57600` instead of `-b 115200`.

## Usage

### Serial Menu Commands

Connect to the Arduino via serial terminal at **9600 baud**. The following commands are available:

| Key | Command | Description |
|-----|---------|-------------|
| `1` | Read battery data | Display full battery information |
| `2` | Reset errors (quick) | Reset BMS errors without EEPROM write |
| `3` | Unlock battery | Full unlock procedure with EEPROM write |
| `4` | LED ON | Turn on battery indicator LEDs |
| `5` | LED OFF | Turn off battery indicator LEDs |
| `6` | Debug dump | Show raw data and MSG analysis |
| `7` | Check lock status | Quick lock status check |
| `s` | Save MSG | Save current MSG to memory for comparison |
| `d` | Compare MSG | Show changes between saved and current MSG |
| `v` | Clone MSG | Write saved MSG to current battery |
| `a` | Advanced reset | Submenu with advanced options |
| `h` | Help | Show menu |

### Advanced Reset Menu (Option `a`)

| Key | Command | Description |
|-----|---------|-------------|
| `1` | Factory reset | Reset with magic byte patterns |
| `2` | Reset handshake | Clear stuck charger handshake state |
| `3` | Set cycle count | Manually set charge cycle counter |
| `4` | Lock battery (test) | Intentionally lock for testing |

## Understanding Battery Data

### Battery Information Output

```
========================================
   MAKITA BATTERY DIAGNOSTIC TOOL
========================================

Model: BL1850B
Type: 18V Li-ion
Capacity: 5000 mAh
Cycles: 127
Health: 92%

Cell Voltages:
  Cell 1: 4.12V (95%)
  Cell 2: 4.11V (94%)
  Cell 3: 4.13V (96%)
  Cell 4: 4.10V (93%)
  Cell 5: 4.11V (94%)
  Pack:   20.57V
  Diff:   0.03V (OK)

Temperatures:
  Cell:   24.5°C
  MOSFET: 26.2°C

Status: OK (Unlocked)
```

### SOC (State of Charge) Table

| Cell Voltage | SOC | Pack Voltage (5S) |
|--------------|-----|-------------------|
| 4.20V | 100% | 21.0V |
| 4.00V | 80% | 20.0V |
| 3.80V | 60% | 19.0V |
| 3.70V | 50% | 18.5V |
| 3.50V | 30% | 17.5V |
| 3.00V | 0% | 15.0V |

### Error Codes

| Code | Meaning | Solution |
|------|---------|----------|
| 0x0 | OK | Battery is healthy |
| 0x1 | Overloaded | Reset with option 3 |
| 0x5 | Warning | Check cells, reset with option 3 |
| 0xF | Dead | May need cell replacement |

### Lock Status

The battery can be locked by:
- **Error flag** in MSG byte 20 (lower nibble)
- **Lock flag** in MSG byte 20 (bits indicating locked state)
- **Checksum mismatch** - BMS detects data corruption
- **Overdischarge** flag in MSG byte 24
- **Overload** flag in MSG byte 25

## Reset Options Explained

### Option 2: Quick Reset (No EEPROM)

- Sends testmode + reset_error commands multiple times
- Does NOT modify EEPROM
- Use when BMS is temporarily stuck but EEPROM is OK
- Fast and safe for first attempt

### Option 3: Full Unlock (EEPROM Write)

Three-phase aggressive unlock:
1. **Phase 1**: Standard reset commands (5 cycles)
2. **Phase 2**: EEPROM write with checksum recalculation
3. **Phase 3**: Extended power cycling with repeated resets

Clears:
- Error code (nybble 40)
- Lock flags
- Overdischarge counter
- Overload flags
- Recalculates all MSG checksums

### Advanced: Factory Reset

Applies known-good "magic" byte patterns used in factory programming. Options:
- Minimal reset (just clear error)
- 0xC1 pattern (used in some battery types)
- 0x94 pattern (used in other battery types)

### Advanced: Reset Handshake

For batteries that unlock but still won't charge on Makita DC18RC:
1. Extended power cycle
2. Repeated testmode/reset sequence
3. EEPROM clear with checksum fix
4. Final power cycle

## Donor Battery Cloning

If you have a working battery of the same model, you can clone its configuration:

1. Connect **working** battery
2. Press `s` to save MSG
3. Disconnect working battery
4. Connect **faulty** battery
5. Press `v` to clone MSG
6. Confirm with `y`

The tool will:
- Copy the saved MSG to the faulty battery
- Clear error codes
- Recalculate checksums
- Write to EEPROM

**Warning**: Only clone between batteries of the SAME model!

## Troubleshooting

### Battery Unlocked But Won't Charge on Makita DC18RC

The DC18RC performs more checks than cheap chargers:

| Check | DC18RC | Cheap Charger |
|-------|--------|---------------|
| MSG (error/lock) | ✓ | ✓ |
| Temperature sensors | ✓ | ✗ |
| BMS response validity | ✓ | ✗ |
| Cell voltages via BMS | ✓ | ✗ |
| SMBus handshake | ✓ | ✗ |

**Red-green blinking** = Cannot complete handshake with BMS

Diagnostic steps:
1. Use option `6` (Debug dump) to see raw data
2. Use option `s` to save MSG before charging attempt
3. Try Makita charger
4. Use option `d` to compare MSG and see what changed

**If charging still fails**:
1. Check cell contacts under PCB (may look OK but be disconnected)
2. Check BMS fuse
3. Check solder joints near connector
4. Try option `a` → `2` (Reset handshake)

### No Communication with Battery

- Verify 4.7kΩ pullup resistor is connected
- Check wiring connections
- Ensure Enable pin is connected
- Try different battery (to rule out completely dead BMS)

### Incorrect Readings

- Check for loose connections
- Verify correct pin assignment
- Some battery types (F0513 chips) have limited support

## Technical Details

### Makita OneWire Protocol

Makita uses a modified OneWire-like protocol with non-standard timings:

| Parameter | Standard 1-Wire | Makita |
|-----------|-----------------|--------|
| Reset pulse | 480 µs | 750 µs |
| Write 1 | 6/64 µs | 12/120 µs |
| Write 0 | 60/10 µs | 100/30 µs |
| Read | 6/9 µs | 10/10 µs |

### Command Reference

| Command | Parameters | Description |
|---------|------------|-------------|
| `0x33` | - | Read ROM (returns 8 bytes) |
| `0xCC` | - | Skip ROM (address all devices) |
| `0xD7` | offset, 0x00, count | Read data block |
| `0xF0` | 0x00 | Charger command (returns ROM + MSG) |
| `0xD9` | 0x96, 0xA5 | Enter test mode |
| `0xDA` | 0x04 | Reset error |
| `0xDA` | 0x31 | LEDs ON |
| `0xDA` | 0x34 | LEDs OFF |
| `0x0F` | 0x00 + 32 bytes | Write MSG to buffer |
| `0x55` | 0xA5 | Commit buffer to EEPROM |

### MSG Structure (32 bytes)

| Byte | Nybbles | Description |
|------|---------|-------------|
| 11 | 22-23 | Battery type code |
| 16 | 32-33 | Capacity code |
| 20 | 40-41 | Error (low nybble), Checksum1 (high nybble) |
| 21 | 42-43 | Checksum2 (low nybble), Checksum3 (high nybble) |
| 24 | 48-49 | Overdischarge counter |
| 25 | 50-51 | Overload flags |
| 26-27 | 52-55 | Cycle count (13-bit) |

### Checksum Calculation

MSG uses three checksums covering different byte ranges:
- Checksum1 (nybble 41): Covers nybbles 0-40
- Checksum2 (nybble 42): Covers nybbles 0-41
- Checksum3 (nybble 43): Covers nybbles 0-42

Formula: `min(sum_of_nybbles, 0xFF) & 0x0F`

## Supported Batteries

### 18V LXT Series
- BL1815, BL1815N
- BL1820, BL1820B
- BL1830, BL1830B
- BL1840, BL1840B
- BL1850, BL1850B
- BL1860, BL1860B

### 14.4V Series
- BL1415, BL1415N
- BL1430, BL1430B
- BL1440
- BL1450

### 40V XGT Series (Limited Support)
- BL4020, BL4025
- BL4040, BL4050

**Note**: 40V batteries use 10 cells and may have protocol differences.

### Chip Variants

| Chip | Features | Notes |
|------|----------|-------|
| Standard | Full support | 2 temperature sensors |
| F0513 | Limited | 1 temp sensor, no error reset via commands |

## Project Structure

```
MakitaBatterySerial/
├── platformio.ini          # PlatformIO configuration
├── README.md               # This file (English)
├── README_RU.md            # Russian documentation
├── CLAUDE.md               # AI assistant context
├── src/                    # PlatformIO source files
│   ├── main.cpp            # Main program and serial menu
│   ├── config.h            # Pin definitions and shared data
│   ├── makita_comm.h/cpp   # Low-level communication
│   ├── makita_commands.h/cpp # Protocol commands
│   ├── makita_data.h/cpp   # Data parsing and calculations
│   ├── makita_print.h/cpp  # Output formatting
│   └── makita_unlock.h/cpp # Reset and unlock functions
├── lib/
│   └── OneWire/            # Modified OneWire library with Makita timings
├── firmware/
│   └── makita_battery_nano328.hex  # Pre-compiled firmware
└── arduino/
    └── MakitaBatteryDiagnostic/
        └── MakitaBatteryDiagnostic.ino  # Single-file Arduino IDE version
```

## Memory Usage

- Flash: ~25KB (82%) - Room for LCD expansion
- RAM: ~500 bytes (24%)

## Future Plans

- [ ] LCD display support (16x2 or OLED)
- [ ] Standalone operation (battery powered)
- [ ] Multiple battery comparison mode
- [ ] Extended 40V battery support

## Safety Warnings

- **Do not** short-circuit battery terminals
- **Do not** attempt to charge cells with reversed polarity
- **Do not** use damaged or swollen batteries
- **Always** verify connections before powering on
- Lithium-ion batteries can be dangerous if mishandled
- This tool modifies battery EEPROM - use at your own risk

## License

MIT License

Copyright (c) 2024

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

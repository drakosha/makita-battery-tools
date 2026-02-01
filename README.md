# Makita Battery Diagnostic Tool

Arduino Nano tool for reading and resetting Makita LXT batteries via custom OneWire protocol.

Based on:
- [open-battery-information](https://github.com/mnh-jansson/open-battery-information)
- [m5din-makita](https://github.com/no-body-in-particular/m5din-makita)
- [makita-lxt-protocol](https://codeberg.org/rosvall/makita-lxt-protocol)

## Features

- Read battery info: model, capacity, charge cycles, health
- Cell voltages with SOC (State of Charge) calculation
- Temperature monitoring (cell + MOSFET)
- **Unlock locked batteries** (write to EEPROM)
- LED control
- Charger protocol diagnostics (DC18RC simulation)
- Donor MSG cloning (copy working battery config to broken one)

## Hardware

```
Arduino Nano          Makita Battery
-----------          --------------
Pin D6  ------------- Data (with 4.7k pullup to 5V)
Pin D8  ------------- Enable
GND     ------------- GND
```

**Important:** Data line requires 4.7k pullup resistor to 5V!

## Build & Upload

```bash
# Using PlatformIO
pio run                    # Build
pio run --target upload    # Upload
pio device monitor         # Serial monitor (9600 baud)
```

## Serial Menu (9600 baud)

```
  1 - Read battery data      # Full info
  2 - Reset errors (quick)   # Without EEPROM write
  3 - Unlock battery         # Full reset with EEPROM
  4 - LED ON
  5 - LED OFF
  6 - Debug dump             # Raw data + MSG analysis
  7 - Check lock status
  9 - Save MSG               # For comparison
  0 - Compare MSG            # Show changes after charging
  ----------------------------------------
  c - Save DONOR MSG         # From WORKING battery
  v - Clone donor MSG        # To BROKEN battery
  t - Test charger handshake # DC18RC protocol diagnostics
  a - Advanced reset         # Submenu: factory reset, handshake reset
  h - Show menu
```

## Reset Options

| Option | Name | What it does |
|--------|------|--------------|
| 2 | Reset errors (quick) | testmode + reset_error x3, **no EEPROM write** |
| 3 | Unlock battery | Full reset: 3 phases + EEPROM (error/lock/overdischarge/overload) |
| a | Advanced reset | Submenu: 1=factory reset, 2=reset handshake |

**When to use:**
- **Option 2** - for temporary locks when BMS is stuck but EEPROM is OK
- **Option 3** - for full unlock of locked battery
- **Option a** - rare cases: magic values (factory), stuck handshake

## Makita OneWire Protocol

Non-standard timings (differs from standard 1-Wire):
- Reset: 750 us
- Write 1: 12/120 us
- Write 0: 100/30 us
- Read: 10/10 us

### Key Commands

| Command | Description |
|---------|-------------|
| `0x33` | Read ROM (8 bytes) |
| `0xCC` | Skip ROM |
| `0xD7 0x00 0x00 0xFF` | Voltage data (29 bytes) |
| `0xD7 0x0E 0x00 0x02` | Cell temperature (Kelvin/10) |
| `0xD7 0x10 0x00 0x02` | MOSFET temperature (Kelvin/10) |
| `0xF0 0x00` | Charger command (ROM + MSG) |
| `0xD9 0x96 0xA5` | Enter test mode |
| `0xDA 0x04` | Reset error |
| `0xDA 0x31` | LEDs ON |
| `0xDA 0x34` | LEDs OFF |
| `0x0F 0x00` + 32 bytes | Write MSG to buffer |
| `0x55 0xA5` | Commit to EEPROM |

### MSG Lock Bytes

- **Byte 19**: Error nibble (lower 4 bits = error code)
- **Byte 20**: Lock nibble (lower 4 bits != 0 = LOCKED)
- **Byte 24**: Overdischarge (inverted)
- **Byte 25**: Overload flags

### Unlock Procedure (store_cmd_direct)

```
1. Reset + Read ROM (0x33)
2. Read 8 bytes ROM ID
3. Send 0x0F 0x00 + 32 bytes MSG with cleared nibbles:
   - msg[19] &= 0xF0 (clear error)
   - msg[20] &= 0xF0 (clear lock)
   - msg[24] = 0xFF (clear overdischarge)
   - msg[25] &= 0x0F (clear overload)
4. Reset + Read ROM (0x33)
5. Read 8 bytes ROM ID
6. Send 0x55 0xA5 (commit to EEPROM)
7. Power cycle
```

## Troubleshooting

### Battery unlocked but won't charge on Makita DC18RC

DC18RC checks more than cheap chargers:

| Check | DC18RC | Cheap charger |
|-------|--------|---------------|
| MSG (error/lock) | Yes | Yes |
| Temperature sensors | Yes | No |
| BMS response validity | Yes | No |
| Cell voltages via BMS | Yes | No |
| SMBus handshake | Yes | No |

If red-green blinking = "cannot handshake with BMS"

**Diagnostics:**
1. Use option `6` - Debug dump (raw data + MSG)
2. Use option `9` - Save MSG before charging attempt
3. Try Makita charger
4. Use option `0` - Compare MSG (see what changed)

**If charging still fails:**
1. Check cell contacts under PCB (may look OK but be disconnected)
2. Check BMS fuse
3. Check solder joints near connector

### Battery Types

| Type | Features |
|------|----------|
| BL18xx | Full support, 2 temperatures |
| F0513 | Limited, 1 temp, no error reset |
| BL36 (40V) | 10 cells, different protocol |

## SOC Table (Li-ion)

| Voltage | SOC |
|---------|-----|
| 4.20V | 100% |
| 4.00V | 80% |
| 3.80V | 60% |
| 3.70V | 50% |
| 3.50V | 30% |
| 3.00V | 0% |

For 5S pack: 21.0V = 100%, 18.5V ~ 50%, 15.0V = 0%

## Supported Batteries

- 18V: BL1815, BL1820, BL1830, BL1840, BL1850, BL1860
- 14.4V: BL1415, BL1430, BL1440, BL1450
- 40V: BL3626, BL4020, BL4025, BL4040, BL4050

## Project Structure

```
MakitaBatterySerial/
├── platformio.ini       # PlatformIO config
├── src/main.cpp         # Main code
├── lib/OneWire/         # Modified OneWire with Makita timings
├── makita_lxt.py        # Python reference (protocol commands)
└── references/          # Original projects for reference
```

## Flash Usage

25214 bytes (82.1%) - ~5.5KB free for LCD expansion

## License

MIT License

# Makita Battery Reader - AI Context

## Project

Arduino Nano tool for Makita battery diagnostics. Full docs in [README.md](README.md).

## Key Files

- `src/main.cpp` - main firmware
- `lib/OneWire/OneWire2.cpp` - modified timings for Makita

## Build (Windows)

```bash
"C:\Users\mike\.platformio\penv\Scripts\pio.exe" run                    # build
"C:\Users\mike\.platformio\penv\Scripts\pio.exe" run --target upload    # upload
"C:\Users\mike\.platformio\penv\Scripts\pio.exe" device monitor         # monitor 9600
```

## Hardware

- **D6** = Data (+ 4.7k pullup)
- **D8** = Enable
- **GND** = Battery GND

## Protocol Quick Reference

```
0x33           - Read ROM (8 bytes)
0xCC           - Skip ROM
0xD7 + params  - Read data (voltages, temps)
0xF0 0x00      - Charger cmd (ROM + MSG)
0xD9 0x96 0xA5 - Testmode
0xDA 0x04      - Reset error
0x0F 0x00 + 32 - Write MSG
0x55 0xA5      - Commit EEPROM
```

## Protocol Documentation

**ВАЖНО:** Используй документацию из `references/makita-lxt-protocol/` для работы с протоколом!

Ключевые файлы:
- `references/makita-lxt-protocol/basic_info_cmd.md` - формат MSG (32 байта)
- `references/makita-lxt-protocol/type5.md` - F0513 чипы

## MSG Format (per protocol docs)

Формат: **nybble-ориентированный**, младший nybble первый.

| Поле | Nybbles | Byte | Формула |
|------|---------|------|---------|
| Battery type | 22-23 | 11 | `SWAP_NIBBLES(msg[11])` |
| Capacity | 32-33 | 16 | `SWAP_NIBBLES(msg[16])` |
| **Error code** | **40** | **20 low** | `msg[20] & 0x0F` |
| Checksums | 41-43 | 20-21 | Проверка lock |
| Overdischarge | 48-49 | 24 | `-5 * SWAP(msg[24]) + 160` |
| Overload | 50-51 | 25 | `5 * SWAP(msg[25]) - 160` |
| Cycle count | 52-55 | 26-27 | 13-bit, complex |

Error codes: 0=OK, 1=Overloaded, 5=Warning, 15=Dead

## Important Bug: 0x33 → 0xCC Switch

After `cmd_and_read_33`, first `cmd_and_read_cc` may fail. Add warm-up read:
```cpp
makita.reset();
delay(100);
cell_temperature();  // discard
delay(50);
float t = cell_temperature();  // real
```

## Memory Usage

- Flash: 21518 bytes (70.0%)
- RAM: 480 bytes (23.4%)

## Next Steps

1. LCD display
2. Standalone device (battery powered)

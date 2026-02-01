# Pre-compiled Firmware

This folder contains pre-compiled firmware for users who don't want to set up a build environment.

## Files

| File | Board | Description |
|------|-------|-------------|
| `makita_battery_nano328.hex` | Arduino Nano (ATmega328P) | Main firmware |

## Quick Upload (Windows)

Replace `COM3` with your Arduino's COM port:

```batch
:: Using Arduino IDE's avrdude
"C:\Program Files (x86)\Arduino\hardware\tools\avr\bin\avrdude.exe" ^
  -C "C:\Program Files (x86)\Arduino\hardware\tools\avr\etc\avrdude.conf" ^
  -v -p atmega328p -c arduino -P COM3 -b 115200 -D ^
  -U flash:w:makita_battery_nano328.hex:i
```

If upload fails with "not in sync" error, try `-b 57600` (old bootloader).

## Quick Upload (Linux/macOS)

```bash
avrdude -p atmega328p -c arduino -P /dev/ttyUSB0 -b 115200 -D \
  -U flash:w:makita_battery_nano328.hex:i
```

## After Upload

1. Open serial terminal at **9600 baud**
2. Press `h` for help menu
3. Connect battery and press `1` to read data

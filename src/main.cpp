/*
 * Makita Battery Information for Arduino Nano
 * Based on:
 *   - open-battery-information by Martin Jansson
 *   - m5din-makita by no-body-in-particular
 *
 * Wiring:
 *   - Pin 6 (ONEWIRE_PIN): Data line to battery (with 4.7k pullup to 5V)
 *   - Pin 8 (ENABLE_PIN): Enable/power control
 *   - GND: Battery ground
 */

#include "config.h"
#include "makita_comm.h"
#include "makita_commands.h"
#include "makita_data.h"
#include "makita_print.h"
#include "makita_unlock.h"

// ============== High-level functions ==============

void readAndPrintAll() {
  // Read all data first with warm-up
  if (!readAllBatteryData()) {
    Serial.println(F("ERROR: Failed to read battery data"));
    Serial.println(F("Check connection and try again."));
    printMenu();
    return;
  }

  printHeader();
  Serial.println();
  printModel();
  Serial.println();
  printBatteryInfo();
  Serial.println();
  printVoltages();
  Serial.println();
  printDiagnosis();
  Serial.println();
  printMenu();
}

// ============== Setup ==============

void setup() {
  Serial.begin(9600);

  pinMode(ONEWIRE_PIN, INPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, HIGH);

  delay(1000);

  Serial.println();
  printSeparator();
  Serial.println(F("   MAKITA BATTERY DIAGNOSTIC TOOL"));
  printSeparator();
  Serial.println(F("Ready. Connect battery and select option."));

  printMenu();
}

// ============== Main loop ==============

void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();

    // Clear remaining characters
    while (Serial.available() > 0) Serial.read();

    switch (cmd) {
      case '1':
      case 'r':
      case 'R':
        Serial.println(F("\nReading battery data..."));
        readAndPrintAll();
        break;

      case '2':
      case 'e':
      case 'E':
        resetBatteryErrors();
        printMenu();
        break;

      case '3':
      case 'u':
      case 'U':
        unlockBattery();
        printMenu();
        break;

      case '4':
        Serial.println(F("\nTurning LEDs ON..."));
        if (is_f0513()) {
          Serial.println(F("ERROR: F0513 chip - LED control not supported"));
        } else {
          testmode_cmd();
          delay(100);
          makita.reset();
          delay(50);
          leds_on_cmd();
          Serial.println(F("Done."));
        }
        printMenu();
        break;

      case '5':
        Serial.println(F("\nTurning LEDs OFF..."));
        if (is_f0513()) {
          Serial.println(F("ERROR: F0513 chip - LED control not supported"));
        } else {
          testmode_cmd();
          delay(100);
          makita.reset();
          delay(50);
          leds_off_cmd();
          Serial.println(F("Done."));
        }
        printMenu();
        break;

      case '6':
        Serial.println(F("\nReading raw data..."));
        // Read fresh data if not cached
        if (!g_battery.valid) {
          readAllBatteryData();
        }
        printRawData();
        printMenu();
        break;

      case '7':
        Serial.println(F("\nChecking lock status..."));
        Serial.println(isBatteryLocked() ? F("Status: LOCKED") : F("Status: UNLOCKED (OK)"));
        printMenu();
        break;

      case 's':
      case 'S':
        saveMSG();
        printMenu();
        break;

      case 'd':
      case 'D':
        compareMSG();
        printMenu();
        break;

      case 'v':
      case 'V':
        cloneMSG();
        printMenu();
        break;

      case 'a':
      case 'A':
        advancedResetMenu();
        printMenu();
        break;

      case 'h':
      case 'H':
      case '?':
        printMenu();
        break;

      case '\n':
      case '\r':
        break;

      default:
        Serial.println(F("Unknown command. Press 'h' for menu."));
        break;
    }
  }
}

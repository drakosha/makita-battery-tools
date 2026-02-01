/*
 * Makita Battery Diagnostic Tool for Arduino Nano
 *
 * Single-file version for Arduino IDE
 *
 * Based on:
 *   - open-battery-information by Martin Jansson
 *   - m5din-makita by no-body-in-particular
 *   - makita-lxt-protocol by rosvall
 *
 * Wiring:
 *   - Pin 6 (ONEWIRE_PIN): Data line to battery (with 4.7k pullup to 5V)
 *   - Pin 8 (ENABLE_PIN): Enable/power control
 *   - GND: Battery ground
 */

// ============================================================================
// CONFIGURATION
// ============================================================================

#define ONEWIRE_PIN 6
#define ENABLE_PIN 8
#define SWAP_NIBBLES(x) ((x & 0x0F) << 4 | (x & 0xF0) >> 4)
#define SHARED_BUF_SIZE 64

// ============================================================================
// MODIFIED ONEWIRE LIBRARY (Makita timings)
// ============================================================================

// Platform specific I/O definitions for AVR (Arduino Nano)
#if defined(__AVR__)
#define PIN_TO_BASEREG(pin)             (portInputRegister(digitalPinToPort(pin)))
#define PIN_TO_BITMASK(pin)             (digitalPinToBitMask(pin))
#define IO_REG_TYPE uint8_t
#define IO_REG_BASE_ATTR asm("r30")
#define IO_REG_MASK_ATTR
#define DIRECT_READ(base, mask)         (((*(base)) & (mask)) ? 1 : 0)
#define DIRECT_MODE_INPUT(base, mask)   ((*((base)+1)) &= ~(mask))
#define DIRECT_MODE_OUTPUT(base, mask)  ((*((base)+1)) |= (mask))
#define DIRECT_WRITE_LOW(base, mask)    ((*((base)+2)) &= ~(mask))
#define DIRECT_WRITE_HIGH(base, mask)   ((*((base)+2)) |= (mask))
#else
// Fallback for other platforms
#define PIN_TO_BASEREG(pin)             (0)
#define PIN_TO_BITMASK(pin)             (pin)
#define IO_REG_TYPE unsigned int
#define IO_REG_BASE_ATTR
#define IO_REG_MASK_ATTR
#define DIRECT_READ(base, pin)          digitalRead(pin)
#define DIRECT_WRITE_LOW(base, pin)     digitalWrite(pin, LOW)
#define DIRECT_WRITE_HIGH(base, pin)    digitalWrite(pin, HIGH)
#define DIRECT_MODE_INPUT(base, pin)    pinMode(pin,INPUT)
#define DIRECT_MODE_OUTPUT(base, pin)   pinMode(pin,OUTPUT)
#endif

class OneWire {
private:
  IO_REG_TYPE bitmask;
  volatile IO_REG_TYPE *baseReg;

public:
  OneWire() { }
  OneWire(uint8_t pin) { begin(pin); }

  void begin(uint8_t pin) {
    pinMode(pin, INPUT);
    bitmask = PIN_TO_BITMASK(pin);
    baseReg = PIN_TO_BASEREG(pin);
  }

  uint8_t reset(void) {
    IO_REG_TYPE mask IO_REG_MASK_ATTR = bitmask;
    volatile IO_REG_TYPE *reg IO_REG_BASE_ATTR = baseReg;
    uint8_t r;
    uint8_t retries = 125;

    noInterrupts();
    DIRECT_MODE_INPUT(reg, mask);
    interrupts();
    do {
      if (--retries == 0) return 0;
      delayMicroseconds(2);
    } while (!DIRECT_READ(reg, mask));

    noInterrupts();
    DIRECT_WRITE_LOW(reg, mask);
    DIRECT_MODE_OUTPUT(reg, mask);
    interrupts();
    delayMicroseconds(750);  // Makita timing (was 480)
    noInterrupts();
    DIRECT_MODE_INPUT(reg, mask);
    delayMicroseconds(70);
    r = !DIRECT_READ(reg, mask);
    interrupts();
    delayMicroseconds(410);
    return r;
  }

  void write_bit(uint8_t v) {
    IO_REG_TYPE mask IO_REG_MASK_ATTR = bitmask;
    volatile IO_REG_TYPE *reg IO_REG_BASE_ATTR = baseReg;

    if (v & 1) {
      noInterrupts();
      DIRECT_WRITE_LOW(reg, mask);
      DIRECT_MODE_OUTPUT(reg, mask);
      delayMicroseconds(12);  // Makita timing (was 10)
      DIRECT_WRITE_HIGH(reg, mask);
      interrupts();
      delayMicroseconds(120); // Makita timing (was 55)
    } else {
      noInterrupts();
      DIRECT_WRITE_LOW(reg, mask);
      DIRECT_MODE_OUTPUT(reg, mask);
      delayMicroseconds(100); // Makita timing (was 65)
      DIRECT_WRITE_HIGH(reg, mask);
      interrupts();
      delayMicroseconds(30);  // Makita timing (was 5)
    }
  }

  uint8_t read_bit(void) {
    IO_REG_TYPE mask IO_REG_MASK_ATTR = bitmask;
    volatile IO_REG_TYPE *reg IO_REG_BASE_ATTR = baseReg;
    uint8_t r;

    noInterrupts();
    DIRECT_MODE_OUTPUT(reg, mask);
    DIRECT_WRITE_LOW(reg, mask);
    delayMicroseconds(10);  // Makita timing (was 3)
    DIRECT_MODE_INPUT(reg, mask);
    delayMicroseconds(10);
    r = DIRECT_READ(reg, mask);
    interrupts();
    delayMicroseconds(53);
    return r;
  }

  void write(uint8_t v, uint8_t power = 0) {
    uint8_t bitMask;
    for (bitMask = 0x01; bitMask; bitMask <<= 1) {
      write_bit((bitMask & v) ? 1 : 0);
    }
    if (!power) {
      noInterrupts();
      DIRECT_MODE_INPUT(baseReg, bitmask);
      DIRECT_WRITE_LOW(baseReg, bitmask);
      interrupts();
    }
  }

  void write_bytes(const uint8_t *buf, uint16_t count, bool power = 0) {
    for (uint16_t i = 0; i < count; i++)
      write(buf[i]);
    if (!power) {
      noInterrupts();
      DIRECT_MODE_INPUT(baseReg, bitmask);
      DIRECT_WRITE_LOW(baseReg, bitmask);
      interrupts();
    }
  }

  uint8_t read() {
    uint8_t bitMask;
    uint8_t r = 0;
    for (bitMask = 0x01; bitMask; bitMask <<= 1) {
      if (read_bit()) r |= bitMask;
    }
    return r;
  }

  void read_bytes(uint8_t *buf, uint16_t count) {
    for (uint16_t i = 0; i < count; i++)
      buf[i] = read();
  }
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

OneWire makita(ONEWIRE_PIN);
byte g_buf[SHARED_BUF_SIZE];

struct BatteryData {
  byte rom[8];
  byte msg[32];
  float voltages[9];
  bool valid;
  bool is_bl36;
  uint8_t cell_count;
};
BatteryData g_battery;

// MSG storage for comparison
byte saved_msg[32];
bool msg_saved = false;

// ============================================================================
// LOW-LEVEL COMMUNICATION
// ============================================================================

void set_enablepin(bool high) {
  digitalWrite(ENABLE_PIN, high ? HIGH : LOW);
}

void trigger_power() {
  set_enablepin(false);
  delay(200);
  set_enablepin(true);
  delay(500);
}

bool cmd_and_read(uint8_t initial, uint8_t *cmd, uint8_t cmd_len, byte *rsp, uint8_t rsp_len) {
  int offset = (initial == 0x33 ? 8 : 0);
  memset(rsp, 0xff, rsp_len + offset);

  for (int i = 0; !makita.reset(); i++) {
    if (i == 5) {
      trigger_power();
      return false;
    }
    delay(500);
  }

  delayMicroseconds(310);

  if (offset) {
    makita.write(initial);
    makita.read_bytes(rsp, offset);
    makita.write_bytes(cmd, cmd_len);
  } else {
    makita.write(initial);
    makita.write_bytes(cmd, cmd_len);
  }

  makita.read_bytes(rsp + offset, rsp_len);

  if (rsp_len < 3 || !(rsp[offset] == 0xFF && rsp[1 + offset] == 0xFF && rsp[2 + offset] == 0xff)) {
    return true;
  } else {
    trigger_power();
    return false;
  }
}

bool cmd_and_read_33(uint8_t *cmd, uint8_t cmd_len, byte *rsp, uint8_t rsp_len) {
  return cmd_and_read(0x33, cmd, cmd_len, rsp, rsp_len);
}

bool cmd_and_read_cc(uint8_t *cmd, uint8_t cmd_len, byte *rsp, uint8_t rsp_len) {
  return cmd_and_read(0xcc, cmd, cmd_len, rsp, rsp_len);
}

void warmup_battery() {
  byte dummy[16];
  trigger_power();
  delay(200);

  for (int i = 0; i < 3; i++) {
    makita.reset();
    delay(100);
    byte cmd[] = { 0xD7, 0x0E, 0x00, 0x02 };
    cmd_and_read_cc(cmd, 4, dummy, 3);
    delay(50);
  }
  makita.reset();
  delay(100);
}

// ============================================================================
// PROTOCOL COMMANDS
// ============================================================================

void f0513_second_command_tree() {
  byte cmd[] = { 0x99 };
  byte rsp[16];
  memset(rsp, 0, 16);
  cmd_and_read_cc(cmd, 1, rsp, 0);
  makita.reset();
  delayMicroseconds(310);
}

void f0513_model_cmd(byte rsp[]) {
  f0513_second_command_tree();
  makita.write(0x31);
  delayMicroseconds(90);
  rsp[0] = makita.read();
  delayMicroseconds(90);
  rsp[1] = makita.read();
}

void f0513_vcell_cmd(byte cmd_byte, byte rsp[]) {
  byte cmd_params[] = { cmd_byte };
  cmd_and_read_cc(cmd_params, 1, rsp, 2);
}

void f0513_temp_cmd(byte rsp[]) {
  byte cmd_params[] = { 0x52 };
  cmd_and_read_cc(cmd_params, 1, rsp, 2);
}

bool is_f0513() {
  byte data[32];
  f0513_model_cmd(data);
  return !(data[0] == 0xFF && data[1] == 0xFF);
}

bool model_cmd(byte rsp[]) {
  byte cmd_params[] = { 0xDC, 0x0C };
  for (int i = 0; i < 10; i++) {
    if (cmd_and_read_cc(cmd_params, 2, rsp, 10)) return true;
  }
  return false;
}

void read_data_request(byte rsp[]) {
  byte cmd_params[] = { 0xD7, 0x00, 0x00, 0xFF };
  cmd_and_read_cc(cmd_params, 4, rsp, 29);
}

bool charger_33_cmd(byte rsp[]) {
  byte cmd_params[] = { 0xF0, 0x00 };
  return cmd_and_read_33(cmd_params, 2, rsp, 32);
}

bool try_charger(byte rsp[]) {
  for (int i = 0; i < 20; i++) {
    if (charger_33_cmd(rsp)) return true;
  }
  return false;
}

void testmode_cmd() {
  byte cmd_params[] = { 0xD9, 0x96, 0xA5 };
  cmd_and_read_33(cmd_params, 3, g_buf, 29);
}

void exit_testmode_cmd() {
  byte cmd_params[] = { 0xD9, 0xFF, 0xFF };
  cmd_and_read_33(cmd_params, 3, g_buf, 1);
}

void send_da_cmd(byte sub_cmd) {
  byte cmd_params[] = { 0xDA, sub_cmd };
  cmd_and_read_33(cmd_params, 2, g_buf, 9);
}

void reset_error_cmd() { send_da_cmd(0x04); }
void leds_on_cmd() { send_da_cmd(0x31); }
void leds_off_cmd() { send_da_cmd(0x34); }

void store_cmd_direct(byte data[]) {
  byte rsp[16];

  for (int i = 0; !makita.reset(); i++) {
    if (i == 5) return;
    delay(100);
  }
  delayMicroseconds(310);

  makita.write(0x33);
  makita.read_bytes(rsp, 8);

  makita.write(0x0F);
  makita.write(0x00);
  makita.write_bytes(data, 32);

  delay(500);

  for (int retry = 0; retry < 3; retry++) {
    for (int i = 0; !makita.reset(); i++) {
      if (i == 5) break;
      delay(100);
    }
    delayMicroseconds(310);

    makita.write(0x33);
    makita.read_bytes(rsp, 8);

    makita.write(0x55);
    makita.write(0xA5);

    delay(500);
  }
}

void write_msg_to_eeprom(byte* msg) {
  testmode_cmd();
  delay(100);
  charger_33_cmd(g_buf);
  delay(100);
  store_cmd_direct(msg);
  delay(500);
  exit_testmode_cmd();
  delay(200);
  trigger_power();
  delay(300);
}

// ============================================================================
// DATA FUNCTIONS
// ============================================================================

int round5(int in) {
  static signed char cAdd[5] = { 0, -1, -2, 2, 1 };
  return in + cAdd[in % 5];
}

uint8_t voltage_to_soc(float voltage) {
  if (voltage >= 4.20f) return 100;
  if (voltage <= 3.00f) return 0;
  return (uint8_t)((voltage - 3.0f) * 83.33f);
}

bool is_new_capacity_format(byte cap_byte) {
  byte swapped = SWAP_NIBBLES(cap_byte);
  return (swapped > 60 && cap_byte <= 8 && cap_byte > 0);
}

uint32_t get_capacity_mah(byte cap_byte) {
  if (is_new_capacity_format(cap_byte)) {
    return (uint32_t)cap_byte * 1000L;
  } else {
    return (uint32_t)SWAP_NIBBLES(cap_byte) * 100L;
  }
}

int get_capacity_for_model(byte cap_byte) {
  if (is_new_capacity_format(cap_byte)) {
    return cap_byte * 10;
  } else {
    return round5(SWAP_NIBBLES(cap_byte));
  }
}

bool has_health() {
  byte rsp[4];
  memset(rsp, 0, 4);
  byte cmd_params[] = { 0xD4, 0xBA, 0x00, 0x01 };
  cmd_and_read_cc(cmd_params, 4, rsp, 2);
  return rsp[1] == 0x06;
}

byte overload() {
  byte rsp[16];
  memset(rsp, 0, 16);
  byte cmd_params[] = { 0xD4, 0x8D, 0x00, 0x07 };
  cmd_and_read_cc(cmd_params, 4, rsp, 8);
  return (rsp[5] & 0xf0) >> 4 | (rsp[6] & 0x70);
}

byte overdischarge() {
  byte rsp[4];
  memset(rsp, 0, 4);
  byte cmd_params[] = { 0xD4, 0xBA, 0x00, 0x01 };
  cmd_and_read_cc(cmd_params, 4, rsp, 2);
  if (rsp[0] == 0xFF) return 0;
  int val = rsp[0] << 1;
  return (val > 100) ? 100 : val;
}

byte health() {
  byte rsp[4];
  memset(rsp, 0, 4);
  byte cmd_params[] = { 0xD4, 0x50, 0x01, 0x02 };
  cmd_and_read_cc(cmd_params, 4, rsp, 3);
  if (rsp[1] == 0xFF || rsp[1] < 10) return 100;
  int val = 14 * (rsp[1] - 10);
  return (val > 100) ? 100 : ((val < 0) ? 0 : val);
}

float cell_temperature() {
  byte rsp[4];
  memset(rsp, 0, 4);
  byte cmd_params[] = { 0xD7, 0x0E, 0x00, 0x02 };
  cmd_and_read_cc(cmd_params, 4, rsp, 3);
  if (rsp[0] == 0xFF && rsp[1] == 0xFF) return -999.0f;
  return (((rsp[0]) | ((int32_t)rsp[1]) << 8) / 10.0f) - 273.15f;
}

float mosfet_temperature() {
  byte rsp[4];
  memset(rsp, 0, 4);
  byte cmd_params[] = { 0xD7, 0x10, 0x00, 0x02 };
  cmd_and_read_cc(cmd_params, 4, rsp, 3);
  if (rsp[0] == 0xFF && rsp[1] == 0xFF) return -999.0f;
  return (((rsp[0]) | ((int32_t)rsp[1]) << 8) / 10.0f) - 273.15f;
}

bool get_voltage_info(float output[]) {
  bool f0513_mode = false;
  uint8_t data[128];
  memset(data, 0, 128);

  read_data_request(data);

  float t_cell = 0;
  float t_mosfet = 0;

  if (data[0] == 0xff && data[1] == 0xff) {
    memset(data, 0xff, 32);
    f0513_mode = true;
  } else {
    t_cell = cell_temperature();
    t_mosfet = mosfet_temperature();
  }

  if (f0513_mode) {
    f0513_vcell_cmd(0x31, data + 2);
    f0513_vcell_cmd(0x32, data + 4);
    f0513_vcell_cmd(0x33, data + 6);
    f0513_vcell_cmd(0x34, data + 8);
    f0513_vcell_cmd(0x35, data + 10);
    f0513_temp_cmd(data + 14);
    int temp_raw = ((int)data[14] | ((int)data[15]) << 8);
    float temp_100 = temp_raw / 100.0f;
    float temp_256 = temp_raw / 256.0f;
    t_cell = (temp_100 > 45.0f) ? temp_256 : temp_100;
    t_mosfet = 0;
  }

  if (data[2] == 0xff && data[3] == 0xff) return false;

  float v_cell1 = ((int)data[2] | ((int)data[3]) << 8) / 1000.0f;
  float v_cell2 = ((int)data[4] | ((int)data[5]) << 8) / 1000.0f;
  float v_cell3 = ((int)data[6] | ((int)data[7]) << 8) / 1000.0f;
  float v_cell4 = ((int)data[8] | ((int)data[9]) << 8) / 1000.0f;
  float v_cell5 = ((int)data[10] | ((int)data[11]) << 8) / 1000.0f;

  if (v_cell1 > 5.0f || v_cell2 > 5.0f || v_cell3 > 5.0f ||
      v_cell4 > 5.0f || v_cell5 > 5.0f) {
    v_cell1 /= 2.0f;
    v_cell2 /= 2.0f;
    v_cell3 /= 2.0f;
    v_cell4 /= 2.0f;
    v_cell5 /= 2.0f;
  }

  float max_v = v_cell1, min_v = v_cell1;
  if (v_cell2 > max_v) max_v = v_cell2;
  if (v_cell3 > max_v) max_v = v_cell3;
  if (v_cell4 > max_v) max_v = v_cell4;
  if (v_cell5 > max_v) max_v = v_cell5;
  if (v_cell2 < min_v) min_v = v_cell2;
  if (v_cell3 < min_v) min_v = v_cell3;
  if (v_cell4 < min_v) min_v = v_cell4;
  if (v_cell5 < min_v) min_v = v_cell5;

  output[0] = v_cell1;
  output[1] = v_cell2;
  output[2] = v_cell3;
  output[3] = v_cell4;
  output[4] = v_cell5;
  output[5] = max_v - min_v;
  output[6] = v_cell1 + v_cell2 + v_cell3 + v_cell4 + v_cell5;
  output[7] = t_cell;
  output[8] = t_mosfet;
  return true;
}

// ============================================================================
// CHECKSUM FUNCTIONS
// ============================================================================

byte calcNybbleSum(const byte* msg, int startByte, int endByte, bool lastLowOnly) {
  int sum = 0;
  int end = lastLowOnly ? endByte - 1 : endByte;
  for (int i = startByte; i <= end; i++) {
    sum += (msg[i] & 0x0F) + (msg[i] >> 4);
  }
  if (lastLowOnly) {
    sum += (msg[endByte] & 0x0F);
  }
  return (sum > 255 ? 255 : sum) & 0x0F;
}

bool verifyMsgChecksums(const byte* msg) {
  if (msg[20] == 0xFF && msg[21] == 0xFF) return false;

  byte chk1 = calcNybbleSum(msg, 0, 7, false);
  byte chk2 = calcNybbleSum(msg, 8, 15, false);
  byte chk3 = calcNybbleSum(msg, 16, 20, true);

  bool primary_ok = (chk1 == (msg[20] >> 4)) &&
                    (chk2 == (msg[21] & 0x0F)) &&
                    (chk3 == (msg[21] >> 4));

  byte chk4 = calcNybbleSum(msg, 22, 23, false);
  byte chk5 = calcNybbleSum(msg, 24, 30, false);

  bool secondary_ok = (chk4 == (msg[31] & 0x0F)) &&
                      (chk5 == (msg[31] >> 4));

  return primary_ok && secondary_ok;
}

void recalcMsgChecksums(byte* msg) {
  byte chk1 = calcNybbleSum(msg, 0, 7, false);
  byte chk2 = calcNybbleSum(msg, 8, 15, false);
  byte chk3 = calcNybbleSum(msg, 16, 20, true);

  msg[20] = (msg[20] & 0x0F) | (chk1 << 4);
  msg[21] = (chk2 & 0x0F) | (chk3 << 4);

  byte chk4 = calcNybbleSum(msg, 22, 23, false);
  byte chk5 = calcNybbleSum(msg, 24, 30, false);

  msg[31] = (chk4 & 0x0F) | (chk5 << 4);
}

bool isBatteryLocked() {
  byte data[64];
  memset(data, 0, 64);
  if (!try_charger(data)) return true;

  byte* msg = data + 8;
  byte err = msg[20] & 0x0F;

  if (err != 0 && err != 5) return true;
  return !verifyMsgChecksums(msg);
}

bool readAllBatteryData() {
  memset(&g_battery, 0, sizeof(g_battery));
  g_battery.valid = false;

  warmup_battery();

  byte charger_data[48];
  memset(charger_data, 0, 48);

  if (!try_charger(charger_data)) {
    return false;
  }

  memcpy(g_battery.rom, charger_data, 8);
  memcpy(g_battery.msg, charger_data + 8, 32);

  makita.reset();
  delay(100);
  cell_temperature();
  delay(50);
  cell_temperature();
  delay(50);

  if (get_voltage_info(g_battery.voltages)) {
    g_battery.cell_count = 5;
    g_battery.is_bl36 = false;
    g_battery.valid = true;
    return true;
  }

  g_battery.valid = true;
  g_battery.cell_count = 0;
  return true;
}

// ============================================================================
// PRINT FUNCTIONS
// ============================================================================

void printHex(byte b) {
  if (b < 0x10) Serial.print('0');
  Serial.print(b, HEX);
}

void printHexArray(const byte* arr, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    printHex(arr[i]);
  }
}

void printSeparator() {
  Serial.println(F("========================================"));
}

void printHeader() {
  Serial.println();
  printSeparator();
  Serial.println(F("       MAKITA BATTERY INFORMATION"));
  printSeparator();
}

void printModel() {
  byte data[64];
  char model[16];
  bool got_model = false;

  memset(data, 0, 64);
  memset(model, 0, 16);

  if (model_cmd(data)) {
    if (data[0] == 'B' && data[1] == 'L') {
      memcpy(model, data, 6);
      model[6] = '\0';
      got_model = true;
    }
  }

  if (!got_model) {
    f0513_model_cmd(data);
    if (!(data[0] == 0xFF && data[1] == 0xFF)) {
      sprintf(model, "BL%02X%02X", data[1], data[0]);
      got_model = true;
    }
  }

  if (!got_model && g_battery.valid) {
    int cap = get_capacity_for_model(g_battery.msg[16]);
    int type = SWAP_NIBBLES(g_battery.msg[11]);

    if (type == 14) {
      sprintf(model, "BL3626");
    } else if (SWAP_NIBBLES(g_battery.msg[25]) < 0xC) {
      sprintf(model, "BL14%02d", cap);
    } else {
      sprintf(model, "BL18%02d", cap);
    }
    got_model = true;
  }

  Serial.print(F("Model:           "));
  Serial.println(got_model ? model : "Unknown");
}

void printBatteryInfo() {
  if (!g_battery.valid) {
    Serial.println(F("ERROR: Cannot read battery info"));
    return;
  }

  const byte* rom = g_battery.rom;
  const byte* msg = g_battery.msg;

  Serial.print(F("ROM ID:          "));
  printHexArray(rom, 8);
  Serial.println();

  Serial.print(F("Mfg Date:        "));
  Serial.print(rom[2]);
  Serial.print('-');
  if (rom[1] < 10) Serial.print('0');
  Serial.print(rom[1]);
  Serial.print(F("-20"));
  if (rom[0] < 10) Serial.print('0');
  Serial.println(rom[0]);

  int raw_count = ((int)SWAP_NIBBLES(msg[27])) | ((int)SWAP_NIBBLES(msg[26])) << 8;
  Serial.print(F("Charge Count:    "));
  Serial.println(raw_count & 0x0fff);

  uint8_t error_code = msg[20] & 0x0F;
  Serial.print(F("Error Code:      0x"));
  printHex(error_code);
  if (error_code == 0) Serial.println(F(" OK"));
  else if (error_code == 1) Serial.println(F(" Overloaded"));
  else if (error_code == 5) Serial.println(F(" Warning"));
  else Serial.println(F(" ERROR"));

  bool locked = (error_code != 0 && error_code != 5) || !verifyMsgChecksums(msg);
  Serial.print(F("Status:          "));
  Serial.println(locked ? F("LOCKED") : F("OK"));

  Serial.print(F("Design Capacity: "));
  Serial.print(get_capacity_mah(msg[16]));
  Serial.println(F(" mAh"));

  Serial.print(F("Battery Type:    "));
  Serial.println(SWAP_NIBBLES(msg[11]));

  int overdis_raw = SWAP_NIBBLES(msg[24]);
  int undervoltage_percent = -5 * overdis_raw + 160;
  if (undervoltage_percent < 0) undervoltage_percent = 0;
  if (undervoltage_percent > 100) undervoltage_percent = 100;

  int overload_raw = SWAP_NIBBLES(msg[25]);
  int overload_percent = 5 * overload_raw - 160;
  if (overload_percent < 0) overload_percent = 0;
  if (overload_percent > 100) overload_percent = 100;

  int health_raw = 100 - (int)((raw_count & 0x0FFF) / 8.96f);
  uint8_t health_percent = (health_raw > 100) ? 100 : ((health_raw < 0) ? 0 : health_raw);

  if (has_health()) {
    health_percent = health();
    undervoltage_percent = overdischarge();
    overload_percent = overload();
  }

  Serial.print(F("Overload:        "));
  Serial.print(overload_percent);
  Serial.println(F("%"));

  Serial.print(F("Overdischarge:   "));
  Serial.print(undervoltage_percent);
  Serial.println(F("%"));

  Serial.print(F("Health:          "));
  Serial.print(health_percent);
  Serial.print(F("% "));
  Serial.println(has_health() ? F("(BMS)") : F("(est)"));

  if (g_battery.cell_count > 0) {
    float min_v = g_battery.voltages[0];
    for (int i = 1; i < g_battery.cell_count && i < 5; i++) {
      if (g_battery.voltages[i] < min_v) min_v = g_battery.voltages[i];
    }
    uint8_t soc = voltage_to_soc(min_v);
    Serial.print(F("Charge (SOC):    "));
    Serial.print(soc);
    Serial.println(F("%"));
  }
}

void printVoltages() {
  if (!g_battery.valid || g_battery.cell_count == 0) {
    Serial.println(F("ERROR: Cannot read voltage data"));
    return;
  }

  int voltage_count = g_battery.cell_count;
  const float* data = g_battery.voltages;

  printSeparator();
  Serial.println(F("         VOLTAGE & TEMPERATURE"));
  printSeparator();

  Serial.print(F("Pack Voltage:    "));
  Serial.print(data[6], 2);
  Serial.println(F(" V"));

  Serial.print(F("Cell Difference: "));
  Serial.print(data[5], 3);
  Serial.println(F(" V"));

  Serial.println();
  Serial.println(F("Temperature:"));

  if (data[7] > -900) {
    Serial.print(F("  Cell:    "));
    Serial.print(data[7], 1);
    Serial.println(F(" C"));
  }
  if (data[8] > -900 && data[8] > 0) {
    Serial.print(F("  MOSFET:  "));
    Serial.print(data[8], 1);
    Serial.println(F(" C"));
  }

  Serial.println();
  Serial.println(F("Individual Cell Voltages:"));

  for (int i = 0; i < voltage_count && i < 5; i++) {
    Serial.print(F("  Cell "));
    Serial.print(i + 1);
    Serial.print(F(":       "));
    Serial.print(data[i], 3);
    Serial.println(F(" V"));
  }

  Serial.println();
  float diff = data[5];
  if (diff < 0.02) {
    Serial.println(F("Balance Status:  GOOD (< 20mV)"));
  } else if (diff < 0.05) {
    Serial.println(F("Balance Status:  OK (< 50mV)"));
  } else if (diff < 0.15) {
    Serial.println(F("Balance Status:  FAIR (< 150mV)"));
  } else {
    Serial.println(F("Balance Status:  POOR - Balancing needed!"));
  }
}

void printMenu() {
  Serial.println();
  printSeparator();
  Serial.println(F("            MAIN MENU"));
  printSeparator();
  Serial.println(F("  1 - Read battery data"));
  Serial.println(F("  2 - Reset errors (quick)"));
  Serial.println(F("  3 - Unlock battery (aggressive)"));
  Serial.println(F("  4 - LED ON     5 - LED OFF"));
  Serial.println(F("  6 - Debug dump (raw + MSG)"));
  Serial.println(F("  7 - Check lock status"));
  printSeparator();
  Serial.println(F("  s - Save MSG   d - Compare MSG"));
  Serial.println(F("  v - Clone saved MSG to battery"));
  Serial.println(F("  h - Show this menu"));
  printSeparator();
}

void printRawData() {
  printSeparator();
  Serial.println(F("         DEBUG DATA DUMP"));
  printSeparator();

  if (!g_battery.valid) {
    Serial.println(F("  No data - run option 1 first"));
    return;
  }

  Serial.println(F("\n[1] Voltage data:"));
  if (g_battery.cell_count > 0) {
    for (int i = 0; i < g_battery.cell_count && i < 5; i++) {
      Serial.print(F("  Cell ")); Serial.print(i + 1);
      Serial.print(F(": ")); Serial.print(g_battery.voltages[i], 3);
      Serial.println(F(" V"));
    }
  } else {
    Serial.println(F("  Voltage read failed"));
  }

  Serial.println(F("\n[2] MSG data:"));

  const byte* rom = g_battery.rom;
  const byte* msg = g_battery.msg;

  Serial.print(F("  ROM: "));
  printHexArray(rom, 8);
  Serial.println();

  Serial.println(F("  MSG hex:"));
  for (int i = 0; i < 32; i++) {
    if (i % 16 == 0) Serial.print(F("    "));
    printHex(msg[i]);
    Serial.print(' ');
    if (i % 16 == 15) Serial.println();
  }

  Serial.println(F("\n  Key fields:"));
  Serial.print(F("    [11] Type:      ")); Serial.println(SWAP_NIBBLES(msg[11]));
  Serial.print(F("    [16] Capacity:  ")); Serial.print(get_capacity_mah(msg[16])); Serial.println(F(" mAh"));

  uint8_t err = msg[20] & 0x0F;
  Serial.print(F("    [20] Error:     0x")); Serial.print(err, HEX);
  if (err == 0) Serial.println(F(" OK"));
  else if (err == 1) Serial.println(F(" Overloaded"));
  else if (err == 5) Serial.println(F(" Warning"));
  else Serial.println(F(" ERROR!"));

  int raw_count = ((int)SWAP_NIBBLES(msg[27])) | ((int)SWAP_NIBBLES(msg[26])) << 8;
  Serial.print(F("    [26-27] Cycles: ")); Serial.println(raw_count & 0x0FFF);
}

// ============================================================================
// UNLOCK FUNCTIONS
// ============================================================================

void clearErrorWithChecksum(byte* msg) {
  msg[20] &= 0xF0;
  recalcMsgChecksums(msg);
}

void saveMSG() {
  byte data[48];
  memset(data, 0, 48);

  if (!try_charger(data)) {
    Serial.println(F("ERROR: Cannot read battery data"));
    return;
  }

  memcpy(saved_msg, data + 8, 32);
  msg_saved = true;

  Serial.println(F("MSG saved."));
  Serial.print(F("  err=0x")); Serial.print(saved_msg[20] & 0x0F, HEX);
  int cycles = ((int)SWAP_NIBBLES(saved_msg[27])) | ((int)SWAP_NIBBLES(saved_msg[26])) << 8;
  Serial.print(F(" cycles=")); Serial.println(cycles & 0x0FFF);
}

void compareMSG() {
  if (!msg_saved) {
    Serial.println(F("No saved MSG. Use 's' first."));
    return;
  }

  byte data[48];
  memset(data, 0, 48);

  if (!try_charger(data)) {
    Serial.println(F("ERROR: Cannot read battery data"));
    return;
  }

  byte* msg = data + 8;

  printSeparator();
  Serial.println(F("  MSG COMPARISON (Saved vs Current)"));
  printSeparator();

  int changes = 0;
  for (int i = 0; i < 32; i++) {
    if (saved_msg[i] != msg[i]) {
      Serial.print(i < 10 ? " " : "");
      Serial.print(i);
      Serial.print(F(": 0x"));
      printHex(saved_msg[i]);
      Serial.print(F(" -> 0x"));
      printHex(msg[i]);
      if (i == 19) Serial.print(F(" ERR"));
      else if (i == 20) Serial.print(F(" LOCK"));
      else if (i == 26 || i == 27) Serial.print(F(" CYC"));
      Serial.println();
      changes++;
    }
  }

  Serial.println(changes ? F("") : F("No changes"));
}

void cloneMSG() {
  if (!msg_saved) {
    Serial.println(F("No saved MSG. Use 's' first."));
    return;
  }

  printSeparator();
  Serial.println(F("  CLONE SAVED MSG"));
  printSeparator();
  Serial.println(F("Press 'y' to confirm:"));

  while (!Serial.available()) delay(10);
  char c = Serial.read();
  while (Serial.available()) Serial.read();

  if (c != 'y' && c != 'Y') {
    Serial.println(F("Cancelled"));
    return;
  }

  byte clone_msg[32];
  memcpy(clone_msg, saved_msg, 32);
  clearErrorWithChecksum(clone_msg);

  Serial.println(F("Writing..."));
  write_msg_to_eeprom(clone_msg);

  byte data[48];
  if (try_charger(data)) {
    byte* msg = data + 8;
    Serial.print(F("Result: err=0x"));
    Serial.println(msg[20] & 0x0F, HEX);
  }
  Serial.println(F("Done."));
}

void resetBatteryErrors() {
  Serial.println(F("Resetting errors..."));
  for (int i = 0; i < 3; i++) {
    delay(300);
    testmode_cmd();
    reset_error_cmd();
    Serial.print('.');
  }
  Serial.println(F("\nReset complete."));
}

void unlockBattery() {
  printSeparator();
  Serial.println(F("     AGGRESSIVE BATTERY UNLOCK"));
  printSeparator();

  // Phase 1
  Serial.println(F("\nPhase 1: Standard reset..."));
  for (int cycle = 0; cycle < 5; cycle++) {
    Serial.print(F("  Cycle "));
    Serial.print(cycle + 1);

    trigger_power();
    for (int i = 0; i < 5; i++) {
      delay(200);
      testmode_cmd();
      reset_error_cmd();
      Serial.print('.');
    }
    Serial.println();

    if (!isBatteryLocked()) {
      Serial.println(F("\n*** SUCCESS: Battery unlocked! ***"));
      return;
    }
  }

  // Phase 2
  Serial.println(F("\nPhase 2: EEPROM clear..."));

  byte charger_data[48];
  memset(charger_data, 0, 48);

  if (try_charger(charger_data)) {
    byte* raw_msg = charger_data + 8;
    clearErrorWithChecksum(raw_msg);

    for (int attempt = 0; attempt < 3; attempt++) {
      Serial.print(F("  Write "));
      Serial.print(attempt + 1);

      write_msg_to_eeprom(raw_msg);

      Serial.print(F(" power cycle..."));
      set_enablepin(false);
      delay(2000);
      set_enablepin(true);
      delay(1000);

      if (!isBatteryLocked()) {
        Serial.println(F("\n*** SUCCESS: Battery unlocked! ***"));
        return;
      }
      Serial.println(F(" still locked"));
    }
  }

  // Phase 3
  Serial.println(F("\nPhase 3: Power cycling..."));
  for (int cycle = 0; cycle < 3; cycle++) {
    set_enablepin(false);
    delay(2000);
    set_enablepin(true);
    delay(1000);

    for (int i = 0; i < 10; i++) {
      testmode_cmd();
      delay(100);
      reset_error_cmd();
      delay(100);
    }

    if (!isBatteryLocked()) {
      Serial.println(F("\n*** SUCCESS: Battery unlocked! ***"));
      return;
    }
  }

  Serial.println(F("\nUnlock failed. May need cell charging or PCB repair."));
}

// ============================================================================
// HIGH-LEVEL FUNCTIONS
// ============================================================================

void readAndPrintAll() {
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
  printMenu();
}

// ============================================================================
// SETUP AND LOOP
// ============================================================================

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

void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();

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
        testmode_cmd();
        leds_on_cmd();
        Serial.println(F("Done."));
        printMenu();
        break;

      case '5':
        Serial.println(F("\nTurning LEDs OFF..."));
        testmode_cmd();
        leds_off_cmd();
        Serial.println(F("Done."));
        printMenu();
        break;

      case '6':
        Serial.println(F("\nReading raw data..."));
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

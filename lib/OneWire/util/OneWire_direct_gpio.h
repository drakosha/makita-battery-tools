#ifndef OneWire_Direct_GPIO_h
#define OneWire_Direct_GPIO_h

// This header should ONLY be included by OneWire.cpp.  These defines are
// meant to be private, used within OneWire.cpp, but not exposed to Arduino
// sketches or other libraries which may include OneWire.h.

#include <stdint.h>

// Platform specific I/O definitions

#if defined(__AVR__)
#define PIN_TO_BASEREG(pin)             (portInputRegister(digitalPinToPort(pin)))
#define PIN_TO_BITMASK(pin)             (digitalPinToBitMask(pin))
#define IO_REG_TYPE uint8_t
#define IO_REG_BASE_ATTR asm("r30")
#define IO_REG_MASK_ATTR
#if defined(__AVR_ATmega4809__)
#define DIRECT_READ(base, mask)         (((*(base)) & (mask)) ? 1 : 0)
#define DIRECT_MODE_INPUT(base, mask)   ((*((base)-8)) &= ~(mask))
#define DIRECT_MODE_OUTPUT(base, mask)  ((*((base)-8)) |= (mask))
#define DIRECT_WRITE_LOW(base, mask)    ((*((base)-4)) &= ~(mask))
#define DIRECT_WRITE_HIGH(base, mask)   ((*((base)-4)) |= (mask))
#else
#define DIRECT_READ(base, mask)         (((*(base)) & (mask)) ? 1 : 0)
#define DIRECT_MODE_INPUT(base, mask)   ((*((base)+1)) &= ~(mask))
#define DIRECT_MODE_OUTPUT(base, mask)  ((*((base)+1)) |= (mask))
#define DIRECT_WRITE_LOW(base, mask)    ((*((base)+2)) &= ~(mask))
#define DIRECT_WRITE_HIGH(base, mask)   ((*((base)+2)) |= (mask))
#endif

#elif defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK66FX1M0__) || defined(__MK64FX512__)
#define PIN_TO_BASEREG(pin)             (portOutputRegister(pin))
#define PIN_TO_BITMASK(pin)             (1)
#define IO_REG_TYPE uint8_t
#define IO_REG_BASE_ATTR
#define IO_REG_MASK_ATTR __attribute__ ((unused))
#define DIRECT_READ(base, mask)         (*((base)+512))
#define DIRECT_MODE_INPUT(base, mask)   (*((base)+640) = 0)
#define DIRECT_MODE_OUTPUT(base, mask)  (*((base)+640) = 1)
#define DIRECT_WRITE_LOW(base, mask)    (*((base)+256) = 1)
#define DIRECT_WRITE_HIGH(base, mask)   (*((base)+128) = 1)

#elif defined(__MKL26Z64__)
#define PIN_TO_BASEREG(pin)             (portOutputRegister(pin))
#define PIN_TO_BITMASK(pin)             (digitalPinToBitMask(pin))
#define IO_REG_TYPE uint8_t
#define IO_REG_BASE_ATTR
#define IO_REG_MASK_ATTR
#define DIRECT_READ(base, mask)         ((*((base)+16) & (mask)) ? 1 : 0)
#define DIRECT_MODE_INPUT(base, mask)   (*((base)+20) &= ~(mask))
#define DIRECT_MODE_OUTPUT(base, mask)  (*((base)+20) |= (mask))
#define DIRECT_WRITE_LOW(base, mask)    (*((base)+8) = (mask))
#define DIRECT_WRITE_HIGH(base, mask)   (*((base)+4) = (mask))

#elif defined(__IMXRT1052__) || defined(__IMXRT1062__)
#define PIN_TO_BASEREG(pin)             (portOutputRegister(pin))
#define PIN_TO_BITMASK(pin)             (digitalPinToBitMask(pin))
#define IO_REG_TYPE uint32_t
#define IO_REG_BASE_ATTR
#define IO_REG_MASK_ATTR
#define DIRECT_READ(base, mask)         ((*((base)+2) & (mask)) ? 1 : 0)
#define DIRECT_MODE_INPUT(base, mask)   (*((base)+1) &= ~(mask))
#define DIRECT_MODE_OUTPUT(base, mask)  (*((base)+1) |= (mask))
#define DIRECT_WRITE_LOW(base, mask)    (*((base)+34) = (mask))
#define DIRECT_WRITE_HIGH(base, mask)   (*((base)+33) = (mask))

#elif defined(__SAM3X8E__) || defined(__SAM3A8C__) || defined(__SAM3A4C__)
#define PIN_TO_BASEREG(pin)             (&(digitalPinToPort(pin)->PIO_PER))
#define PIN_TO_BITMASK(pin)             (digitalPinToBitMask(pin))
#define IO_REG_TYPE uint32_t
#define IO_REG_BASE_ATTR
#define IO_REG_MASK_ATTR
#define DIRECT_READ(base, mask)         (((*((base)+15)) & (mask)) ? 1 : 0)
#define DIRECT_MODE_INPUT(base, mask)   ((*((base)+5)) = (mask))
#define DIRECT_MODE_OUTPUT(base, mask)  ((*((base)+4)) = (mask))
#define DIRECT_WRITE_LOW(base, mask)    ((*((base)+13)) = (mask))
#define DIRECT_WRITE_HIGH(base, mask)   ((*((base)+12)) = (mask))
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#endif

#elif defined(__PIC32MX__)
#define PIN_TO_BASEREG(pin)             (portModeRegister(digitalPinToPort(pin)))
#define PIN_TO_BITMASK(pin)             (digitalPinToBitMask(pin))
#define IO_REG_TYPE uint32_t
#define IO_REG_BASE_ATTR
#define IO_REG_MASK_ATTR
#define DIRECT_READ(base, mask)         (((*(base+4)) & (mask)) ? 1 : 0)
#define DIRECT_MODE_INPUT(base, mask)   ((*(base+2)) = (mask))
#define DIRECT_MODE_OUTPUT(base, mask)  ((*(base+1)) = (mask))
#define DIRECT_WRITE_LOW(base, mask)    ((*(base+8+1)) = (mask))
#define DIRECT_WRITE_HIGH(base, mask)   ((*(base+8+2)) = (mask))

#elif defined(ARDUINO_ARCH_ESP8266)
#define PIN_TO_BASEREG(pin)             ((volatile uint32_t*) GPO)
#define PIN_TO_BITMASK(pin)             (1UL << (pin))
#define IO_REG_TYPE uint32_t
#define IO_REG_BASE_ATTR
#define IO_REG_MASK_ATTR

static inline __attribute__((always_inline))
void directModeInput(IO_REG_TYPE mask)
{
    if(mask > 0x8000)
    {
        GP16FFS(GPFFS_GPIO(16));
        GPC16 = 0;
        GP16E &= ~1;
    }
    else
    {
        GPE &= ~(mask);
    }
}

static inline __attribute__((always_inline))
void directModeOutput(IO_REG_TYPE mask)
{
    if(mask > 0x8000)
    {
        GP16FFS(GPFFS_GPIO(16));
        GPC16 = 0;
        GP16E |= 1;
    }
    else
    {
        GPE |= (mask);
    }
}
static inline __attribute__((always_inline))
bool directRead(IO_REG_TYPE mask)
{
    if(mask > 0x8000)
        return GP16I & 0x01;
    else
        return ((GPI & (mask)) ? true : false);
}

#define DIRECT_READ(base, mask)             directRead(mask)
#define DIRECT_MODE_INPUT(base, mask)       directModeInput(mask)
#define DIRECT_MODE_OUTPUT(base, mask)      directModeOutput(mask)
#define DIRECT_WRITE_LOW(base, mask)    (mask > 0x8000) ? GP16O &= ~1 : (GPOC = (mask))
#define DIRECT_WRITE_HIGH(base, mask)   (mask > 0x8000) ? GP16O |= 1 : (GPOS = (mask))

#elif defined(ARDUINO_ARCH_ESP32)
#include <driver/rtc_io.h>
#include <soc/gpio_struct.h>
#define PIN_TO_BASEREG(pin)             (0)
#define PIN_TO_BITMASK(pin)             (pin)
#define IO_REG_TYPE uint32_t
#define IO_REG_BASE_ATTR
#define IO_REG_MASK_ATTR

static inline __attribute__((always_inline))
IO_REG_TYPE directRead(IO_REG_TYPE pin)
{
#if CONFIG_IDF_TARGET_ESP32C3
    return (GPIO.in.val >> pin) & 0x1;
#else
    if ( pin < 32 )
        return (GPIO.in >> pin) & 0x1;
    else if ( pin < 46 )
        return (GPIO.in1.val >> (pin - 32)) & 0x1;
#endif
    return 0;
}

static inline __attribute__((always_inline))
void directWriteLow(IO_REG_TYPE pin)
{
#if CONFIG_IDF_TARGET_ESP32C3
    GPIO.out_w1tc.val = ((uint32_t)1 << pin);
#else
    if ( pin < 32 )
        GPIO.out_w1tc = ((uint32_t)1 << pin);
    else if ( pin < 46 )
        GPIO.out1_w1tc.val = ((uint32_t)1 << (pin - 32));
#endif
}

static inline __attribute__((always_inline))
void directWriteHigh(IO_REG_TYPE pin)
{
#if CONFIG_IDF_TARGET_ESP32C3
    GPIO.out_w1ts.val = ((uint32_t)1 << pin);
#else
    if ( pin < 32 )
        GPIO.out_w1ts = ((uint32_t)1 << pin);
    else if ( pin < 46 )
        GPIO.out1_w1ts.val = ((uint32_t)1 << (pin - 32));
#endif
}

static inline __attribute__((always_inline))
void directModeInput(IO_REG_TYPE pin)
{
#if CONFIG_IDF_TARGET_ESP32C3
    GPIO.enable_w1tc.val = ((uint32_t)1 << (pin));
#else
    if ( digitalPinIsValid(pin) )
    {
        if ( pin < 32 )
            GPIO.enable_w1tc = ((uint32_t)1 << pin);
        else
            GPIO.enable1_w1tc.val = ((uint32_t)1 << (pin - 32));
    }
#endif
}

static inline __attribute__((always_inline))
void directModeOutput(IO_REG_TYPE pin)
{
#if CONFIG_IDF_TARGET_ESP32C3
    GPIO.enable_w1ts.val = ((uint32_t)1 << (pin));
#else
    if ( digitalPinIsValid(pin) && pin <= 33 )
    {
        if ( pin < 32 )
            GPIO.enable_w1ts = ((uint32_t)1 << pin);
        else
            GPIO.enable1_w1ts.val = ((uint32_t)1 << (pin - 32));
    }
#endif
}

#define DIRECT_READ(base, pin)          directRead(pin)
#define DIRECT_WRITE_LOW(base, pin)     directWriteLow(pin)
#define DIRECT_WRITE_HIGH(base, pin)    directWriteHigh(pin)
#define DIRECT_MODE_INPUT(base, pin)    directModeInput(pin)
#define DIRECT_MODE_OUTPUT(base, pin)   directModeOutput(pin)

#ifdef  interrupts
#undef  interrupts
#endif
#ifdef  noInterrupts
#undef  noInterrupts
#endif
#define noInterrupts() {portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;portENTER_CRITICAL(&mux)
#define interrupts() portEXIT_CRITICAL(&mux);}

#elif defined(ARDUINO_ARCH_STM32)
#define PIN_TO_BASEREG(pin)             (0)
#define PIN_TO_BITMASK(pin)             ((uint32_t)digitalPinToPinName(pin))
#define IO_REG_TYPE uint32_t
#define IO_REG_BASE_ATTR
#define IO_REG_MASK_ATTR
#define DIRECT_READ(base, pin)          digitalReadFast((PinName)pin)
#define DIRECT_WRITE_LOW(base, pin)     digitalWriteFast((PinName)pin, LOW)
#define DIRECT_WRITE_HIGH(base, pin)    digitalWriteFast((PinName)pin, HIGH)
#define DIRECT_MODE_INPUT(base, pin)    pin_function((PinName)pin, STM_PIN_DATA(STM_MODE_INPUT, GPIO_NOPULL, 0))
#define DIRECT_MODE_OUTPUT(base, pin)   pin_function((PinName)pin, STM_PIN_DATA(STM_MODE_OUTPUT_PP, GPIO_NOPULL, 0))

#elif defined(__SAMD21G18A__)
#define PIN_TO_BASEREG(pin)             portModeRegister(digitalPinToPort(pin))
#define PIN_TO_BITMASK(pin)             (digitalPinToBitMask(pin))
#define IO_REG_TYPE uint32_t
#define IO_REG_BASE_ATTR
#define IO_REG_MASK_ATTR
#define DIRECT_READ(base, mask)         (((*((base)+8)) & (mask)) ? 1 : 0)
#define DIRECT_MODE_INPUT(base, mask)   ((*((base)+1)) = (mask))
#define DIRECT_MODE_OUTPUT(base, mask)  ((*((base)+2)) = (mask))
#define DIRECT_WRITE_LOW(base, mask)    ((*((base)+5)) = (mask))
#define DIRECT_WRITE_HIGH(base, mask)   ((*((base)+6)) = (mask))

#elif defined(__riscv)
#define PIN_TO_BASEREG(pin)             (0)
#define PIN_TO_BITMASK(pin)             digitalPinToBitMask(pin)
#define IO_REG_TYPE uint32_t
#define IO_REG_BASE_ATTR
#define IO_REG_MASK_ATTR

static inline __attribute__((always_inline))
IO_REG_TYPE directRead(IO_REG_TYPE mask)
{
    return ((GPIO_REG(GPIO_INPUT_VAL) & mask) != 0) ? 1 : 0;
}

static inline __attribute__((always_inline))
void directModeInput(IO_REG_TYPE mask)
{
    GPIO_REG(GPIO_OUTPUT_XOR)  &= ~mask;
    GPIO_REG(GPIO_IOF_EN)      &= ~mask;
    GPIO_REG(GPIO_INPUT_EN)  |=  mask;
    GPIO_REG(GPIO_OUTPUT_EN) &= ~mask;
}

static inline __attribute__((always_inline))
void directModeOutput(IO_REG_TYPE mask)
{
    GPIO_REG(GPIO_OUTPUT_XOR)  &= ~mask;
    GPIO_REG(GPIO_IOF_EN)      &= ~mask;
    GPIO_REG(GPIO_INPUT_EN)  &= ~mask;
    GPIO_REG(GPIO_OUTPUT_EN) |=  mask;
}

static inline __attribute__((always_inline))
void directWriteLow(IO_REG_TYPE mask)
{
    GPIO_REG(GPIO_OUTPUT_VAL) &= ~mask;
}

static inline __attribute__((always_inline))
void directWriteHigh(IO_REG_TYPE mask)
{
    GPIO_REG(GPIO_OUTPUT_VAL) |= mask;
}

#define DIRECT_READ(base, mask)          directRead(mask)
#define DIRECT_WRITE_LOW(base, mask)     directWriteLow(mask)
#define DIRECT_WRITE_HIGH(base, mask)    directWriteHigh(mask)
#define DIRECT_MODE_INPUT(base, mask)    directModeInput(mask)
#define DIRECT_MODE_OUTPUT(base, mask)   directModeOutput(mask)

#else
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
#warning "OneWire. Fallback mode. Using API calls for pinMode,digitalRead and digitalWrite."

#endif

#endif

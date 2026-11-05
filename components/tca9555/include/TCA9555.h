#pragma once

#include <stdint.h>
#include <stdbool.h>

// T5.1: C-ified flat API (replaces the TCA9555 C++ class and derived classes).
// The type field distinguishes TCA9535 (35), PCA9555 (55), PCA9535 (35), CAT9555 (55).
// Uses the uc2_wire_c.h C-facing I2C API (no uc2::Wire_ dependency).

typedef struct {
    uint8_t address;
    uint8_t error;
    uint8_t type;
} tca9555_dev_t;

#define TCA9555_LIB_VERSION "0.4.4"

#define TCA9555_OK                        0x00
#define TCA9555_PIN_ERROR                 0x81
#define TCA9555_I2C_ERROR                 0x82
#define TCA9555_VALUE_ERROR               0x83
#define TCA9555_PORT_ERROR                0x84

#define TCA9555_INVALID_READ              -100

#ifndef TCA9555_PIN_NAMES
#define TCA9555_PIN_NAMES
    #define TCA_P00         0
    #define TCA_P01         1
    #define TCA_P02         2
    #define TCA_P03         3
    #define TCA_P04         4
    #define TCA_P05         5
    #define TCA_P06         6
    #define TCA_P07         7
    #define TCA_P10         8
    #define TCA_P11         9
    #define TCA_P12         10
    #define TCA_P13         11
    #define TCA_P14         12
    #define TCA_P15         13
    #define TCA_P16         14
    #define TCA_P17         15
#endif

#ifdef __cplusplus
extern "C" {
#endif

void tca9555_init(tca9555_dev_t *dev, uint8_t address, uint8_t type);
bool tca9555_begin(tca9555_dev_t *dev, uint8_t mode, uint16_t mask);
bool tca9555_is_connected(tca9555_dev_t *dev);
uint8_t tca9555_get_address(tca9555_dev_t *dev);

// 1 PIN INTERFACE
bool tca9555_pinmode1(tca9555_dev_t *dev, uint8_t pin, uint8_t mode);
bool tca9555_write1(tca9555_dev_t *dev, uint8_t pin, uint8_t value);
uint8_t tca9555_read1(tca9555_dev_t *dev, uint8_t pin);
bool tca9555_set_polarity(tca9555_dev_t *dev, uint8_t pin, uint8_t value);
uint8_t tca9555_get_polarity(tca9555_dev_t *dev, uint8_t pin);

// 8 PIN INTERFACE
bool tca9555_pinmode8(tca9555_dev_t *dev, uint8_t port, uint8_t mask);
bool tca9555_write8(tca9555_dev_t *dev, uint8_t port, uint8_t mask);
int tca9555_read8(tca9555_dev_t *dev, uint8_t port);
bool tca9555_set_polarity8(tca9555_dev_t *dev, uint8_t port, uint8_t mask);
uint8_t tca9555_get_polarity8(tca9555_dev_t *dev, uint8_t port);

// 16 PIN INTERFACE
bool tca9555_pinmode16(tca9555_dev_t *dev, uint16_t mask);
bool tca9555_write16(tca9555_dev_t *dev, uint16_t mask);
uint16_t tca9555_read16(tca9555_dev_t *dev);
bool tca9555_set_polarity16(tca9555_dev_t *dev, uint16_t mask);
uint8_t tca9555_get_polarity16(tca9555_dev_t *dev);

// OTHER
int tca9555_last_error(tca9555_dev_t *dev);
uint8_t tca9555_get_type(tca9555_dev_t *dev);

#ifdef __cplusplus
}
#endif
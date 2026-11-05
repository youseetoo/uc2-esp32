// T5.1: TCA9555 C-ified from C++ class to flat C functions.
// Original: https://github.com/RobTillaart/TCA9555 (Apache-2.0)
// I2C TCA9555 16 channel port expander driver.
// Uses the uc2_wire_c.h C-facing I2C API.

#include "TCA9555.h"
#include "uc2_hal/uc2_wire_c.h"
#include "uc2_hal/uc2_gpio.h"
#include <string.h>
#include <string.h>

// Register addresses
#define TCA9555_INPUT_PORT_REGISTER_0     0x00
#define TCA9555_INPUT_PORT_REGISTER_1     0x01
#define TCA9555_OUTPUT_PORT_REGISTER_0    0x02
#define TCA9555_OUTPUT_PORT_REGISTER_1    0x03
#define TCA9555_POLARITY_REGISTER_0       0x04
#define TCA9555_POLARITY_REGISTER_1       0x05
#define TCA9555_CONFIGURATION_PORT_0      0x06
#define TCA9555_CONFIGURATION_PORT_1      0x07

static bool tca_write_register(tca9555_dev_t *dev, uint8_t reg, uint8_t value)
{
    wire_begin_transmission(dev->address);
    wire_write(reg);
    wire_write(value);
    uint8_t rc = wire_end_transmission(true);
    if (rc != 0) {
        dev->error = TCA9555_I2C_ERROR;
        return false;
    }
    dev->error = TCA9555_OK;
    return true;
}

static uint8_t tca_read_register(tca9555_dev_t *dev, uint8_t reg)
{
    wire_begin_transmission(dev->address);
    wire_write(reg);
    uint8_t rc = wire_end_transmission(false);
    if (rc != 0) {
        dev->error = TCA9555_I2C_ERROR;
        return (uint8_t)rc;
    }
    wire_request_from(dev->address, 1);
    int val = wire_read();
    if (val < 0) {
        dev->error = TCA9555_I2C_ERROR;
        return 0xFF;
    }
    dev->error = TCA9555_OK;
    return (uint8_t)val;
}

void tca9555_init(tca9555_dev_t *dev, uint8_t address, uint8_t type)
{
    memset(dev, 0, sizeof(*dev));
    dev->address = address;
    dev->error = TCA9555_OK;
    dev->type = type;
}

bool tca9555_begin(tca9555_dev_t *dev, uint8_t mode, uint16_t mask)
{
    if ((dev->address < 0x20) || (dev->address > 0x27)) return false;
    if (!tca9555_is_connected(dev)) return false;

    if (mode == OUTPUT) {
        tca9555_pinmode16(dev, 0x0000);
        tca9555_write16(dev, mask);
    } else {
        tca9555_pinmode16(dev, 0xFFFF);
    }
    return true;
}

bool tca9555_is_connected(tca9555_dev_t *dev)
{
    wire_begin_transmission(dev->address);
    uint8_t rc = wire_end_transmission(true);
    return (rc == 0);
}

uint8_t tca9555_get_address(tca9555_dev_t *dev)
{
    return dev->address;
}

// 1 PIN INTERFACE
bool tca9555_pinmode1(tca9555_dev_t *dev, uint8_t pin, uint8_t mode)
{
    if (pin > 15) { dev->error = TCA9555_PIN_ERROR; return false; }
    if ((mode != INPUT) && (mode != OUTPUT)) { dev->error = TCA9555_VALUE_ERROR; return false; }

    uint8_t confreg = TCA9555_CONFIGURATION_PORT_0;
    if (pin > 7) {
        confreg = TCA9555_CONFIGURATION_PORT_1;
        pin -= 8;
    }
    uint8_t val = tca_read_register(dev, confreg);
    uint8_t prevVal = val;
    uint8_t mask = 1 << pin;
    if (mode == INPUT) val |= mask;
    else val &= ~mask;
    if (val != prevVal) {
        return tca_write_register(dev, confreg, val);
    }
    dev->error = TCA9555_OK;
    return true;
}

bool tca9555_write1(tca9555_dev_t *dev, uint8_t pin, uint8_t value)
{
    if (pin > 15) { dev->error = TCA9555_PIN_ERROR; return false; }

    uint8_t opr = TCA9555_OUTPUT_PORT_REGISTER_0;
    if (pin > 7) {
        opr = TCA9555_OUTPUT_PORT_REGISTER_1;
        pin -= 8;
    }
    uint8_t val = tca_read_register(dev, opr);
    uint8_t prevVal = val;
    uint8_t mask = 1 << pin;
    if (value) val |= mask;
    else val &= ~mask;
    if (val != prevVal) {
        return tca_write_register(dev, opr, val);
    }
    dev->error = TCA9555_OK;
    return true;
}

uint8_t tca9555_read1(tca9555_dev_t *dev, uint8_t pin)
{
    if (pin > 15) { dev->error = TCA9555_PIN_ERROR; return TCA9555_INVALID_READ; }

    uint8_t ipr = TCA9555_INPUT_PORT_REGISTER_0;
    if (pin > 7) {
        ipr = TCA9555_INPUT_PORT_REGISTER_1;
        pin -= 8;
    }
    uint8_t val = tca_read_register(dev, ipr);
    uint8_t mask = 1 << pin;
    dev->error = TCA9555_OK;
    if (val & mask) return HIGH;
    return LOW;
}

bool tca9555_set_polarity(tca9555_dev_t *dev, uint8_t pin, uint8_t value)
{
    if (pin > 15) { dev->error = TCA9555_PIN_ERROR; return false; }
    if ((value != LOW) && (value != HIGH)) { dev->error = TCA9555_VALUE_ERROR; return false; }

    uint8_t polreg = TCA9555_POLARITY_REGISTER_0;
    if (pin > 7) {
        polreg = TCA9555_POLARITY_REGISTER_1;
        pin -= 8;
    }
    uint8_t val = tca_read_register(dev, polreg);
    uint8_t prevVal = val;
    uint8_t mask = 1 << pin;
    if (value == HIGH) val |= mask;
    else val &= ~mask;
    if (val != prevVal) {
        return tca_write_register(dev, polreg, val);
    }
    dev->error = TCA9555_OK;
    return true;
}

uint8_t tca9555_get_polarity(tca9555_dev_t *dev, uint8_t pin)
{
    if (pin > 15) { dev->error = TCA9555_PIN_ERROR; return false; }

    uint8_t polreg = TCA9555_POLARITY_REGISTER_0;
    if (pin > 7) polreg = TCA9555_POLARITY_REGISTER_1;
    dev->error = TCA9555_OK;
    uint8_t mask = tca_read_register(dev, polreg);
    return (mask >> pin) == 0x01;
}

// 8 PIN INTERFACE
bool tca9555_pinmode8(tca9555_dev_t *dev, uint8_t port, uint8_t mask)
{
    if (port > 1) { dev->error = TCA9555_PORT_ERROR; return false; }
    dev->error = TCA9555_OK;
    if (port == 0) return tca_write_register(dev, TCA9555_CONFIGURATION_PORT_0, mask);
    return tca_write_register(dev, TCA9555_CONFIGURATION_PORT_1, mask);
}

bool tca9555_write8(tca9555_dev_t *dev, uint8_t port, uint8_t mask)
{
    if (port > 1) { dev->error = TCA9555_PORT_ERROR; return false; }
    dev->error = TCA9555_OK;
    if (port == 0) return tca_write_register(dev, TCA9555_OUTPUT_PORT_REGISTER_0, mask);
    return tca_write_register(dev, TCA9555_OUTPUT_PORT_REGISTER_1, mask);
}

int tca9555_read8(tca9555_dev_t *dev, uint8_t port)
{
    if (port > 1) { dev->error = TCA9555_PORT_ERROR; return TCA9555_INVALID_READ; }
    dev->error = TCA9555_OK;
    if (port == 0) return tca_read_register(dev, TCA9555_INPUT_PORT_REGISTER_0);
    return tca_read_register(dev, TCA9555_INPUT_PORT_REGISTER_1);
}

bool tca9555_set_polarity8(tca9555_dev_t *dev, uint8_t port, uint8_t mask)
{
    if (port > 1) { dev->error = TCA9555_PORT_ERROR; return false; }
    dev->error = TCA9555_OK;
    if (port == 0) return tca_write_register(dev, TCA9555_POLARITY_REGISTER_0, mask);
    return tca_write_register(dev, TCA9555_POLARITY_REGISTER_1, mask);
}

uint8_t tca9555_get_polarity8(tca9555_dev_t *dev, uint8_t port)
{
    if (port > 1) { dev->error = TCA9555_PORT_ERROR; return 0; }
    dev->error = TCA9555_OK;
    if (port == 0) return tca_read_register(dev, TCA9555_POLARITY_REGISTER_0);
    return tca_read_register(dev, TCA9555_POLARITY_REGISTER_1);
}

// 16 PIN INTERFACE
bool tca9555_pinmode16(tca9555_dev_t *dev, uint16_t mask)
{
    bool b = true;
    b &= tca9555_pinmode8(dev, 0, mask & 0xFF);
    b &= tca9555_pinmode8(dev, 1, mask >> 8);
    return b;
}

bool tca9555_write16(tca9555_dev_t *dev, uint16_t mask)
{
    bool b = true;
    b &= tca9555_write8(dev, 0, mask & 0xFF);
    b &= tca9555_write8(dev, 1, mask >> 8);
    return b;
}

uint16_t tca9555_read16(tca9555_dev_t *dev)
{
    uint16_t rv = 0;
    rv |= ((uint16_t)tca9555_read8(dev, 1) << 8);
    rv |= (uint16_t)tca9555_read8(dev, 0);
    return rv;
}

bool tca9555_set_polarity16(tca9555_dev_t *dev, uint16_t mask)
{
    bool b = true;
    b &= tca9555_set_polarity8(dev, 0, mask & 0xFF);
    b &= tca9555_set_polarity8(dev, 1, mask >> 8);
    return b;
}

uint8_t tca9555_get_polarity16(tca9555_dev_t *dev)
{
    uint16_t rv = 0;
    rv |= ((uint16_t)tca9555_get_polarity8(dev, 1) << 8);
    rv |= (uint16_t)tca9555_get_polarity8(dev, 0);
    return (uint8_t)rv;
}

int tca9555_last_error(tca9555_dev_t *dev)
{
    int error = dev->error;
    dev->error = TCA9555_OK;
    return error;
}

uint8_t tca9555_get_type(tca9555_dev_t *dev)
{
    return dev->type;
}
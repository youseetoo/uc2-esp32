// T2.6: TMCStepper SPI shim. The accelmotor drives the TMC2209 over UART (Stream),
// so the hardware-SPI path (TMC2130/TMC2160/TMC5130/TMC5160/TMC2660 SPI mode) is
// dead code here. This stub provides the Arduino `SPI` object + SPISettings so the
// library compiles; the methods are no-ops (never called at runtime on this app).
// The software-SPI (bit-bang) SW_SPIClass is separate (uses TMC_platforms.h
// digitalWrite/digitalRead macros -> uc2_gpio) and is real.
#pragma once
#include <cstddef>
#include <cstdint>

// Arduino-compatible bit order / data mode constants (values as in the Arduino core).
#define MSBFIRST 0b00000001
#define LSBFIRST 0b00000000
#define SPI_MODE0 0b00
#define SPI_MODE1 0b01
#define SPI_MODE2 0b10
#define SPI_MODE3 0b11

struct SPISettings
{
    SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) { (void)clock; (void)bitOrder; (void)dataMode; }
    SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode, uint8_t bus) { (void)clock; (void)bitOrder; (void)dataMode; (void)bus; }
    SPISettings() {}
};

class SPIClass
{
public:
    void begin() {}
    void end() {}
    void beginTransaction(const SPISettings &settings) { (void)settings; }
    void endTransaction() {}
    void transfer(uint8_t *buf, size_t count) { (void)buf; (void)count; }
    uint8_t transfer(uint8_t v) { return v; }
    uint16_t transfer16(uint16_t v) { return v; }
    void setBitOrder(uint8_t bitOrder) { (void)bitOrder; }
    void setDataMode(uint8_t dataMode) { (void)dataMode; }
    void setClockDivider(uint32_t divider) { (void)divider; }
    bool usingSPI(int spi) { (void)spi; return false; }
};

// A single no-op hardware-SPI object (inline: one definition shared across TUs).
inline SPIClass SPI;

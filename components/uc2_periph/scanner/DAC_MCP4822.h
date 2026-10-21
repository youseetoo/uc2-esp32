/**
 * @file DAC_MCP4822.h
 * @brief MCP4822 Dual 12-bit DAC driver for galvo control
 *
 * This driver provides control for the MCP4822 dual-channel DAC via SPI.
 * - Channel A: X-axis galvo control
 * - Channel B: Y-axis galvo control
 *
 * Features:
 * - 20 MHz SPI communication for maximum update rate
 * - Optional LDAC pin for synchronized DAC updates
 * - Polling mode for deterministic timing
 *
 * @note This is adapted from the LASERSCANNER project for UC2 integration.
 * T4.1: pure C (was a C++ class wrapping the same SPI calls; single static
 * instance, the only one the firmware ever used — GalvoController::dac_).
 */

#ifndef DAC_MCP4822_H
#define DAC_MCP4822_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the MCP4822 DAC (full control)
 * @param spi_host SPI host (e.g., SPI2_HOST, SPI3_HOST)
 * @param pin_mosi MOSI pin number (SDI on DAC)
 * @param pin_sclk SCLK pin number
 * @param pin_cs CS pin number
 * @param pin_ldac LDAC pin number (-1 if not used, tie to GND)
 * @param clock_speed_hz SPI clock speed in Hz (max 20 MHz for MCP4822)
 * @return true if initialization successful
 */
bool dac4822_init_host(spi_host_device_t spi_host, int pin_mosi, int pin_sclk,
                       int pin_cs, int pin_ldac, int clock_speed_hz);

/**
 * @brief Initialize the MCP4822 DAC (simplified, uses SPI2_HOST @ 20 MHz)
 * @param pin_mosi MOSI pin number (SDI on DAC)
 * @param pin_sclk SCLK pin number
 * @param pin_cs CS pin number
 * @param pin_ldac LDAC pin number (-1 if not used, tie to GND)
 * @return true if initialization successful
 */
static inline bool dac4822_init(int pin_mosi, int pin_sclk, int pin_cs, int pin_ldac)
{
    return dac4822_init_host(SPI2_HOST, pin_mosi, pin_sclk, pin_cs, pin_ldac, 20000000);
}

/**
 * @brief Check if DAC is initialized
 * @return true if initialized
 */
bool dac4822_is_initialized(void);

/**
 * @brief Write a 12-bit value to a DAC channel
 * @param channel_b false for channel A (X), true for channel B (Y)
 * @param value 12-bit value (0-4095)
 */
void dac4822_write(bool channel_b, uint16_t value);

/**
 * @brief Set X-axis position (channel A)
 * @param value 12-bit value (0-4095)
 */
static inline void dac4822_set_x(uint16_t value) { dac4822_write(false, value); }

/**
 * @brief Set Y-axis position (channel B)
 * @param value 12-bit value (0-4095)
 */
static inline void dac4822_set_y(uint16_t value) { dac4822_write(true, value); }

/**
 * @brief Set both X and Y positions
 * @param x 12-bit X value (0-4095)
 * @param y 12-bit Y value (0-4095)
 */
void dac4822_set_xy(uint16_t x, uint16_t y);

/**
 * @brief Pulse the LDAC pin to latch all channels
 * Only has effect if LDAC pin was configured during init.
 * If LDAC is tied to GND, DAC updates immediately on CS rising edge.
 */
void dac4822_ldac_pulse(void);

#ifdef __cplusplus
}
#endif

#endif // DAC_MCP4822_H

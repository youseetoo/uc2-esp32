// Host stub: soc/soc_caps.h
//
// Per-chip capability constants. Values mirror the ESP32-S3 (the app target);
// on the host they only matter as bounds checks (uc2_gpio.cpp etc.).
#pragma once
#include <stdint.h>

#define SOC_GPIO_PIN_COUNT 40

// UART ports (uc2_serial_c.h / uc2_serial.h).
#define UART_NUM_0 0
#define UART_NUM_1 1
#define UART_NUM_2 2
#define SOC_UART_NUM 3

#define SOC_SPI_PERIPH_NUM 3
#define SOC_I2C_PERIPH_NUM 2
#define SOC_LEDC_TIMER_NUM 8
#define SOC_ADC_CHANNEL_NUM 10

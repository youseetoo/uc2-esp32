#pragma once
// T4.1: pure-C interrupt controller (was a C++ namespace with std::map).
// Listeners are stored in a fixed array indexed by pin (ESP32-S3 has < 48 GPIOs).
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
// T1.3: direct IDF (no Arduino) — gpio_int_type_t comes from the IDF gpio
// driver (same header uc2_gpio.cpp uses).
#include <driver/gpio.h>

#define IC_MAX_LISTENERS 64

// callback signature: receives the pin that fired
typedef void (*ic_listener_t)(uint8_t pin);

void interrupt_controller_init(void);
void interrupt_controller_add_listener(uint8_t pin, ic_listener_t listener,
                                       gpio_int_type_t int_type);
void interrupt_controller_remove_listener(uint8_t pin);

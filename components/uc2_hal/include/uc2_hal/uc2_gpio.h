// T1.3 (no Arduino): uc2_gpio — GPIO + digital interrupt façade over the
// IDF GPIO driver. API surface = exactly what main/ uses (grep-verified):
// pinMode (26), digitalWrite (44), digitalRead (8), attachInterrupt (1:
// FanController tacho, FALLING), detachInterrupt (0), digitalPinToInterrupt
// (1), noInterrupts/interrupts (1/1).
//
// Interrupts: a single lazily-installed ISR service + one shared handler
// walks a small pin→callback table (attachInterrupt ≤ 8 pins). Callbacks run
// in ISR context — keep them short, mark IRAM_ATTR if the body needs flash
// access.
//
// C-compatible (stdint, not cstdint; extern "C" guards): included from C
// modules (T4.1 State.c). The guards also switch the C++ definitions in
// uc2_gpio.cpp to plain C linkage — every caller sees the same header, so all
// C++ call sites keep resolving.
#pragma once

#include <stdint.h>

#include <freertos/FreeRTOS.h>

#ifdef __cplusplus
extern "C" {
#endif

// Arduino-compatible constants (values from esp32-hal).
#define INPUT         0x01
#define OUTPUT        0x03
#define INPUT_PULLUP  0x04
#define INPUT_PULLDOWN 0x05  // DigitalInController E-stop / endstops
#define HIGH          0x01
#define LOW           0x00
#define FALLING       1
#define RISING        2
#define CHANGE        3

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
int digitalRead(uint8_t pin);

// On ESP32 pins are their own interrupts.
static inline int digitalPinToInterrupt(uint8_t pin) { return (int)pin; }

// Edge-triggered callback (ISR context). No-op if the pin is invalid or the
// table is full.
void attachInterrupt(uint8_t pin, void (*fn)(void), uint8_t edge);
// Reverts the pin's interrupt type to NONE and removes it from the table.
void detachInterrupt(uint8_t pin);

// Critical-section shorthand (task context only).
static inline void noInterrupts() { portDISABLE_INTERRUPTS(); }
static inline void interrupts()   { portENABLE_INTERRUPTS(); }

#ifdef __cplusplus
}
#endif

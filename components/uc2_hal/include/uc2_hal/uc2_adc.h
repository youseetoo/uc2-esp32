// T2.3: Arduino-compatible ADC façade over ESP-IDF 6.1 esp_adc (oneshot mode).
// API surface = what GpioCanSlave.cpp uses (analogRead, analogReadResolution,
// analogSetPinAttenuation) + the attenuation constants (ADC_11db, etc.).
//
// Design:
//   * Lazy init: first analogSetPinAttenuation() or analogRead() call initialises
//     whichever ADC unit (1 or 2) the pin belongs to.
//   * Per-pin attenuation is stored in a small table (max 16 pins) and applied
//     at configure time.
//   * analogReadResolution() stores the desired bit width (Arduino semantics:
//     value is returned in 0..2^bits-1; default 12 on S3).
//   * Values are scaled to the requested resolution (Arduino-ESP32 behaviour).
#pragma once

#include <cstdint>

// ── Arduino-ESP32 attenuation constants (value = index, NOT dB) ────────────
#define ADC_11db  0   // max attenuation (12 dB on S3)
#define ADC_6db   1
#define ADC_2_5db 2
#define ADC_0db   3   // no attenuation

int  analogRead(int pin);
void analogReadResolution(int bits);
void analogSetPinAttenuation(int pin, int attenuation);

// eFuse-calibrated millivolt read (Arduino-ESP32 analogReadMilliVolts: per-channel
// curve-fitting scheme on S3). Falls back to a naive full-scale scale when the
// calibration eFuse is absent.
int  analogReadMilliVolts(int pin);

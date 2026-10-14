// T2.9: pins_arduino.h shim — only needed in the #else (ARDUINO < 100) branch of
// OneWire.h. digitalPinToBitMask is the only symbol used by OneWire's old path.
// On ESP32 the pin number is its own bit mask, so we return it directly.
#pragma once

static inline uint8_t digitalPinToBitMask(uint8_t pin) { return pin; }

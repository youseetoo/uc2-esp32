// T2.6: AccelStepper raw-IDF shim. The vendored youseeetoo/AccelStepper takes its
// non-Arduino branch (#include <WProgram.h> / <wiring.h>) under raw ESP-IDF
// (ARDUINO is not defined >= 100). Map onto uc2_hal's global Arduino-compat layer:
//   pinMode/digitalWrite/digitalRead + HIGH/LOW/INPUT/OUTPUT/INPUT_PULLUP
//       -> uc2_hal/uc2_gpio.h
//   delay/delayMicroseconds/millis/micros
//       -> uc2_hal/uc2_time.h
// (all already global-scope, so the library's unqualified calls resolve as-is).
#pragma once
#include "uc2_hal.h"

// Arduino typedef the library uses (raw IDF has no `boolean`); Arduino-ESP32
// defines it as an alias for bool.
typedef bool boolean;

// max/min for the library. (constrain / sqrt / fabs come from uc2_misc.h via the
// uc2_hal.h include above, so they are NOT redefined here — that would be a
// redefinition error.)
template <typename T>
const T& max(const T& a, const T& b) { return (a > b) ? a : b; }
template <typename T>
const T& min(const T& a, const T& b) { return (a < b) ? a : b; }

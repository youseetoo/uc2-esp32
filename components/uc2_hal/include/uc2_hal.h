#pragma once
// uc2_hal — umbrella include.
// Thin C++ façades over direct ESP-IDF 6.1 calls (no Arduino, no shim).
// main/ modules include <uc2_hal.h> (or a specific sub-header) during the T1.3
// conversion; everything else calls the IDF drivers directly.
// See components/uc2_hal/CMakeLists.txt +
// DOCS/PORT_PLAN_STEPS/refs/arduino-idf-mapping.md (API table).
//
// T4.1 dual-language: the C++-only sub-headers (uc2_esp, uc2_prefs,
// uc2_serial, uc2_string, uc2_wire — classes/namespaces) and the using
// aliases are pulled in only for C++ TUs. Pure-C TUs (via PinConfig.h etc.)
// get the C-safe subset instead.
#include "uc2_hal/uc2_time.h"
#include "uc2_hal/uc2_log.h"
#include "uc2_hal/uc2_gpio.h"
#include "uc2_hal/uc2_ledc.h"
#include "uc2_hal/uc2_misc.h"
#include "uc2_hal/uc2_adc.h"

#ifdef __cplusplus
#include "uc2_hal/uc2_esp.h"
#include "uc2_hal/uc2_prefs.h"
#include "uc2_hal/uc2_serial.h"
#include "uc2_hal/uc2_string.h"
#include "uc2_hal/uc2_wire.h"

// Arduino-compat name: ported app code (uc2_motor, main/) declares
// `Preferences prefs;` unqualified, as Arduino.h did. Alias uc2::Preferences
// at global scope so those files compile without per-file edits.
using uc2::Preferences;
// Arduino-compat name: ported app code uses `String` unqualified (Arduino.h
// provided it at global scope). Alias uc2::String so those files compile.
using uc2::String;
#else // !__cplusplus — pure C branch
// C-facing serial/wire shims (the C++ classes above are unavailable in C).
#include "uc2_hal/uc2_serial_c.h"
#include "uc2_hal/uc2_wire_c.h"
#endif // __cplusplus

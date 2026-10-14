#pragma once
// uc2_hal — umbrella include.
// Thin C++ façades over direct ESP-IDF 6.1 calls (no Arduino, no shim).
// main/ modules include <uc2_hal.h> (or a specific sub-header) during the T1.3
// conversion; everything else calls the IDF drivers directly.
// See components/uc2_hal/CMakeLists.txt +
// DOCS/PORT_PLAN_STEPS/refs/arduino-idf-mapping.md (API table).
#include "uc2_hal/uc2_time.h"
#include "uc2_hal/uc2_log.h"
#include "uc2_hal/uc2_gpio.h"
#include "uc2_hal/uc2_esp.h"
#include "uc2_hal/uc2_prefs.h"
#include "uc2_hal/uc2_serial.h"
#include "uc2_hal/uc2_string.h"
#include "uc2_hal/uc2_wire.h"
#include "uc2_hal/uc2_ledc.h"
#include "uc2_hal/uc2_misc.h"
#include "uc2_hal/uc2_adc.h"

// Arduino-compat name: ported app code (uc2_motor, main/) declares
// `Preferences prefs;` unqualified, as Arduino.h did. Alias uc2::Preferences
// at global scope so those files compile without per-file edits.
using uc2::Preferences;
// Arduino-compat name: ported app code uses `String` unqualified (Arduino.h
// provided it at global scope). Alias uc2::String so those files compile.
using uc2::String;

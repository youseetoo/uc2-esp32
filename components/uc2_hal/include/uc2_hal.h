#pragma once
// uc2_hal — umbrella include.
// Thin C++ façades over direct ESP-IDF 6.1 calls (no Arduino, no shim).
// main/ modules include <uc2_hal.h> (or a specific sub-header) during the T1.3
// conversion; everything else calls the IDF drivers directly.
// See components/uc2_hal/CMakeLists.txt +
// DOCS/PORT_PLAN_STEPS/refs/arduino-idf-mapping.md (API table).
#include "uc2_hal/uc2_time.h"
#include "uc2_hal/uc2_log.h"
#include "uc2_hal/uc2_esp.h"
#include "uc2_hal/uc2_prefs.h"
#include "uc2_hal/uc2_serial.h"
#include "uc2_hal/uc2_string.h"
#include "uc2_hal/uc2_wire.h"

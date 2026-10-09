// Host stub: esp_debug_helpers.h — this header is missing from the repo, yet
// FocusMotor.cpp / i2c_master.cpp / Ws2812Rmt.cpp include it. The log_* macros
// it would normally provide are already defined in Arduino.h, so this is a thin
// pass-through that just guarantees the logging surface is in scope.
#pragma once
#include "Arduino.h"

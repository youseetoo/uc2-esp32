// T2.9: Arduino API shim for onewire_ds18b20 (OneWire + DallasTemperature).
// Maps the minimal Arduino I/O and time API onto uc2_hal.
#pragma once

#include <cstdint>
#include "WProgram.h"   // constrain, boolean, max/min, yield (includes uc2_hal)
#include "pgmspace.h"   // PROGMEM, pgm_read_byte etc.
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace uc2 {}

typedef uc2::Serial_ Stream;
typedef uc2::Serial_ HardwareSerial;

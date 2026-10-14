// T2.6: TMCStepper raw-IDF port shim. Provides the Arduino API surface the
// vendored teemuatlut/TMCStepper v0.7.3 library needs, mapped onto uc2_hal:
//   Stream / HardwareSerial -> uc2::Serial_ (uc2_hal/uc2_serial.h, the UART facade
//                               with begin/read/available/write/flush/print/println/setTimeout)
//   Serial / Serial1        -> the uc2_hal global uc2::Serial_ objects
//   digitalWrite/Read/pinMode, delay/millis/micros -> uc2_hal (uc2_gpio.h / uc2_time.h)
// The TMC2209 is driven over UART in the accelmotor (TMCController uses `&Serial1`,
// a Stream*), so Stream = uc2::Serial_ is the key mapping. SW_CAPABLE_PLATFORM stays
// false (no SoftwareSerial.h here), so the software-serial ctors are `= delete`.
#pragma once

#include "uc2_hal.h"   // umbrella: gpio, time, serial, string, log, wire, i2c, esp

namespace uc2 {
}  // (Serial_/Stream_ already in uc2 via uc2_hal; the typedefs below are global)

// The library refers to Stream / HardwareSerial unqualified (global scope).
typedef uc2::Serial_ Stream;
typedef uc2::Serial_ HardwareSerial;

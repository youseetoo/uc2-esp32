#pragma once
// T2.1: Arduino pgmspace.h shim for raw IDF. PROGMEM is a no-op on ESP32 (SRAM,
// no separate program space). Provided so headers that #include <pgmspace.h>
// (e.g. uc2_app/JsonKeys.h) resolve without Arduino.

#ifndef PROGMEM
#define PROGMEM
#endif

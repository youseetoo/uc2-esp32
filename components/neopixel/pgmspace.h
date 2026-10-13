// pgmspace.h shim for raw ESP-IDF 6.x (replaces the Arduino header).
// On the ESP32 the flash is memory-mapped, so PROGMEM is a no-op attribute and
// pgm_read_* are plain pointer dereferences. The Adafruit NeoPixel driver
// (Adafruit_NeoPixel.h/.cpp) includes <pgmspace.h> for these; this shim makes
// it compile under raw IDF without the Arduino core.
#pragma once
#include <stdint.h>

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef PGM_P
#define PGM_P const char *
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr)  (*(const uint8_t *)(addr))
#endif

#ifndef pgm_read_word
#define pgm_read_word(addr)  (*(const uint16_t *)(addr))
#endif

#ifndef pgm_read_dword
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))
#endif

#ifndef pgm_read_ptr
#define pgm_read_ptr(addr)   (*(const void *const *)(addr))
#endif

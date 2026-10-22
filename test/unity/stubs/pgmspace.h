// Host stub: pgmspace.h — flash-program-space qualifiers are no-ops on host.
// Must parse as C (included from C TUs via JsonKeys.h) as well as C++.
#pragma once
#include <stdint.h>
#include <string.h>

// MSVC has no GCC __attribute__ built-in; production headers (JsonKeys.h,
// Endpoints.h) rely on it. Must be defined here because pgmspace.h is
// included before Arduino.h in several TUs.
#ifdef _MSC_VER
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#define PROGMEM
#define PGM_P const char*
#define PSTR(s) (s)

static inline uint8_t pgm_read_byte(const void* addr) { return *(const uint8_t*)(addr); }
static inline uint16_t pgm_read_word(const void* addr) { return *(const uint16_t*)(addr); }
static inline uint32_t pgm_read_dword(const void* addr) { return *(const uint32_t*)(addr); }
static inline const char* pgm_read_string(const char* addr) { return addr; }
static inline float pgm_read_float(const void* addr) { return *(const float*)(addr); }
static inline void pgm_read_word_array(uint16_t* dest, const void* src, int n) {
    for (int i = 0; i < n; i++) dest[i] = pgm_read_word((const uint8_t*)src + i * 2);
}

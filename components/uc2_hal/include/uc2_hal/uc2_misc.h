// T1.3 (no Arduino): Arduino utility functions used in main/ (grep-verified
// 2026-10-12): random(min,max) (LaserController, LedController), map
// (LaserController servo, DialController — dial path compile-out in most
// combos).
//
// T4.1: dual-language — C++ keeps the original inline/template API; pure-C
// consumers (LaserController.c) get static inline equivalents.
#pragma once

#ifdef __cplusplus
#include <cstdint>

#include "esp_random.h"  // esp_random (IDF >= 6.1: esp_hw_support; was esp_system/esp_random.h)

// Arduino random(min, max): uniform in [min, max] (both inclusive).
inline long random(long min, long max)
{
    if (max < min)
    {
        const long t = min;
        min = max;
        max = t;
    }
    return min + static_cast<long>(esp_random() % static_cast<unsigned long>(max - min + 1L));
}

// C-facing alias: pure-C modules compiled as C++ (e.g. LedController.c, which
// the host test #includes into a .cpp) call uc2_random — the name the C branch
// (below) exposes. The C branch cannot reuse `random` (picolibc stdlib.h
// declares `long random(void)`), so dual-compiled .c files use uc2_random in
// both; here it just forwards to the C++ `random`.
inline long uc2_random(long min, long max)
{
    return random(min, max);
}

// Arduino map: linear remap, integer arithmetic, no clamping (Arduino
// semantics).
inline long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Arduino math helpers (constrain + the std math fns the ported modules use
// unqualified, as Arduino.h provided). Shared here so every component gets them.
#include <cmath>
using std::fabs;
using std::fabsf;
using std::sqrt;

// Arduino constrain: clamp x into [lo, hi].
template <typename T>
inline T constrain(T x, T lo, T hi)
{
    return (x < lo) ? lo : ((x > hi) ? hi : x);
}

#else // !__cplusplus — pure C branch
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "esp_random.h"  // esp_random (IDF >= 6.1: esp_hw_support; was esp_system/esp_random.h)

// Arduino random(min, max): uniform in [min, max] (both inclusive).
// Named uc2_random in C: picolibc stdlib.h declares `long random(void)`,
// which would conflict (C has no overloading — the C++ branch keeps `random`).
static inline long uc2_random(long min, long max)
{
    if (max < min)
    {
        const long t = min;
        min = max;
        max = t;
    }
    return min + (long)(esp_random() % (unsigned long)(max - min + 1L));
}

// Arduino map: linear remap, integer arithmetic, no clamping.
static inline long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Arduino constrain: clamp x into [lo, hi].
#define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#endif // __cplusplus

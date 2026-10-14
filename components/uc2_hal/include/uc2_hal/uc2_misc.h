// T1.3 (no Arduino): Arduino utility functions used in main/ (grep-verified
// 2026-10-12): random(min,max) (LaserController, LedController), map
// (LaserController servo, DialController — dial path compile-out in most
// combos).
#pragma once

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

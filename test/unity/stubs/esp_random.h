// Host stub: esp_random.h
//
// Deterministic LCG (same sequence as esp_random() in host_impl.cpp) so tests
// are reproducible.
#pragma once
#include <stdint.h>

static inline uint32_t esp_random(void)
{
    static uint32_t s = 0x12345678;
    s = s * 1103515245u + 12345u;
    return s >> 8;
}

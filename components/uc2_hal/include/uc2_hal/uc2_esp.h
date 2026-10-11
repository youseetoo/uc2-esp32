// T1.3 (no Arduino): ESP.* → direct IDF calls.
// Only the three members used in main/ (grep-verified 2026-10-11:
// restart 8, getFreeHeap 7, getCycleCount 4 — State, NVSConfig, Ws2812Rmt,
// main, CANopenModule, SerialProcess).
// NOTE: getCycleCount must return CPU CYCLES (Ws2812Rmt compares against
// 480000u ≈ 2 ms @ 240 MHz) — esp_timer µs would be off by ~240×.
#pragma once

#include <cstdint>

#include "esp_cpu.h"        // esp_cpu_get_cyclecount (esp_hw_support in 6.1)
#include "esp_heap_caps.h"  // esp_get_free_heap_size
#include "esp_system.h"     // esp_restart

class EspClass
{
public:
    void restart()
    {
        esp_restart();
    }

    uint32_t getFreeHeap()
    {
        return static_cast<uint32_t>(esp_get_free_heap_size());
    }

    uint32_t getCycleCount()
    {
        return static_cast<uint32_t>(esp_cpu_get_cycle_count());
    }
};

extern const EspClass ESP;

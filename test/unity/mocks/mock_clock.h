#pragma once
#include <cstdint>

// Deterministic mock clock for unit tests.
// Override millis() when UNIT_TEST is defined.
namespace MockClock {
    extern uint32_t currentMs;
    void reset();
    void advanceMs(uint32_t ms);
    uint32_t getMs();
}

#ifdef UNIT_TEST
    #undef millis
    #define millis() MockClock::getMs()
#endif

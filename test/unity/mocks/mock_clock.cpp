#include "mock_clock.h"

namespace MockClock {
    uint32_t currentMs = 0;

    void reset() {
        currentMs = 0;
    }

    void advanceMs(uint32_t ms) {
        currentMs += ms;
    }

    uint32_t getMs() {
        return currentMs;
    }
}

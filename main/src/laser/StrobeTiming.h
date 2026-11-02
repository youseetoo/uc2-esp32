#pragma once
// Strobe timing rules. Pure C++11, host-testable (test/native/test_strobe_timing.cpp).
// LaserStrobe.cpp applies them on the illumination node; the limits are
// enforced on the node itself, never only by the host.
#include <stdint.h>

namespace StrobeTiming
{
    struct Limits
    {
        uint32_t maxWidthUs;      // longest flash the node will produce
        uint32_t maxDelayUs;      // longest SYNC-to-flash delay
        uint16_t maxDutyPermille; // flash-on time per flash spacing, 1..1000
    };

    inline uint32_t clampWidth(uint32_t widthUs, const Limits &L)
    {
        if (widthUs < 1)
            widthUs = 1;
        return widthUs > L.maxWidthUs ? L.maxWidthUs : widthUs;
    }

    inline uint32_t clampDelay(uint32_t delayUs, const Limits &L)
    {
        return delayUs > L.maxDelayUs ? L.maxDelayUs : delayUs;
    }

    // Shortest spacing between two flash starts that keeps width/spacing at or
    // below the duty limit (rounded up, so the limit is never exceeded).
    inline uint32_t minSpacingUs(uint32_t widthUs, uint16_t maxDutyPermille)
    {
        if (maxDutyPermille == 0 || maxDutyPermille >= 1000)
            return widthUs;
        return (uint32_t)(((uint64_t)widthUs * 1000u + maxDutyPermille - 1u) / maxDutyPermille);
    }

    // Time left until the flash after the node spent (now - tSync) handling
    // the SYNC. Subtracting it keeps our own processing out of the jitter.
    inline uint32_t remainingDelay(uint32_t delayUs, int64_t tSyncUs, int64_t nowUs)
    {
        const int64_t elapsed = nowUs - tSyncUs;
        if (elapsed <= 0)
            return delayUs;
        if (elapsed >= (int64_t)delayUs)
            return 0;
        return delayUs - (uint32_t)elapsed;
    }

    // Rejects a flash whose start is closer to the previous one than the duty
    // limit allows. A rejected flash does not move the reference point.
    struct DutyLimiter
    {
        bool any = false;
        int64_t lastStartUs = 0;

        bool allow(int64_t startUs, uint32_t widthUs, uint16_t maxDutyPermille)
        {
            if (any && startUs - lastStartUs < (int64_t)minSpacingUs(widthUs, maxDutyPermille))
                return false;
            any = true;
            lastStartUs = startUs;
            return true;
        }

        void reset() { any = false; }
    };

    enum class Change
    {
        None,    // request equals the current state
        Enable,  // strobe switched on: take the pin from PWM
        Disable, // strobe switched off: give the pin back to PWM
        Update   // still on, new delay/width
    };

    struct Channel
    {
        bool enabled = false;
        uint32_t delayUs = 0;
        uint32_t widthUs = 20;
        uint32_t fired = 0;
        uint32_t skipped = 0;
        DutyLimiter duty;
    };

    // Applies a configuration request with clamping. Enabling resets the
    // counters and the duty reference, so every sweep starts from zero.
    inline Change apply(Channel &c, bool enable, uint32_t delayUs, uint32_t widthUs, const Limits &L)
    {
        const uint32_t d = clampDelay(delayUs, L);
        const uint32_t w = clampWidth(widthUs, L);
        if (!enable)
        {
            if (!c.enabled)
                return Change::None;
            c.enabled = false;
            return Change::Disable;
        }
        if (!c.enabled)
        {
            c.enabled = true;
            c.delayUs = d;
            c.widthUs = w;
            c.fired = 0;
            c.skipped = 0;
            c.duty.reset();
            return Change::Enable;
        }
        if (c.delayUs == d && c.widthUs == w)
            return Change::None;
        c.delayUs = d;
        c.widthUs = w;
        return Change::Update;
    }
}

#include "AxisWatchdog.h"

namespace AxisWatchdog
{
    void reset(State &s)
    {
        s.wasExpected = false;
        s.motionStartMs = 0;
        s.lastProgressMs = 0;
        s.lastProgressCnt = 0;
    }

    Trip update(const Config &cfg, State &s, bool motionExpected,
                int32_t commandedStepsPerSec, int64_t rawCounts,
                int32_t positionErrorSteps, uint32_t nowMs)
    {
        if (!cfg.enabled)
        {
            s.wasExpected = motionExpected;
            return TRIP_NONE;
        }

        // Gate: only judge while the backend reports motion AND the commanded
        // speed is above the floor. Below the floor (e.g. accel ramps that move
        // less than one encoder count per sample) we cannot distinguish stall
        // from slow motion, so we don't try.
        bool active = motionExpected && (abs(commandedStepsPerSec) >= cfg.minSpeedStepsPerSec);

        if (!active)
        {
            // Idle / too slow to judge — arm for the next motion start.
            s.wasExpected = false;
            return TRIP_NONE;
        }

        // Rising edge of "actively moving": (re)start the grace + progress clocks.
        if (!s.wasExpected)
        {
            s.wasExpected = true;
            s.motionStartMs = nowMs;
            s.lastProgressMs = nowMs;
            s.lastProgressCnt = rawCounts;
        }

        // ---- LOST_STEPS: moving but persistently behind ---------------------
        // Independent of the stall test — catches slipping (encoder advancing,
        // just not enough) which the stall test would miss.
        if ((int32_t)abs(positionErrorSteps) > cfg.lagLimitSteps)
            return TRIP_LOST_STEPS;

        // ---- STALL: no encoder progress for stallTimeoutMs ------------------
        int64_t moved = rawCounts - s.lastProgressCnt;
        if ((moved < 0 ? -moved : moved) >= (int64_t)cfg.noiseThresholdCounts)
        {
            // Real progress — reset the stall clock.
            s.lastProgressCnt = rawCounts;
            s.lastProgressMs = nowMs;
            return TRIP_NONE;
        }

        // Ramp guard: allow a short grace window after motion start so the
        // initial acceleration ramp (below encoder resolution) never trips.
        if ((nowMs - s.motionStartMs) < cfg.graceMs)
            return TRIP_NONE;

        if ((nowMs - s.lastProgressMs) >= cfg.stallTimeoutMs)
            return TRIP_STALL;

        return TRIP_NONE;
    }
}

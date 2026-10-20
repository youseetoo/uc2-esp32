// T4.1: AxisWatchdog — pure C (was a C++ namespace; the module is pure logic).
static const char *TAG = "AxisWatchdog";
#include "AxisWatchdog.h"
#include "uc2_hal/uc2_log.h"
#include <stdlib.h> // abs()

void axis_wd_reset(AxisWatchdogState *s)
{
    s->wasExpected = false;
    s->motionStartMs = 0;
    s->lastProgressMs = 0;
    s->lastProgressCnt = 0;
}

AxisWatchdogTrip axis_wd_update(const AxisWatchdogConfig *cfg, AxisWatchdogState *s,
                                bool motionExpected, int32_t commandedStepsPerSec,
                                int64_t rawCounts, int32_t positionErrorSteps,
                                uint32_t nowMs)
{
    if (!cfg->enabled)
    {
        s->wasExpected = motionExpected;
        return AW_TRIP_NONE;
    }

    // Gate: only judge while the backend reports motion AND the commanded
    // speed is above the floor. Below the floor (e.g. accel ramps that move
    // less than one encoder count per sample) we cannot distinguish stall
    // from slow motion, so we don't try.
    bool active = motionExpected && (abs(commandedStepsPerSec) >= cfg->minSpeedStepsPerSec);

    if (!active)
    {
        // Idle / too slow to judge — arm for the next motion start.
        s->wasExpected = false;
        return AW_TRIP_NONE;
    }

    // Rising edge of "actively moving": (re)start the grace + progress clocks.
    if (!s->wasExpected)
    {
        s->wasExpected = true;
        s->motionStartMs = nowMs;
        s->lastProgressMs = nowMs;
        s->lastProgressCnt = rawCounts;
    }

    // ---- LOST_STEPS: moving but persistently behind ---------------------
    // Independent of the stall test — catches slipping (encoder advancing,
    // just not enough) which the stall test would miss.
    if ((int32_t)abs(positionErrorSteps) > cfg->lagLimitSteps) {
        log_i("AxisWatchdog: LOST_STEPS trip (posErr=%d > lagLimit=%d)", (int32_t)abs(positionErrorSteps), cfg->lagLimitSteps);
        return AW_TRIP_LOST_STEPS;
    }

    // ---- STALL: no encoder progress for stallTimeoutMs ------------------
    int64_t moved = rawCounts - s->lastProgressCnt;
    if ((moved < 0 ? -moved : moved) >= (int64_t)cfg->noiseThresholdCounts)
    {
        // Real progress — reset the stall clock.
        log_i("AxisWatchdog: progress detected (moved=%lld counts)", (long long)moved);
        s->lastProgressCnt = rawCounts;
        s->lastProgressMs = nowMs;
        return AW_TRIP_NONE;
    }

    // Ramp guard: allow a short grace window after motion start so the
    // initial acceleration ramp (below encoder resolution) never trips.
    if ((nowMs - s->motionStartMs) < cfg->graceMs)
        return AW_TRIP_NONE;

    if ((nowMs - s->lastProgressMs) >= cfg->stallTimeoutMs) {
        log_i("AxisWatchdog: STALL trip (no progress for %u ms)", (unsigned)cfg->stallTimeoutMs);
        return AW_TRIP_STALL;
    }

    return AW_TRIP_NONE;
}

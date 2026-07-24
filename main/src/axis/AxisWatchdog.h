#pragma once
#include <Arduino.h>

// ============================================================================
// AxisWatchdog — fast, cheap blocked/slipping-axis detection (design v2, WP4).
//
// Independent of any PID so it protects EVERY mode, including OPEN_LOOP. It only
// reads counters + timers, so it is safe to run every motor-loop iteration.
//
// This is pure logic (no FocusMotor / CAN dependency): AxisController feeds it
// samples and acts on the returned trip (stop + latch + notify). Thresholds are
// derived from the measured encoder noise (AxisCalibration.residualScatter),
// not hardcoded constants.
// ============================================================================

namespace AxisWatchdog
{
    enum Trip : uint8_t
    {
        TRIP_NONE = 0,
        TRIP_STALL = 1,      // encoder not progressing while motion is expected
        TRIP_LOST_STEPS = 2  // moving but position error exceeds the lag limit
    };

    struct Config
    {
        uint16_t noiseThresholdCounts = 3;   // "progress" must exceed this (from scatter)
        int32_t  lagLimitSteps        = 200; // |posErr| beyond this -> LOST_STEPS
        uint32_t stallTimeoutMs       = 80;  // no progress this long -> STALL
        uint32_t graceMs              = 60;  // ignore stall right after motion start
        int32_t  minSpeedStepsPerSec  = 200; // only judge when commanded speed exceeds this
        bool     enabled              = true;
    };

    struct State
    {
        bool     wasExpected      = false; // motion-expected edge tracking
        uint32_t motionStartMs    = 0;     // when motion last became expected
        uint32_t lastProgressMs   = 0;     // last time the encoder advanced
        int64_t  lastProgressCnt  = 0;     // encoder count at last progress
    };

    // Reset the transient state (call on resetAxis / new move).
    void reset(State &s);

    // Evaluate one sample. Returns the trip that fired this tick (TRIP_NONE if
    // healthy). Latching is the caller's responsibility.
    //   motionExpected     : stepper backend reports the axis running
    //   commandedStepsPerSec : |commanded speed| (steps/s)
    //   rawCounts          : current signed encoder count
    //   positionErrorSteps : measured - commanded (steps)
    Trip update(const Config &cfg, State &s, bool motionExpected,
                int32_t commandedStepsPerSec, int64_t rawCounts,
                int32_t positionErrorSteps, uint32_t nowMs);
}

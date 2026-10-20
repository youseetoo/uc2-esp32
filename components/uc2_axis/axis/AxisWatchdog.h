#pragma once
// T4.1: pure-C (was a C++ namespace; the module is pure logic — no C++ deps).
#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// AxisWatchdog — fast, cheap blocked/slipping-axis detection.
//
// Independent of any PID so it protects EVERY mode, including OPEN_LOOP. It only
// reads counters + timers, so it is safe to run every motor-loop iteration.
//
// This is pure logic (no FocusMotor / CAN dependency): AxisController feeds it
// samples and acts on the returned trip (stop + latch + notify). Thresholds are
// derived from the measured encoder noise (AxisCalibration.residualScatter),
// not hardcoded constants.
// ============================================================================

typedef enum AxisWatchdogTrip {
    AW_TRIP_NONE = 0,
    AW_TRIP_STALL = 1,      // encoder not progressing while motion is expected
    AW_TRIP_LOST_STEPS = 2  // moving but position error exceeds the lag limit
} AxisWatchdogTrip;

// Caller fills this (AxisController::configureWatchdog derives the values from
// measured scatter); no in-struct defaults — C.
typedef struct AxisWatchdogConfig {
    uint16_t noiseThresholdCounts; // "progress" must exceed this (from scatter)
    int32_t  lagLimitSteps;        // |posErr| beyond this -> LOST_STEPS
    uint32_t stallTimeoutMs;       // no progress this long -> STALL
    uint32_t graceMs;              // ignore stall right after motion start
    int32_t  minSpeedStepsPerSec;  // only judge when commanded speed exceeds this
    bool     enabled;
} AxisWatchdogConfig;

typedef struct AxisWatchdogState {
    bool     wasExpected;      // motion-expected edge tracking
    uint32_t motionStartMs;    // when motion last became expected
    uint32_t lastProgressMs;   // last time the encoder advanced
    int64_t  lastProgressCnt;  // encoder count at last progress
} AxisWatchdogState;

// Reset the transient state (call on resetAxis / new move).
void axis_wd_reset(AxisWatchdogState *s);

// Evaluate one sample. Returns the trip that fired this tick (AW_TRIP_NONE if
// healthy). Latching is the caller's responsibility.
//   motionExpected     : stepper backend reports the axis running
//   commandedStepsPerSec : |commanded speed| (steps/s)
//   rawCounts          : current signed encoder count
//   positionErrorSteps : measured - commanded (steps)
AxisWatchdogTrip axis_wd_update(const AxisWatchdogConfig *cfg, AxisWatchdogState *s,
                                bool motionExpected, int32_t commandedStepsPerSec,
                                int64_t rawCounts, int32_t positionErrorSteps,
                                uint32_t nowMs);

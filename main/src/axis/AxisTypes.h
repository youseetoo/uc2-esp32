#pragma once
#include <Arduino.h>

// ============================================================================
// axis/ module — clean-slate closed-loop / feedback layer (design v2).
//
// UNIT POLICY (constraint #2): every value exposed by this module is in STEPS.
// Encoder counts are internal and appear only in the explicitly-named
// diagnostic fields (rawCounts / countsPerStep). All conversions live in
// AxisCalibration; nothing above this layer ever sees counts.
// ============================================================================

// Position/behaviour mode of a single axis.
enum AxisMode : uint8_t
{
    MODE_OPEN_LOOP = 0, // exactly today's behaviour; done == steps emitted
    MODE_MONITOR   = 1, // move open-loop, compare, notify — never corrects
    MODE_CORRECT   = 2, // move open-loop, verify, bounded correction, adopt encoder
    MODE_SERVO     = 3  // PID velocity loop drives to target in count-space
};

// Coarse health summary for a quick host-side red/yellow/green.
enum AxisHealth : uint8_t
{
    HEALTH_OK       = 0,
    HEALTH_DEGRADED = 1, // still moving but out of spec (e.g. correction failed)
    HEALTH_FAULT    = 2  // latched fault; motion refused until resetAxis()
};

// Specific fault cause. NONE means healthy.
enum AxisFault : uint8_t
{
    FAULT_NONE       = 0,
    FAULT_STALL      = 1, // encoder not moving while motion expected
    FAULT_LOST_STEPS = 2, // moving but persistently behind commanded position
    FAULT_DIVERGENCE = 3, // error exceeded threshold and could not be corrected
    FAULT_TIMEOUT    = 4, // motion did not complete in the allotted time
    FAULT_CAL_INVALID = 5, // calibration stale (e.g. microsteps changed) — warning
    FAULT_CAL_FAILED = 6, // calibration routine could not measure the relation
    FAULT_ENC_NOISE  = 7  // encoder readings inconsistent beyond tolerance
};

// Reset policy for resetAxis(). Determines which side becomes truth again.
enum AxisResetPolicy : uint8_t
{
    RESET_TRUST_ENCODER = 0, // adjust commanded steps so error becomes zero
    RESET_TRUST_STEPS   = 1, // zero the encoder to match the step counter
    RESET_FORCE_REHOME  = 2  // clear everything; caller must re-home
};

// Live per-axis feedback. All step-based (see UNIT POLICY above).
struct AxisFeedback
{
    int32_t commandedSteps    = 0; // FAS position (authority in OPEN_LOOP)
    int32_t measuredSteps     = 0; // encoder counts -> steps via calibration
    int32_t positionErrorSteps = 0; // measured - commanded, relative to origin
    int64_t rawCounts         = 0; // diagnostics only (raw encoder count)
    uint8_t mode   = MODE_OPEN_LOOP;
    uint8_t health = HEALTH_OK;
    uint8_t fault  = FAULT_NONE;
    bool    calibrated = false; // valid sign+scale for the current microstepping
    bool    referenced = false; // homed (absolute moves allowed)
};

// Persisted calibration record (NVS). countsPerStep is a MAGNITUDE in Q16.16;
// the sign of the step<->count relation lives in countSign.
struct AxisCalibration
{
    int32_t  countsPerStep_q16 = 0; // |counts per step|, Q16.16 fixed point
    int8_t   countSign         = 1; // +1 / -1 : step dir vs count dir
    int32_t  backlashCounts    = 0; // measured reversal slack (counts)
    uint16_t microstepsAtCal   = 0; // microstep setting the cal was taken at
    uint16_t residualScatter   = 0; // measured noise (counts) -> sets tolerances
    uint32_t timestamp         = 0; // millis() at calibration
    uint8_t  quality           = 0; // 0..100, from regression R^2
    bool     valid             = false; // false until a successful calibration
    bool     derived           = false; // true if rescaled from a different microstep
};

// ---- fixed-point helpers (Q16.16) --------------------------------------------
// deltaCounts -> deltaSteps, honouring sign. Returns 0 if uncalibrated.
static inline int32_t axisCountsToSteps(const AxisCalibration &c, int64_t deltaCounts)
{
    if (c.countsPerStep_q16 == 0)
        return 0;
    // steps = countSign * counts / |countsPerStep|
    int64_t num = deltaCounts * 65536LL * (int64_t)c.countSign;
    return (int32_t)(num / (int64_t)c.countsPerStep_q16);
}

// deltaSteps -> deltaCounts, honouring sign.
static inline int64_t axisStepsToCounts(const AxisCalibration &c, int32_t deltaSteps)
{
    int64_t mag = ((int64_t)deltaSteps * (int64_t)c.countsPerStep_q16) / 65536LL;
    return mag * (int64_t)c.countSign;
}

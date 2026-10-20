#pragma once
// T4.1: pure C (was a C++ namespace with std::function hooks; the hooks are
// now plain function pointers + a void* ctx for the captured axis).
#include <stdbool.h>
#include <stdint.h>
#include "AxisTypes.h"

// ============================================================================
// AxisCalibration — measures the relationship between commanded steps and
// encoder counts (design v2, WP3). This is the load-bearing WP: it turns the
// four unknowns (sign, scale, offset, backlash) into measured numbers and
// derives the detection thresholds (residualScatter) from real noise instead
// of hardcoded constants.
//
// The routine is transport-agnostic: the caller supplies hooks that command
// blocking relative moves and read the encoder. All external values are STEPS.
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Hooks the routine uses to drive one axis. Provided by AxisController; every
// callback receives the ctx it was registered with (the axis index, packed).
typedef struct AxisCalibrationHooks {
    void *ctx;
    // Current FAS commanded step position.
    int32_t (*get_step_pos)(void *ctx);
    // Move deltaSteps at |speed|, block until the motor stops. Return false
    // if aborted (endstop / e-stop / timeout) so the routine can bail out.
    bool (*move_rel_blocking)(void *ctx, int32_t deltaSteps, int32_t speed);
    // Signed encoder count (direction already applied by the backend).
    int64_t (*get_raw_count)(void *ctx);
    // True if motion must abort right now (endstop hit / e-stop / fault).
    bool (*aborted)(void *ctx);
} AxisCalibrationHooks;

// Tunables. The C axis_calibrate() fills the legacy defaults before calling.
//
// METHOD (revised 2026-07-24): a monotonic SWEEP, not alternating ± legs.
// The old method probed +500,-500,+1000,-1000,... — every leg was a
// direction reversal, so every measurement lost the backlash. That injects
// a sign(x)-shaped error into the regression which TILTS the fitted slope
// (measured ~4% low on real hardware). Sweeping in one direction keeps all
// segments same-direction, so backlash lands in the INTERCEPT instead —
// where it is harmless to the slope and directly measurable as the offset
// between the forward and reverse legs.
typedef struct AxisCalibrationParams {
    int32_t  probeSpeed;             // steps/s, low & safe
    uint8_t  sweepSegments;          // samples per direction
    int32_t  segmentSteps;           // steps between samples
    int32_t  minCountsForValidity;   // sweep must move at least this many counts
    float    minR2;                  // regression quality gate (per leg)
    float    maxSlopeMismatch;       // fwd/rev slopes must agree within this
    uint32_t settleMs;               // dwell after each move before reading
    uint16_t currentMicrosteps;      // filled in by caller (TMC setting)
} AxisCalibrationParams;

// Legacy default parameter table (probeSpeed 2000, 10 segments x 500 steps,
// minR2 0.98, maxSlopeMismatch 0.10, settle 60 ms). currentMicrosteps is
// left as given (the caller fills it in).
void axis_cal_params_default(AxisCalibrationParams *p, uint16_t currentMicrosteps);

// Run the full routine (origin -> sign -> scale -> backlash). On success
// fills `out` and returns true. On failure returns false and sets `err`.
bool axis_cal_run(const AxisCalibrationHooks *hooks, const AxisCalibrationParams *params,
                  AxisCalibration *out, AxisFault *err);

// Raw data of the LAST run(), kept so the host can inspect WHY a fit came
// out the way it did (nonlinearity, hysteresis, lost counts at speed...).
// Points are the individual (commanded steps, measured counts) probe legs
// fed into the regression, in execution order.
#define AXIS_CAL_MAX_POINTS 48
typedef struct AxisCalibrationLastRun {
    // Absolute sweep samples: (cumulative steps from start, encoder counts).
    // The first nForward entries are the forward leg, the rest the reverse
    // leg — i.e. exactly the data an `enctable` sweep would plot.
    int32_t stepsAtPoint[AXIS_CAL_MAX_POINTS];
    int32_t countsAtPoint[AXIS_CAL_MAX_POINTS];
    uint8_t nPoints;
    uint8_t nForward;
    double  slopeForward, interceptForward, r2Forward, residForward;
    double  slopeReverse, interceptReverse, r2Reverse, residReverse;
    double  slope;       // adopted slope (mean of the two legs, signed)
    double  intercept;   // forward-leg intercept
    double  r2;          // worst of the two legs
    double  residualStd; // worst of the two legs
} AxisCalibrationLastRun;

const AxisCalibrationLastRun *axis_cal_last_run(void);

// ---------------------------------------------------------------------------
// Persist / restore a calibration record per axis in NVS ("axiscal").
bool axis_cal_store_save(int axis, const AxisCalibration *cal);
bool axis_cal_store_load(int axis, AxisCalibration *cal);
void axis_cal_store_clear(int axis);

// Microstep-change rule (design §4): if currentMicrosteps != microstepsAtCal,
// rescale countsPerStep analytically, mark the record derived/unverified, and
// signal a CAL_INVALID *warning* (does not block motion). Returns true if a
// rescale happened (i.e. the caller should warn).
bool axis_cal_store_apply_microstep_rule(AxisCalibration *cal, uint16_t currentMicrosteps);

#ifdef __cplusplus
}
#endif

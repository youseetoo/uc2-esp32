#pragma once
#include <Arduino.h>
#include <functional>
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

namespace AxisCalibrationRoutine
{
    // Hooks the routine uses to drive one axis. Provided by AxisController.
    struct Hooks
    {
        // Current FAS commanded step position.
        std::function<int32_t()> getStepPos;
        // Move deltaSteps at |speed|, block until the motor stops. Return false
        // if aborted (endstop / e-stop / timeout) so the routine can bail out.
        std::function<bool(int32_t deltaSteps, int32_t speed)> moveRelBlocking;
        // Signed encoder count (direction already applied by the backend).
        std::function<int64_t()> getRawCount;
        // True if motion must abort right now (endstop hit / e-stop / fault).
        std::function<bool()> aborted;
    };

    // Tunables. Defaults match the 2 mm-pole-pitch / ~0.3-0.6 counts-per-step
    // regime described in the design (thresholds intentionally coarse).
    //
    // METHOD (revised 2026-07-24): a monotonic SWEEP, not alternating ± legs.
    // The old method probed +500,-500,+1000,-1000,... — every leg was a
    // direction reversal, so every measurement lost the backlash. That injects
    // a sign(x)-shaped error into the regression which TILTS the fitted slope
    // (measured ~4% low on real hardware). Sweeping in one direction keeps all
    // segments same-direction, so backlash lands in the INTERCEPT instead —
    // where it is harmless to the slope and directly measurable as the offset
    // between the forward and reverse legs.
    struct Params
    {
        int32_t  probeSpeed        = 2000;  // steps/s, low & safe
        uint8_t  sweepSegments     = 10;    // samples per direction
        int32_t  segmentSteps      = 500;   // steps between samples
        int32_t  minCountsForValidity = 4;  // sweep must move at least this many counts
        float    minR2             = 0.98f; // regression quality gate (per leg)
        float    maxSlopeMismatch  = 0.10f; // fwd/rev slopes must agree within 10%
        uint32_t settleMs          = 60;    // dwell after each move before reading
        uint16_t currentMicrosteps = 16;    // filled in by caller (TMC setting)
    };

    // Run the full routine (origin -> sign -> scale -> backlash). On success
    // fills `out` and returns true. On failure returns false and sets `err`.
    bool run(const Hooks &hooks, const Params &params, AxisCalibration &out, AxisFault &err);

    // Raw data of the LAST run(), kept so the host can inspect WHY a fit came
    // out the way it did (nonlinearity, hysteresis, lost counts at speed...).
    // Points are the individual (commanded steps, measured counts) probe legs
    // fed into the regression, in execution order.
    struct LastRun
    {
        static constexpr int MAX_POINTS = 48;
        // Absolute sweep samples: (cumulative steps from start, encoder counts).
        // The first nForward entries are the forward leg, the rest the reverse
        // leg — i.e. exactly the data an `enctable` sweep would plot.
        int32_t stepsAtPoint[MAX_POINTS] = {0};
        int32_t countsAtPoint[MAX_POINTS] = {0};
        uint8_t nPoints = 0;
        uint8_t nForward = 0;
        double  slopeForward = 0, interceptForward = 0, r2Forward = 0, residForward = 0;
        double  slopeReverse = 0, interceptReverse = 0, r2Reverse = 0, residReverse = 0;
        double  slope = 0;      // adopted slope (mean of the two legs, signed)
        double  intercept = 0;  // forward-leg intercept
        double  r2 = 0;         // worst of the two legs
        double  residualStd = 0;// worst of the two legs
    };
    const LastRun &lastRun();
}

namespace AxisCalibrationStore
{
    // Persist / restore a calibration record per axis in NVS ("axiscal").
    bool save(int axis, const AxisCalibration &cal);
    bool load(int axis, AxisCalibration &cal);
    void clear(int axis);

    // Microstep-change rule (design §4): if currentMicrosteps != microstepsAtCal,
    // rescale countsPerStep analytically, mark the record derived/unverified, and
    // signal a CAL_INVALID *warning* (does not block motion). Returns true if a
    // rescale happened (i.e. the caller should warn).
    bool applyMicrostepRule(AxisCalibration &cal, uint16_t currentMicrosteps);
}

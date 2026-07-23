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
    struct Params
    {
        int32_t  probeSpeed        = 2000;  // steps/s, low & safe
        int32_t  lengths[4]        = {500, 1000, 2000, 4000}; // scale probe lengths
        uint8_t  numLengths        = 4;
        int32_t  minCountsForValidity = 4;  // sign step must move at least this many counts
        float    minR2             = 0.98f; // regression quality gate
        uint8_t  backlashRepeats   = 3;     // averaged reversal measurements
        int32_t  backlashApproach  = 2000;  // steps to travel each backlash leg
        uint32_t settleMs          = 60;    // dwell after each move before reading
        uint16_t currentMicrosteps = 16;    // filled in by caller (TMC setting)
    };

    // Run the full routine (origin -> sign -> scale -> backlash). On success
    // fills `out` and returns true. On failure returns false and sets `err`.
    bool run(const Hooks &hooks, const Params &params, AxisCalibration &out, AxisFault &err);
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

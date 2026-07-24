#pragma once
#include <Arduino.h>
#include "AxisTypes.h"
#include "AxisCalibration.h"

// ============================================================================
// AxisController — the IAxis-style facade the rest of the firmware talks to
// (design v2, WP1). It maintains a per-axis AxisFeedback every loop and owns
// the encoder backend + calibration record.
//
// UNIT POLICY: every public value is in STEPS. Encoder counts are internal.
// Encoderless axes produce IDENTITY feedback (measuredSteps == commandedSteps,
// positionErrorSteps == 0, mode == OPEN_LOOP) so callers never branch on
// encoder presence.
//
// Scope note (WP1–WP3): moveTo() currently always runs OPEN_LOOP through
// FocusMotor; MONITOR only observes (updates positionErrorSteps + would notify
// once WP5 lands). CORRECT/SERVO behaviour is added in WP6/WP7 — setMode()
// accepts them but they do not yet alter motion. FastAccelStepper and its
// RMT/PCNT backend are never touched here.
// ============================================================================

namespace AxisController
{
    // Wire this into setup()/loop() next to the legacy encoder controller.
    void setup();
    void loop();

    // ---- unified motion interface (ImSwitch-visible; unchanged semantics) ---
    void moveTo(int axis, int32_t targetSteps, int32_t speed, bool isAbsolute);
    void stop(int axis);

    // ---- additive, optional ------------------------------------------------
    void setMode(int axis, AxisMode mode);
    AxisFeedback getFeedback(int axis);
    void resetAxis(int axis, AxisResetPolicy policy);

    // Run the WP3 calibration routine on `axis` (blocking, on-demand). Returns
    // true and persists the result on success; sets the axis fault on failure.
    bool calibrateAxis(int axis, const AxisCalibrationRoutine::Params &params);
    // Convenience overload using default params + the current microstep setting.
    bool calibrateAxis(int axis);

    // Read-only access to the persisted calibration for host reporting.
    AxisCalibration getCalibration(int axis);

    bool hasEncoder(int axis);

    // ---- deferred requests (safe to call from another task, e.g. the CANopen
    // timer task). They only set a scalar flag; the actual work runs in loop()
    // on the main task, so g_axes is never mutated concurrently. calibrate is
    // blocking and MUST be deferred this way rather than called inline. --------
    void requestMode(int axis, uint8_t mode);
    void requestReset(int axis, uint8_t policy);
    void requestCalibration(int axis);
}

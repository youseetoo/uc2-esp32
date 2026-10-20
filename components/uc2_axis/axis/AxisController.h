#pragma once
// T4.1: pure C (was a C++ namespace AxisController{...}). The C++ call sites
// (uc2_app/main.cpp, CANopenModule / DeviceRouter) are being converted to the
// axis_* functions below in the same batch.
#include <stdbool.h>
#include <stdint.h>
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
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Wire this into setup()/loop() next to the legacy encoder controller.
void axis_controller_setup(void);
void axis_controller_loop(void);

// ---- unified motion interface (ImSwitch-visible; unchanged semantics) ---
// Runs in the axis's configured mode (OPEN_LOOP/MONITOR/CORRECT/SERVO).
void axis_move_to(int axis, int32_t targetSteps, int32_t speed, bool isAbsolute);
// Same, but forcing a mode for THIS move only (the axis's configured mode is
// untouched). Used by the per-move {"closedloop":0|1} JSON flag.
void axis_move_to_with_mode(int axis, int32_t targetSteps, int32_t speed, bool isAbsolute,
                            uint8_t modeOverride);
void axis_stop(int axis);

// ---- additive, optional ------------------------------------------------
void axis_set_mode(int axis, uint8_t mode);
AxisFeedback axis_get_feedback(int axis);
void axis_reset(int axis, uint8_t policy); // AxisResetPolicy

// Run the WP3 calibration routine on `axis` (blocking, on-demand). Returns
// true and persists the result on success; sets the axis fault on failure.
bool axis_calibrate(int axis, const AxisCalibrationParams *params);
// Convenience using default params + the current microstep setting.
bool axis_calibrate_defaults(int axis);

// Read-only access to the persisted calibration for host reporting.
AxisCalibration axis_get_calibration(int axis);

// Diagnostic sweep: step the axis in `points` equal increments of
// `stepSize` (forward, then back to the start), sampling the raw encoder
// count at every stop. Emits the whole table as one JSON event so the host
// can plot counts-vs-steps and judge linearity / hysteresis / lost counts.
// Blocking (runs on the caller's task) — request it via axis_request_encoder_table.
bool axis_encoder_table(int axis, int points, int32_t stepSize, int32_t speed);

bool axis_has_encoder(int axis);

// ---- deferred requests (safe to call from another task, e.g. the CANopen
// timer task). They only set a scalar flag; the actual work runs in
// axis_controller_loop() on the main task, so g_axes is never mutated
// concurrently. Calibration is blocking and MUST be deferred this way
// rather than called inline. --------
void axis_request_mode(int axis, uint8_t mode);
void axis_request_reset(int axis, uint8_t policy);
void axis_request_calibration(int axis);
void axis_request_encoder_table(int axis, int points, int32_t stepSize, int32_t speed);

#ifdef __cplusplus
}
#endif

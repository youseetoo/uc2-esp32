// T4.1: C shims for the C++ FocusMotor / FAccelStep / TMCController functions
// the pure-C AxisController (uc2_axis) needs. Implemented in axis_motor_c.cpp
// (a thin C++ TU) so the C module stays free of C++ dependencies.
// Axis is 0..AXIS_MAX_STEPPERS-1; safe no-ops when out of range / disabled.
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// FocusMotor::isRunning(axis) — stepper backend reports the axis running.
bool axis_motor_is_running(int axis);

// FocusMotor::setPosition(axis, pos) — adopt `pos` as the FAS position.
void axis_motor_set_position(int axis, int32_t pos);

// true if the axis is being forced to abort (endstop hit / hard limit).
bool axis_motor_aborted(int axis);

// FAccelStep::setLiveSpeed(axis, signedSpeed) — the SERVO velocity path.
// No-op unless USE_FASTACCEL (AccelStep builds have no live set-speed).
void axis_motor_set_live_speed(int axis, int32_t signedSpeed);

// tmc_get_microsteps() — 0 unless TMC_CONTROLLER.
uint16_t axis_motor_get_microsteps(void);

#ifdef __cplusplus
}
#endif

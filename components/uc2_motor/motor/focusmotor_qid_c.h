// T4.1: C shim for the C++ FocusMotor — used by the pure-C QidRegistry
// (pause/resume motor control, gated on MOTOR_CONTROLLER). The C QidRegistry
// calls these C functions; this shim (a thin C++ TU) forwards to the C++
// FocusMotor namespace. Keeps the C module free of C++ dependencies.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Per-axis MotorData field accessors (the C QidRegistry reads/writes these
// for pause/resume). Axis is 0..MOTOR_AXIS_COUNT-1; safe no-ops when out of range.

int focusmotor_get_qid(int axis);
int focusmotor_is_stopped(int axis);
int32_t focusmotor_get_current_position(int axis);
int32_t focusmotor_get_target_position(int axis);
int32_t focusmotor_get_speed(int axis);
int32_t focusmotor_get_acceleration(int axis);
int focusmotor_get_absolute(int axis);
void focusmotor_stop_stepper(int axis);
void focusmotor_set_qid(int axis, int qid);
void focusmotor_set_target_position(int axis, int32_t target);
void focusmotor_set_speed(int axis, int32_t speed);
void focusmotor_set_acceleration(int axis, int32_t acceleration);
void focusmotor_set_absolute(int axis, int absolute);
void focusmotor_set_is_stop(int axis, int is_stop);
void focusmotor_set_stopped(int axis, int stopped);
void focusmotor_set_isforever(int axis, int isforever);
void focusmotor_start_stepper(int axis, int dir);

#ifdef __cplusplus
}
#endif

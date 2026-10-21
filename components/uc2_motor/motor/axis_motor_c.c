// T4.1: Pure C shims for the pure-C motor modules — what the pure-C
// AxisController (uc2_axis) needs. Forwards to FocusMotor.c, the FAccelStep
// island (facc_step_*) and TMCController (tmc_*).
// Gates mirror the app builds' defines (-DMOTOR_CONTROLLER, -DUSE_FASTACCEL,
// -DTMC_CONTROLLER); safe no-ops when out of range / disabled.
#include "axis_motor_c.h"
#include "MotorTypes.h"   // Stepper, MotorData, DEFAULT_ACCELERATION
#ifdef MOTOR_CONTROLLER
#include "FocusMotor.h"
#endif
#if defined(MOTOR_CONTROLLER) && defined(USE_FASTACCEL)
#include "FAccelStep.h"
#endif
#ifdef TMC_CONTROLLER
#include "TMCController.h"
#endif

bool axis_motor_is_running(int axis)
{
#ifdef MOTOR_CONTROLLER
    return focusmotor_is_running(axis);
#else
    (void)axis;
    return false;
#endif
}

void axis_motor_set_position(int axis, int32_t pos)
{
#ifdef MOTOR_CONTROLLER
    focusmotor_set_position((Stepper)axis, (int)pos);
#else
    (void)axis; (void)pos;
#endif
}

bool axis_motor_aborted(int axis)
{
#ifdef MOTOR_CONTROLLER
    if (axis >= 0)
    {
        MotorData **d = focusmotor_get_data();
        if (d && d[axis])
            return d[axis]->hardLimitTriggered || d[axis]->endstop_hit;
    }
#endif
    (void)axis;
    return false;
}

void axis_motor_set_live_speed(int axis, int32_t signedSpeed)
{
#if defined(MOTOR_CONTROLLER) && defined(USE_FASTACCEL)
    // SERVO velocity path: honour the axis' configured acceleration
    // (same sane default as faccel_step_start).
    int32_t accel = DEFAULT_ACCELERATION;
    if (axis >= 0)
    {
        MotorData **d = focusmotor_get_data();
        if (d && d[axis] && d[axis]->acceleration > 0)
            accel = d[axis]->acceleration;
    }
    faccel_step_set_live_speed(axis, signedSpeed, (uint32_t)accel);
#else
    (void)axis; (void)signedSpeed;
#endif
}

uint16_t axis_motor_get_microsteps(void)
{
#ifdef TMC_CONTROLLER
    return tmc_get_microsteps();
#else
    return 0;
#endif
}

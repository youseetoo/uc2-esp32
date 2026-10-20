// T4.1: C++ TU implementing axis_motor_c.h over the C++
// FocusMotor / FAccelStep / TMCController namespaces (thin forwarders).
// Gates mirror the CXX_FLAGS defines of the app builds (-DMOTOR_CONTROLLER,
// -DUSE_FASTACCEL, -DTMC_CONTROLLER).
#include "axis_motor_c.h"

#ifdef MOTOR_CONTROLLER
#include "FocusMotor.h"
#include "MotorTypes.h"
#endif
#if defined(MOTOR_CONTROLLER) && defined(USE_FASTACCEL)
#include "FAccelStep.h"
#endif
#ifdef TMC_CONTROLLER
#include "TMCController.h"
#endif

extern "C" {

bool axis_motor_is_running(int axis)
{
#ifdef MOTOR_CONTROLLER
    return FocusMotor::isRunning(axis);
#else
    (void)axis;
    return false;
#endif
}

void axis_motor_set_position(int axis, int32_t pos)
{
#ifdef MOTOR_CONTROLLER
    FocusMotor::setPosition(static_cast<Stepper>(axis), (int)pos);
#else
    (void)axis; (void)pos;
#endif
}

bool axis_motor_aborted(int axis)
{
#ifdef MOTOR_CONTROLLER
    if (axis >= 0)
    {
        MotorData **d = FocusMotor::getData();
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
    FAccelStep::setLiveSpeed(axis, signedSpeed);
#else
    (void)axis; (void)signedSpeed;
#endif
}

uint16_t axis_motor_get_microsteps(void)
{
#ifdef TMC_CONTROLLER
    return TMCController::getMicrosteps();
#else
    return 0;
#endif
}

} // extern "C"

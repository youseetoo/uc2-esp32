// T4.1: C shim implementation for the C++ FocusMotor — used by the pure-C
// QidRegistry (pause/resume motor control). This is a thin C++ TU that
// forwards the C focusmotor_* calls to the C++ FocusMotor namespace.
#include "focusmotor_qid_c.h"
#include "FocusMotor.h"
#include "PinConfig.h"   // MOTOR_AXIS_COUNT

extern "C" {

int focusmotor_get_qid(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return -1;
    return FocusMotor::getData()[axis]->qid;
}

int focusmotor_is_stopped(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return 1;
    return FocusMotor::getData()[axis]->stopped ? 1 : 0;
}

int32_t focusmotor_get_current_position(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return 0;
    return FocusMotor::getData()[axis]->currentPosition;
}

int32_t focusmotor_get_target_position(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return 0;
    return FocusMotor::getData()[axis]->targetPosition;
}

int32_t focusmotor_get_speed(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return 0;
    return FocusMotor::getData()[axis]->speed;
}

int32_t focusmotor_get_acceleration(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return 0;
    return FocusMotor::getData()[axis]->acceleration;
}

int focusmotor_get_absolute(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return 0;
    return FocusMotor::getData()[axis]->absolutePosition ? 1 : 0;
}

void focusmotor_stop_stepper(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return;
    FocusMotor::stopStepper(axis);
}

void focusmotor_set_qid(int axis, int qid)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return;
    FocusMotor::getData()[axis]->qid = qid;
}

void focusmotor_set_target_position(int axis, int32_t target)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return;
    FocusMotor::getData()[axis]->targetPosition = target;
}

void focusmotor_set_speed(int axis, int32_t speed)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return;
    FocusMotor::getData()[axis]->speed = speed;
}

void focusmotor_set_acceleration(int axis, int32_t acceleration)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return;
    FocusMotor::getData()[axis]->acceleration = acceleration;
}

void focusmotor_set_absolute(int axis, int absolute)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return;
    FocusMotor::getData()[axis]->absolutePosition = absolute ? 1 : 0;
}

void focusmotor_set_is_stop(int axis, int is_stop)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return;
    FocusMotor::getData()[axis]->isStop = is_stop ? 1 : 0;
}

void focusmotor_set_stopped(int axis, int stopped)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return;
    FocusMotor::getData()[axis]->stopped = stopped ? 1 : 0;
}

void focusmotor_set_isforever(int axis, int isforever)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return;
    FocusMotor::getData()[axis]->isforever = isforever ? 1 : 0;
}

void focusmotor_start_stepper(int axis, int reduced)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return;
    FocusMotor::startStepper(axis, reduced);
}

}  // extern "C"

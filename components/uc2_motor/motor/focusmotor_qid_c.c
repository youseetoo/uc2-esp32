// T4.1: Pure C accessors over the pure-C FocusMotor module — used by the
// pure-C QidRegistry (pause/resume motor control) and AxisController.
// Reads/writes per-axis MotorData fields via focusmotor_get_data().
// focusmotor_start_stepper()/focusmotor_stop_stepper() are still declared in
// focusmotor_qid_c.h for convenience but are DEFINED in FocusMotor.c
// (the old C++ shim defined them too — that duplicate is gone).
#include "focusmotor_qid_c.h"
#include "FocusMotor.h"
#include "PinConfig.h"   // MOTOR_AXIS_COUNT

static MotorData *_md(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
        return NULL;
    MotorData **d = focusmotor_get_data();
    if (!d)
        return NULL;
    return d[axis];
}

int focusmotor_get_qid(int axis)
{
    MotorData *m = _md(axis);
    return m ? m->qid : -1;
}

int focusmotor_is_stopped(int axis)
{
    MotorData *m = _md(axis);
    return m ? (m->stopped ? 1 : 0) : 1;
}

int32_t focusmotor_get_current_position(int axis)
{
    MotorData *m = _md(axis);
    return m ? m->currentPosition : 0;
}

int32_t focusmotor_get_target_position(int axis)
{
    MotorData *m = _md(axis);
    return m ? m->targetPosition : 0;
}

int32_t focusmotor_get_speed(int axis)
{
    MotorData *m = _md(axis);
    return m ? m->speed : 0;
}

int32_t focusmotor_get_acceleration(int axis)
{
    MotorData *m = _md(axis);
    return m ? m->acceleration : 0;
}

int focusmotor_get_absolute(int axis)
{
    MotorData *m = _md(axis);
    return m ? (m->absolutePosition ? 1 : 0) : 0;
}

void focusmotor_set_qid(int axis, int qid)
{
    MotorData *m = _md(axis);
    if (m)
        m->qid = qid;
}

void focusmotor_set_target_position(int axis, int32_t target)
{
    MotorData *m = _md(axis);
    if (m)
        m->targetPosition = target;
}

void focusmotor_set_speed(int axis, int32_t speed)
{
    MotorData *m = _md(axis);
    if (m)
        m->speed = speed;
}

void focusmotor_set_acceleration(int axis, int32_t acceleration)
{
    MotorData *m = _md(axis);
    if (m)
        m->acceleration = acceleration;
}

void focusmotor_set_absolute(int axis, int absolute)
{
    MotorData *m = _md(axis);
    if (m)
        m->absolutePosition = absolute ? 1 : 0;
}

void focusmotor_set_is_stop(int axis, int is_stop)
{
    MotorData *m = _md(axis);
    if (m)
        m->isStop = is_stop ? 1 : 0;
}

void focusmotor_set_stopped(int axis, int stopped)
{
    MotorData *m = _md(axis);
    if (m)
        m->stopped = stopped ? 1 : 0;
}

void focusmotor_set_isforever(int axis, int isforever)
{
    MotorData *m = _md(axis);
    if (m)
        m->isforever = isforever ? 1 : 0;
}

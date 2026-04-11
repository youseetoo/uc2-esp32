#include <PinConfig.h>
#include "LocalMotorBackend.h"
#include "../FocusMotor.h"

#ifdef USE_FASTACCEL
#include "../FAccelStep.h"
#endif
#ifdef USE_ACCELSTEP
#include "../AccelStep.h"
#endif

void LocalMotorBackend::setup()
{
    // The actual FastAccelStepper/AccelStepper init is still done in FocusMotor::setup()
    // because it depends on TCA9535 wiring and pin config. This backend only wraps
    // the runtime calls.
    log_i("LocalMotorBackend::setup()");
}

void LocalMotorBackend::loop()
{
    // Nothing needed — FocusMotor::loop() handles soft-limit checks
}

void LocalMotorBackend::startMove(int axis, MotorData* data, int reduced)
{
#ifdef USE_FASTACCEL
    FAccelStep::startFastAccelStepper(axis);
#elif defined(USE_ACCELSTEP)
    AccelStep::startAccelStepper(axis);
#else
    log_w("LocalMotorBackend: no stepper driver for axis %d", axis);
#endif
}

void LocalMotorBackend::stopMove(int axis)
{
#ifdef USE_FASTACCEL
    FAccelStep::stopFastAccelStepper(axis);
#elif defined(USE_ACCELSTEP)
    AccelStep::stopAccelStepper(axis);
#endif
}

long LocalMotorBackend::getPosition(int axis) const
{
#ifdef USE_FASTACCEL
    return FAccelStep::getCurrentPosition(static_cast<Stepper>(axis));
#else
    return FocusMotor::getData()[axis]->currentPosition;
#endif
}

bool LocalMotorBackend::isRunning(int axis) const
{
#ifdef USE_FASTACCEL
    return FAccelStep::isRunning(axis);
#elif defined(USE_ACCELSTEP)
    return AccelStep::isRunning(axis);
#else
    return false;
#endif
}

void LocalMotorBackend::updateData(int axis)
{
#ifdef USE_FASTACCEL
    FAccelStep::updateData(axis);
#elif defined(USE_ACCELSTEP)
    AccelStep::updateData(axis);
#endif
}

void LocalMotorBackend::setPosition(int axis, int pos)
{
#ifdef USE_FASTACCEL
    FAccelStep::setPosition(static_cast<Stepper>(axis), pos);
#elif defined(USE_ACCELSTEP)
    AccelStep::setPosition(static_cast<Stepper>(axis), pos);
#endif
}

void LocalMotorBackend::setAutoEnable(bool enable)
{
#ifdef USE_FASTACCEL
    FAccelStep::setAutoEnable(enable);
#endif
}

void LocalMotorBackend::setEnable(bool enable)
{
#ifdef USE_FASTACCEL
    FAccelStep::Enable(enable);
#elif defined(USE_ACCELSTEP)
    AccelStep::Enable(enable);
#endif
}

#include <PinConfig.h>
#include "LocalMotorBackend.h"
#include "../FocusMotor.h"

#ifdef USE_FASTACCEL
#include "../FAccelStep.h"
#endif
#ifdef USE_ACCELSTEP
#include "../AccelStep.h"
#endif
#ifdef USE_TCA9535
#include "../../i2c/tca_controller.h"
#endif

void LocalMotorBackend::setup()
{
    log_i("LocalMotorBackend::setup(): initialising stepper hardware");
#ifdef USE_FASTACCEL
#ifdef USE_TCA9535
    FAccelStep::setExternalCallForPin(tca_controller::setExternalPin);
#endif
    FAccelStep::setupFastAccelStepper();
#endif
#ifdef USE_ACCELSTEP
#ifdef USE_TCA9535
    AccelStep::setExternalCallForPin(tca_controller::setExternalPin);
#endif
    AccelStep::setupAccelStepper();
#endif
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

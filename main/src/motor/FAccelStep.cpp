#include "FAccelStep.h"
#include "PinConfig.h"

void FAccelStep::startFastAccelStepper(int i)
{
    // enableEnablePin(i);
    log_i("Start Stepper %i ", i);
    if (faststeppers[i] == nullptr)
    {
        return;
    }
    int speed = data[i]->speed;
    if (speed < 0)
    {
        speed *= -1;
    }

    faststeppers[i]->setSpeedInHz(speed);
    faststeppers[i]->setAcceleration(data[i]->acceleration);

    if (data[i]->isforever)
    {
        // run forver (e.g. PSx or initaited via Serial)
        if (data[i]->speed > 0)
        {
            // run clockwise
            log_i("forward");
            faststeppers[i]->runForward();
        }
        else
        {
            // run counterclockwise
            log_i("backward");
            faststeppers[i]->runBackward();
        }
    }
    else
    {
        if (data[i]->absolutePosition == 1)
        {
            // absolute position coordinates
            faststeppers[i]->moveTo(data[i]->targetPosition, false);
        }
        else if (data[i]->absolutePosition == 0)
        {
            // relative position coordinates
            faststeppers[i]->move(data[i]->targetPosition, false);
        }
    }
    data[i]->stopped = false;
}

void FAccelStep::setupFastAccelStepper()
{
    engine.init();
    if (pinConfig.I2C_SCL > 0)
    {
        engine.setExternalCallForPin(_externalCallForPin);
    }

    // setup the data
    for (int i = 0; i < faststeppers.size(); i++)
    {
        faststeppers[i] = nullptr;
    }

    /* restore previously saved motor position values*/

    // setup the stepper A
    if (pinConfig.MOTOR_A_STEP >= 0)
    {
        if (pinConfig.I2C_SCL > 0)
            setupFastAccelStepper(Stepper::A, 100 | PIN_EXTERNAL_FLAG, 104 | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_A_STEP);
        else
            setupFastAccelStepper(Stepper::A, pinConfig.MOTOR_ENABLE, pinConfig.MOTOR_A_DIR, pinConfig.MOTOR_A_STEP);
        data[Stepper::A]->isActivated = true;
    }

    // setup the stepper X
    if (pinConfig.MOTOR_X_STEP >= 0)
    {
        if (pinConfig.I2C_SCL > 0)
            setupFastAccelStepper(Stepper::X, 100 | PIN_EXTERNAL_FLAG, 101 | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_X_STEP);
        else
            setupFastAccelStepper(Stepper::X, pinConfig.MOTOR_ENABLE, pinConfig.MOTOR_X_DIR, pinConfig.MOTOR_X_STEP);
        data[Stepper::X]->isActivated = true;
    }

    // setup the stepper Y
    if (pinConfig.MOTOR_Y_STEP >= 0)
    {
        if (pinConfig.I2C_SCL > 0)
            setupFastAccelStepper(Stepper::Y, 100 | PIN_EXTERNAL_FLAG, 102 | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_Y_STEP);
        else
            setupFastAccelStepper(Stepper::Y, pinConfig.MOTOR_ENABLE, pinConfig.MOTOR_Y_DIR, pinConfig.MOTOR_Y_STEP);
        data[Stepper::Y]->isActivated = true;
    }

    // setup the stepper Z
    if (pinConfig.MOTOR_Z_STEP >= 0)
    {
        if (pinConfig.I2C_SCL > 0)
            setupFastAccelStepper(Stepper::Z, 100 | PIN_EXTERNAL_FLAG, 103 | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_Z_STEP);
        else
            setupFastAccelStepper(Stepper::Z, pinConfig.MOTOR_ENABLE, pinConfig.MOTOR_Z_DIR, pinConfig.MOTOR_Z_STEP);
        data[Stepper::Z]->isActivated = true;
    }
}

void FAccelStep::setupFastAccelStepper(Stepper stepper, int motoren, int motordir, int motorstp)
{
    faststeppers[stepper] = engine.stepperConnectToPin(motorstp);
    faststeppers[stepper]->setEnablePin(motoren, pinConfig.MOTOR_ENABLE_INVERTED);
    faststeppers[stepper]->setDirectionPin(motordir, false);
    faststeppers[stepper]->setAutoEnable(pinConfig.MOTOR_AUTOENABLE);
    faststeppers[stepper]->setSpeedInHz(MAX_VELOCITY_A);
    faststeppers[stepper]->setAcceleration(DEFAULT_ACCELERATION);
    faststeppers[stepper]->setCurrentPosition(data[stepper]->currentPosition);
    faststeppers[stepper]->move(2);
    faststeppers[stepper]->move(-2);
}

void FAccelStep::stopFastAccelStepper(int i)
{
    if (faststeppers[i] == nullptr)
        return;
    faststeppers[i]->forceStop();
    faststeppers[i]->stopMove();
    data[i]->isforever = false;
    data[i]->speed = 0;
    data[i]->currentPosition = faststeppers[i]->getCurrentPosition();
    data[i]->stopped = true;
}

void FAccelStep::setExternalCallForPin(
    bool (*func)(uint8_t pin, uint8_t value))
{
    _externalCallForPin = func;
}

void FAccelStep::updateData(int i)
{
    data[i]->currentPosition = faststeppers[i]->getCurrentPosition();
}

void FAccelStep::setAutoEnable(bool enable)
{
    for (int i = 0; i < faststeppers.size(); i++)
    {
        log_d("enable motor %d - automode on", i);
        faststeppers[i]->setAutoEnable(enable);
    }
}

void FAccelStep::Enable(bool enable)
{
    for (int i = 0; i < faststeppers.size(); i++)
    {

        if (enable)
        {
            log_d("enable motor %d - going manual mode!", i);
            faststeppers[i]->enableOutputs();
            faststeppers[i]->setAutoEnable(false);
        }
        else
        {
            log_d("disable motor %d - going manual mode!", i);
            faststeppers[i]->disableOutputs();
            faststeppers[i]->setAutoEnable(false);
        }
    }
}


void FAccelStep::setPosition(Stepper s, int val)
{
    faststeppers[s]->setCurrentPosition(val);
}


bool FAccelStep::isRunning(int i)
{
    return faststeppers[i]->isRunning();
}

void FAccelStep::move(Stepper s, int steps, bool blocking)
{
    // move the motor by the given steps 
    faststeppers[s]->move(steps, blocking);
}
#include "FAccelStep.h"
#include <PinConfig.h>

// convert pins to internal pins in setupFastAccelStepper
#ifdef USE_TCA9535
#undef PIN_EXTERNAL_FLAG
#define PIN_EXTERNAL_FLAG 128
#else
#undef PIN_EXTERNAL_FLAG
#define PIN_EXTERNAL_FLAG 0x00
#endif
using namespace FocusMotor;
namespace FAccelStep
{
    void startFastAccelStepper(int i)
    {
        // enableEnablePin(i);
        if (faststeppers[i] == nullptr)
        {
            log_e("stepper %i is null", i);
            return;
        }
        int speed = getData()[i]->speed;
        if (speed < 0)
        {
            speed *= -1;
        }

        faststeppers[i]->setSpeedInHz(speed);
        faststeppers[i]->setAcceleration(getData()[i]->acceleration);

        getData()[i]->stopped = false;
        if (getData()[i]->isforever)
        {
            // run forver (e.g. PSx or initaited via Serial)
            if (getData()[i]->speed > 0)
            {
                // run clockwise
                log_i("forward");
                faststeppers[i]->runForward();
            }
            else if (getData()[i]->speed < 0)
            {
                // run counterclockwise
                log_i("backward");
                faststeppers[i]->runBackward();
            }
        }
        else
        {
            if (getData()[i]->absolutePosition == 1)
            {
                // absolute position coordinates
                //log_i("moveTo %i", getData()[i]->targetPosition);
                faststeppers[i]->moveTo(getData()[i]->targetPosition, false);
            }
            else if (getData()[i]->absolutePosition == 0)
            {
                // relative position coordinates
                //log_i("move %i", getData()[i]->targetPosition);
                faststeppers[i]->move(getData()[i]->targetPosition, false);
            }
        }

        /*
        log_i("start stepper (act): motor:%i isforver:%i, speed: %i, maxSpeed: %i, target pos: %i, isabsolute: %i, isacceleration: %i, acceleration: %i, isStopped %i, isRunning %i",
              i,
              getData()[i]->isforever,
              getData()[i]->speed,
              getData()[i]->maxspeed,
              getData()[i]->targetPosition,
              getData()[i]->absolutePosition,
              getData()[i]->isaccelerated,
              getData()[i]->acceleration,
              getData()[i]->stopped,
              faststeppers[i]->isRunning());
              */
    }

    void setupFastAccelStepper()
    {
        if (getData()[Stepper::A] == nullptr)
            log_e("Stepper A getData() NULL");
        if (getData()[Stepper::X] == nullptr)
            log_e("Stepper X getData() NULL");
        if (getData()[Stepper::Y] == nullptr)
            log_e("Stepper Y getData() NULL");
        if (getData()[Stepper::Z] == nullptr)
            log_e("Stepper Z getData() NULL");
        engine.init();
        log_i("FastAccelStepper engine initialized");
#ifdef USE_TCA9535
        engine.setExternalCallForPin(_externalCallForPin);
#endif

        // setup the getData()
        for (int i = 0; i < faststeppers.size(); i++)
        {
            faststeppers[i] = nullptr;
        }
        // setup the stepper A
        if (pinConfig.MOTOR_A_STEP >= 0)
        {
            log_i("setupFastAccelStepper A");
            setupFastAccelStepper(Stepper::A, pinConfig.MOTOR_ENABLE | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_A_DIR  | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_A_STEP);
            getData()[Stepper::A]->isActivated = true;
        }

        // setup the stepper X
        if (pinConfig.MOTOR_X_STEP >= 0)
        {
            log_i("setupFastAccelStepper X");
            setupFastAccelStepper(Stepper::X, pinConfig.MOTOR_ENABLE | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_X_DIR | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_X_STEP);
            getData()[Stepper::X]->isActivated = true;
        }

        // setup the stepper Y
        if (pinConfig.MOTOR_Y_STEP >= 0)
        {
            log_i("setupFastAccelStepper Y");
            setupFastAccelStepper(Stepper::Y, pinConfig.MOTOR_ENABLE | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_Y_DIR | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_Y_STEP);
            getData()[Stepper::Y]->isActivated = true;
        }

        // setup the stepper Z
        if (pinConfig.MOTOR_Z_STEP >= 0)
        {
            log_i("setupFastAccelStepper Z");
            setupFastAccelStepper(Stepper::Z, pinConfig.MOTOR_ENABLE | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_Z_DIR | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_Z_STEP);
            getData()[Stepper::Z]->isActivated = true;
        }
    }

    void setupFastAccelStepper(Stepper stepper, int motoren, int motordir, int motorstp)
    {
        log_i("setupFastAccelStepper %i with motor pins: %i, %i, %i", stepper, motoren, motordir, motorstp);
        log_i("Heap before setupFastAccelStepper: %d", ESP.getFreeHeap());
        faststeppers[stepper] = engine.stepperConnectToPin(motorstp);
        faststeppers[stepper]->setEnablePin(motoren, pinConfig.MOTOR_ENABLE_INVERTED);
        faststeppers[stepper]->setDirectionPin(motordir, false);
        faststeppers[stepper]->setAutoEnable(pinConfig.MOTOR_AUTOENABLE);
        faststeppers[stepper]->setSpeedInHz(MAX_VELOCITY_A);
        faststeppers[stepper]->setAcceleration(DEFAULT_ACCELERATION);
        faststeppers[stepper]->setCurrentPosition(getData()[stepper]->currentPosition);
        faststeppers[stepper]->move(2);
        faststeppers[stepper]->move(-2);
    }

    void stopFastAccelStepper(int i)
    {
        if (faststeppers[i] == nullptr)
            return;
        faststeppers[i]->forceStop();
        faststeppers[i]->stopMove();
        log_i("stop stepper %i", i);
        getData()[i]->isforever = false;
        getData()[i]->speed = 0;
        getData()[i]->currentPosition = faststeppers[i]->getCurrentPosition();
        getData()[i]->stopped = true;
    }

    void setExternalCallForPin(
        bool (*func)(uint8_t pin, uint8_t value))
    {
        _externalCallForPin = func;
    }

    void updateData(int i)
    {
        getData()[i]->currentPosition = faststeppers[i]->getCurrentPosition();
    }

    void setAutoEnable(bool enable)
    {
        for (int i = 0; i < faststeppers.size(); i++)
        {
            log_d("enable motor %d - automode on", i);
            faststeppers[i]->setAutoEnable(enable);
        }
    }

    void Enable(bool enable)
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

    void setPosition(Stepper s, int val)
    {
        faststeppers[s]->setCurrentPosition(val);
    }

    bool isRunning(int i)
    {
        return faststeppers[i]->isRunning();
    }

    void move(Stepper s, int steps, bool blocking)
    {
        // move the motor by the given steps
        faststeppers[s]->move(steps, blocking);
    }
}
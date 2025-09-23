#include <PinConfig.h>

#include "AccelStep.h"
#include "FocusMotor.h"
#include "esp_task_wdt.h"
using namespace FocusMotor;

void driveMotorXLoop(void *pvParameter)
{
    AccelStep::driveMotorLoop(Stepper::X);
}
void driveMotorYLoop(void *pvParameter)
{
    AccelStep::driveMotorLoop(Stepper::Y);
}
void driveMotorZLoop(void *pvParameter)
{
    AccelStep::driveMotorLoop(Stepper::Z);
}
void driveMotorALoop(void *pvParameter)
{
    AccelStep::driveMotorLoop(Stepper::A);
}

namespace AccelStep
{

    void poweron()
    {
        if (!power_enable)
        {
#ifdef USE_TCA9535            
                _externalCallForPin(100, HIGH ^ pinConfig.MOTOR_ENABLE_INVERTED);
#endif
        }
        else
        {
            pinMode(pinConfig.MOTOR_ENABLE, OUTPUT);
            digitalWrite(pinConfig.MOTOR_ENABLE, HIGH ^ pinConfig.MOTOR_ENABLE_INVERTED);
        }
        power_enable = true;
        log_i("poweron motors");
    }

    void poweroff(bool force)
    {
        if ((getData()[Stepper::A]->stopped &&
             getData()[Stepper::X]->stopped &&
             getData()[Stepper::Y]->stopped &&
             getData()[Stepper::Z]->stopped &&
             power_enable) ||
            force)
        {
#ifdef USE_TCA9535
            _externalCallForPin(100, LOW ^ pinConfig.MOTOR_ENABLE_INVERTED);
#else
            pinMode(pinConfig.MOTOR_ENABLE, OUTPUT);
            digitalWrite(pinConfig.MOTOR_ENABLE, LOW ^ pinConfig.MOTOR_ENABLE_INVERTED);
#endif
            power_enable = false;
            log_i("poweroff motors");
        }
    }

    void setupAccelStepper()
    {
        /*
          Motor related settings
        */

        for (int i = 0; i < taskRunning.size(); i++)
        {
            taskRunning[i] = false;
        }

        if (pinConfig.MOTOR_A_STEP > -1)
        {
#ifdef USE_TCA9535
            {
                steppers[Stepper::A] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_A_STEP, 104 | PIN_EXTERNAL_FLAG, -1, -1, true, _externalCallForPin);
            }
#else
            if (pinConfig.AccelStepperMotorType == AccelStepper::DRIVER)
            {
                steppers[Stepper::A] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_A_STEP, pinConfig.MOTOR_A_DIR);
            }
            else if (pinConfig.AccelStepperMotorType == AccelStepper::HALF4WIRE)
            {
                steppers[Stepper::A] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.MOTOR_A_STEP, pinConfig.MOTOR_A_DIR, pinConfig.MOTOR_A_0, pinConfig.MOTOR_A_1);
            }
#endif
        }
        if (pinConfig.MOTOR_X_STEP > -1)
        {
#ifdef USE_TCA9535
            steppers[Stepper::X] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_X_STEP, 101 | PIN_EXTERNAL_FLAG, -1, -1, true, _externalCallForPin);
            //[Stepper::X]->setEnablePin(100 | PIN_EXTERNAL_FLAG);
            // steppers[Stepper::X]->setPinsInverted(false, false, true);
#else
            if (pinConfig.AccelStepperMotorType == AccelStepper::DRIVER)
            {
                steppers[Stepper::X] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_X_STEP, pinConfig.MOTOR_X_DIR);
            }
            else if (pinConfig.AccelStepperMotorType == AccelStepper::HALF4WIRE)
            {
                steppers[Stepper::X] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.MOTOR_X_STEP, pinConfig.MOTOR_X_DIR, pinConfig.MOTOR_X_0, pinConfig.MOTOR_X_1);
            }
#endif
        }
        if (pinConfig.MOTOR_Y_STEP > -1)
        {
#ifdef USE_TCA9535
            steppers[Stepper::Y] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_Y_STEP, 102 | PIN_EXTERNAL_FLAG, -1, -1, true, _externalCallForPin);
#else
            if (pinConfig.AccelStepperMotorType == AccelStepper::DRIVER)
            {
                steppers[Stepper::Y] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_Y_STEP, pinConfig.MOTOR_Y_DIR);
            }
            else if (pinConfig.AccelStepperMotorType == AccelStepper::HALF4WIRE)
            {
                steppers[Stepper::Y] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.MOTOR_Y_STEP, pinConfig.MOTOR_Y_DIR, pinConfig.MOTOR_Y_0, pinConfig.MOTOR_Y_1);
            }
#endif
        }
        if (pinConfig.MOTOR_Z_STEP > -1)
        {
#ifdef USE_TCA9535
            steppers[Stepper::Z] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_Z_STEP, 103 | PIN_EXTERNAL_FLAG, -1, -1, true, _externalCallForPin);
#else
            if (pinConfig.AccelStepperMotorType == AccelStepper::DRIVER)
            {
                steppers[Stepper::Z] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_Z_STEP, pinConfig.MOTOR_Z_DIR);
            }
            else if (pinConfig.AccelStepperMotorType == AccelStepper::HALF4WIRE)
            {
                steppers[Stepper::Z] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.MOTOR_Z_STEP, pinConfig.MOTOR_Z_DIR, pinConfig.MOTOR_Z_0, pinConfig.MOTOR_Z_1);
            }
#endif
        }

        // setting default values
        for (int i = 0; i < steppers.size(); i++)
        {
            if (steppers[i])
            {
                log_d("setting default values for motor %i", i);
                steppers[i]->setMaxSpeed(MAX_VELOCITY_A);
                steppers[i]->setAcceleration(DEFAULT_ACCELERATION);
                steppers[i]->runToNewPosition(-1);
                steppers[i]->runToNewPosition(1);
                log_d("1");

                steppers[i]->setCurrentPosition(getData()[i]->currentPosition);
            }
        }
        poweroff(true);
    }

    void startAccelStepper(int i)
    {

        log_d("start rotator: motor:%i, isforver:%i, speed: %i, maxSpeed: %i, target pos: %i, isabsolute: %i, isacceleration: %i, acceleration: %i",
              i,
              getData()[i]->isforever,
              getData()[i]->speed,
              getData()[i]->maxspeed,
              getData()[i]->targetPosition,
              getData()[i]->absolutePosition,
              getData()[i]->isaccelerated,
              getData()[i]->acceleration);

        // adjust direction pin if necessary
        if (getData()[i]->directionPinInverted)
        {
            steppers[i]->setPinsInverted (true, false, false);
            log_i("Inverting direction pin for motor %i", i);
        }

        if (!getData()[i]->isforever)
        {
            // accelstepper wants in relative mode that targetpostion and speed point into same direction
            if (!getData()[i]->absolutePosition && ((getData()[i]->targetPosition < 0 && getData()[i]->speed > 0) || (getData()[i]->targetPosition > 0 && getData()[i]->speed < 0)))
                getData()[i]->speed *= -1;
            // in absolute mode speed direction is up to the target and current position difference
            if (getData()[i]->absolutePosition)
            {
                int speeddir = getData()[i]->targetPosition - getData()[i]->currentPosition;
                if ((speeddir > 0 && getData()[i]->speed < 0) || (speeddir < 0 && getData()[i]->speed > 0))
                    getData()[i]->speed *= -1;
            }
        }
        steppers[i]->setMaxSpeed(getData()[i]->maxspeed);
        steppers[i]->setAcceleration(getData()[i]->acceleration);
        if (getData()[i]->absolutePosition)
        {
            if (getData()[i]->currentPosition == getData()[i]->targetPosition)
                return;
            // absolute position coordinates
            steppers[i]->moveTo(getData()[i]->targetPosition);
        }
        else
        {
            // relative position coordinates
            steppers[i]->move(getData()[i]->targetPosition);
        }
        getData()[i]->stopped = false;

        if (i == 0 && !taskRunning[i])
            xTaskCreatePinnedToCore(&driveMotorALoop, "motor_task_A", pinConfig.MOTOR_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY - 1, NULL, 0);
        if (i == 1 && !taskRunning[i])
        {
            xTaskCreatePinnedToCore(&driveMotorXLoop, "motor_task_X", pinConfig.MOTOR_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY - 1, NULL, 0);
            log_i("started x task");
        }
        // else
        //     log_i("x wont start");
        if (i == 2 && !taskRunning[i])
            xTaskCreatePinnedToCore(&driveMotorYLoop, "motor_task_Y", pinConfig.MOTOR_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY - 1, NULL, 0);
        if (i == 3 && !taskRunning[i])
            xTaskCreatePinnedToCore(&driveMotorZLoop, "motor_task_Z", pinConfig.MOTOR_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY - 1, NULL, 0);
    }


    void setPosition(Stepper axis, int pos){
        getData()[axis]->currentPosition = pos;
        steppers[axis]->setCurrentPosition(pos);
    }
    
    void stopAccelStepper(int i)
    {
        log_i("stop stepper %i", i);
        steppers[i]->stop();
        getData()[i]->isforever = false;
        getData()[i]->speed = 0;
        getData()[i]->currentPosition = steppers[i]->currentPosition();
        getData()[i]->stopped = true;
    }

    bool isRunning(int stepperid)
    {
        AccelStepper *s = steppers[stepperid];
        return s->isRunning();
    }

    void driveMotorLoop(int stepperid)
    {
        poweron();
        AccelStepper *s = steppers[stepperid];
        s->setMaxSpeed(getData()[stepperid]->maxspeed);
        log_i("Start Task %i", stepperid);
        while (!getData()[stepperid]->stopped)
        {

            if (getData()[stepperid]->isforever)
            {
                s->setSpeed(getData()[stepperid]->speed);
                s->runSpeed();
            }
            else
            {
                if (!getData()[stepperid]->isaccelerated)
                    s->setSpeed(getData()[stepperid]->speed);
                if (!s->run())
                    stopAccelStepper(stepperid);
                // checks if a stepper is still running
                // log_i("distance to go:%i", s->distanceToGo());
                if (s->distanceToGo() == 0 && !getData()[stepperid]->stopped)
                {
                    log_i("stop stepper:%i", stepperid);
                    // if not turn it off
                    stopAccelStepper(stepperid);
                }
            }
            getData()[stepperid]->currentPosition = s->currentPosition();
        }
        getData()[stepperid]->stopped = true;
        poweroff(false);
        log_i("Stop Task %i", stepperid);
        taskRunning[stepperid] = false;
        vTaskDelete(NULL);
    }

    void Enable(bool en)
    {
        if (en)
        {
            poweron();
        }
        else
            poweroff(true);
    }

    void updateData(int val)
    {
        getData()[val]->currentPosition = steppers[val]->currentPosition();
    }

    void setExternalCallForPin(
        bool (*func)(uint8_t pin, uint8_t value))
    {
        _externalCallForPin = func;
    }
}
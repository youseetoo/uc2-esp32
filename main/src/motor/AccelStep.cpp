#include <PinConfig.h>

#include "AccelStep.h"
#include "FocusMotor.h"
#include "esp_task_wdt.h"
#ifdef TMC_CONTROLLER
#include "../tmc/TMCController.h"
#endif

using namespace FocusMotor;
#ifdef TMC_CONTROLLER
using namespace TMCController;
#endif

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
#else
            pinMode(pinConfig.MOTOR_ENABLE, OUTPUT);
            digitalWrite(pinConfig.MOTOR_ENABLE, HIGH ^ pinConfig.MOTOR_ENABLE_INVERTED);
#endif
            power_enable = true;
            log_i("poweron motors");
        }
    }

    void poweroff(bool force)
    {
        bool allStopped = true;
        if (!force) {
            const int axes[] = { Stepper::A, Stepper::X, Stepper::Y, Stepper::Z };
            for (int i = 0; i < 4; i++) {
                MotorData *md = getData()[axes[i]];
                if (md && !md->stopped) { allStopped = false; break; }
            }
        }
        if (force || (allStopped && power_enable))
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

        if (getData()[i] ->acceleration >= 0){
            getData()[i] ->isaccelerated = true; 
        }

        log_d("start rotator: motor:%i, isforver:%i, speed: %i, maxSpeed: %i, target pos: %i, isabsolute: %i, isacceleration: %i, acceleration: %i",
              i,
              getData()[i]->isforever,
              getData()[i]->speed,
              getData()[i]->maxspeed,
              getData()[i]->targetPosition,
              getData()[i]->absolutePosition,
              getData()[i]->isaccelerated,
              getData()[i]->acceleration);

            // if the motor speed is above threshold, maximise motor current to avoid stalling
            // We rather do that to not change motor current with any call (e.g. PS, Encoder, etc)
#ifdef TMC_CONTROLLER // TODO: This is only working on TMC2209-enabled sattelite boards since we have only one TMC Controller for one motor
            // read value from preferences
            Preferences preferences;
            preferences.begin("tmc", false);
            uint16_t rmsCurrFromPref = preferences.getInt("current", pinConfig.tmc_rms_current);
            preferences.end();
            if (abs(getData()[i]->speed) > 10000)
            {
                rmsCurrFromPref = (int)((float)rmsCurrFromPref * 1.5);
                log_i("Overdrive current for motor %i: %i", i, rmsCurrFromPref);
            }
            TMCController::setTMCCurrent(rmsCurrFromPref);
#endif

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
        {
            taskRunning[i] = true;
            xTaskCreatePinnedToCore(&driveMotorALoop, "motor_task_A", pinConfig.MOTOR_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY - 1, NULL, 0);
        }
        if (i == 1 && !taskRunning[i])
        {
            taskRunning[i] = true;
            xTaskCreatePinnedToCore(&driveMotorXLoop, "motor_task_X", pinConfig.MOTOR_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY - 1, NULL, 0);
            log_i("started x task");
        }
        if (i == 2 && !taskRunning[i])
        {
            taskRunning[i] = true;
            xTaskCreatePinnedToCore(&driveMotorYLoop, "motor_task_Y", pinConfig.MOTOR_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY - 1, NULL, 0);
        }
        if (i == 3 && !taskRunning[i])
        {
            taskRunning[i] = true;
            xTaskCreatePinnedToCore(&driveMotorZLoop, "motor_task_Z", pinConfig.MOTOR_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY - 1, NULL, 0);
        }
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
        taskRunning[i] = false;

    }

    bool isRunning(int stepperid)
    {
        if (steppers[stepperid] == nullptr)
            return false;

        AccelStepper *s = steppers[stepperid];
        return s->isRunning();
    }

    void driveMotorLoop(int stepperid)
    {
        poweron();
        AccelStepper *s = steppers[stepperid];
        s->setMaxSpeed(getData()[stepperid]->maxspeed);
        log_i("Start Task %i", stepperid);

        long tNow = millis();
        while (!getData()[stepperid]->stopped)
        {

            if (getData()[stepperid]->isforever)
            {
                //log_i("run forever stepper %i at speed %i", stepperid, getData()[stepperid]->speed);
                s->setSpeed(getData()[stepperid]->speed);
                s->runSpeed();
            }
            else
            {
                //log(i "run stepper %i to position %i at speed %i", stepperid, getData()[stepperid]->targetPosition, getData()[stepperid]->speed);
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

            if(millis() - tNow > 10){
                // do less updates to speed up things
                getData()[stepperid]->currentPosition = s->currentPosition();
                tNow = millis();
            }

            // FEED THE WATCHDOG periodically (not every step)
            static uint32_t s_lastWdt[4] = {0,0,0,0};
            uint32_t nowMs = millis();
            if (stepperid >= 0 && stepperid < 4 && nowMs - s_lastWdt[stepperid] > 50) {
                s_lastWdt[stepperid] = nowMs;
                esp_task_wdt_reset();
                vTaskDelay(1);   // yield to lower-prio tasks ~20Hz
            } else {
                taskYIELD();     // yield to same-prio tasks without blocking
            }
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
        // check if stepper is running and update the position in the data struct
         AccelStepper *s = steppers[val];
         if (s) 
        {
        getData()[val]->currentPosition = steppers[val]->currentPosition();
        }
    }

    void setExternalCallForPin(
        bool (*func)(uint8_t pin, uint8_t value))
    {
        _externalCallForPin = func;
    }
}
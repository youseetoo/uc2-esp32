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
        if (i < 0 || i >= (int)steppers.size() || !steppers[i]) {
            log_e("startAccelStepper: invalid stepper %d", i);
            return;
        }
        MotorData *d = getData()[i];
        if (!d) { log_e("startAccelStepper: null MotorData %d", i); return; }

        // Mode selection:
        //   forever            -> constant speed, no target  (runSpeed loop)
        //   accel & target     -> trapezoidal profile        (s->run())
        //   const & target     -> constant speed to target   (runSpeed until distanceToGo==0)
        bool useAccel = (d->acceleration > 0) && !d->isforever;
        d->isaccelerated = useAccel;

        log_d("start motor %d: mode=%s speed=%ld maxspeed=%ld target=%d abs=%d accel=%ld forever=%d",
              i,
              useAccel ? "accel" : (d->isforever ? "forever" : "const"),
              (long)d->speed, (long)d->maxspeed,
              d->targetPosition, (int)d->absolutePosition,
              (long)d->acceleration, (int)d->isforever);

#ifdef TMC_CONTROLLER
        // Bump TMC current at high speeds to avoid stalling
        Preferences preferences;
        preferences.begin("tmc", false);
        uint16_t rmsCurrFromPref = preferences.getInt("current", pinConfig.tmc_rms_current);
        preferences.end();
        if (abs((long)d->speed) > 10000) {
            rmsCurrFromPref = (uint16_t)((float)rmsCurrFromPref * 1.5f);
            log_i("Overdrive current for motor %i: %i", i, rmsCurrFromPref);
        }
        TMCController::setTMCCurrent(rmsCurrFromPref);
#endif

        if (d->directionPinInverted)
            steppers[i]->setPinsInverted(true, false, false);

        // Sanitize: setMaxSpeed needs positive value
        long reqSpeed = (long)abs(d->speed);
        long maxSpd   = (long)abs(d->maxspeed);
        if (maxSpd <= 0) maxSpd = MAX_VELOCITY_A;
        long appliedMax = (reqSpeed > 0 && reqSpeed < maxSpd) ? reqSpeed : maxSpd;

        steppers[i]->setMaxSpeed((float)appliedMax);

        if (d->isforever) {
            // Constant speed forever; sign of d->speed sets direction
            steppers[i]->setSpeed((float)d->speed);
        }
        else if (useAccel) {
            steppers[i]->setAcceleration((float)d->acceleration);
            if (d->absolutePosition) {
                if (d->currentPosition == d->targetPosition) {
                    d->stopped = true;
                    return;
                }
                steppers[i]->moveTo(d->targetPosition);
            } else {
                long rel = d->targetPosition;
                // Honor speed sign as direction override in relative mode
                if ((d->speed < 0 && rel > 0) || (d->speed > 0 && rel < 0))
                    rel = -rel;
                steppers[i]->move(rel);
            }
        }
        else {
            // Constant speed to target (no acceleration ramp)
            long signedSpeed = (d->speed != 0) ? (long)d->speed : appliedMax;
            if (d->absolutePosition) {
                if (d->currentPosition == d->targetPosition) {
                    d->stopped = true;
                    return;
                }
                long delta = (long)d->targetPosition - (long)d->currentPosition;
                if ((delta > 0 && signedSpeed < 0) || (delta < 0 && signedSpeed > 0))
                    signedSpeed = -signedSpeed;
                steppers[i]->moveTo(d->targetPosition);
            } else {
                long rel = d->targetPosition;
                if ((rel > 0 && signedSpeed < 0) || (rel < 0 && signedSpeed > 0))
                    rel = -rel;
                steppers[i]->move(rel);
            }
            steppers[i]->setSpeed((float)signedSpeed);
        }

        d->stopped = false;
        d->isStop  = false;

        TaskFunction_t fn = nullptr;
        const char *name = nullptr;
        switch (i) {
            case 0: fn = &driveMotorALoop; name = "motor_task_A"; break;
            case 1: fn = &driveMotorXLoop; name = "motor_task_X"; break;
            case 2: fn = &driveMotorYLoop; name = "motor_task_Y"; break;
            case 3: fn = &driveMotorZLoop; name = "motor_task_Z"; break;
        }
        if (fn && !taskRunning[i]) {
            taskRunning[i] = true;
            // Full DEFAULT priority (not -1) for stable step timing
            xTaskCreatePinnedToCore(fn, name, pinConfig.MOTOR_TASK_STACKSIZE, NULL,
                                    pinConfig.DEFAULT_TASK_PRIORITY, NULL, 0);
        }
    }


    void setPosition(Stepper axis, int pos){
        getData()[axis]->currentPosition = pos;
        steppers[axis]->setCurrentPosition(pos);
    }

    void stopAccelStepper(int i)
    {
        log_i("stop stepper %i", i);
        if (i < 0 || i >= (int)steppers.size()) return;
        if (steppers[i]) steppers[i]->stop();
        MotorData *d = getData()[i];
        if (d) {
            d->isforever = false;
            d->speed = 0;
            if (steppers[i]) d->currentPosition = steppers[i]->currentPosition();
            d->stopped = true;
        }
        // taskRunning is cleared by the task itself when it exits its while-loop
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
        AccelStepper *s = steppers[stepperid];
        MotorData    *d = getData()[stepperid];
        if (!s || !d) {
            taskRunning[stepperid] = false;
            vTaskDelete(NULL);
            return;
        }

        poweron();
        log_i("Start Task %d", stepperid);

        const uint32_t WDT_PERIOD_MS = 50;
        const uint32_t POS_PERIOD_MS = 20;
        uint32_t lastWdt = millis();
        uint32_t lastPos = millis();
        bool     targetReached = false;

        while (!d->stopped && !d->isStop)
        {
            if (d->isforever) {
                // Constant speed forever; pick up runtime speed updates
                s->setSpeed((float)d->speed);
                s->runSpeed();
            }
            else if (d->isaccelerated) {
                // Trapezoidal profile to target — run() returns false when done
                if (!s->run()) {
                    targetReached = true;
                    break;
                }
            }
            else {
                // Constant speed to target
                if (s->distanceToGo() == 0) {
                    targetReached = true;
                    break;
                }
                s->runSpeed();
            }

            uint32_t nowMs = millis();
            if (nowMs - lastPos >= POS_PERIOD_MS) {
                d->currentPosition = s->currentPosition();
                lastPos = nowMs;
            }
            if (nowMs - lastWdt >= WDT_PERIOD_MS) {
                esp_task_wdt_reset();
                lastWdt = nowMs;
                vTaskDelay(1);   // ~20 Hz yield to lower-prio tasks
            }
            // No per-step taskYIELD — keeps step timing stable.
        }

        d->currentPosition = s->currentPosition();
        d->stopped = true;

        log_i("Stop Task %d (targetReached=%d)", stepperid, (int)targetReached);
        poweroff(false);
        taskRunning[stepperid] = false;

        // Notify upper layer so QID is reported done. Slave's FocusMotor::loop
        // doesn't always observe the stop window in time on a single-axis node.
        if (targetReached)
            FocusMotor::stopStepper(stepperid);

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
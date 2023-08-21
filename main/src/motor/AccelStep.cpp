#include "AccelStep.h"
#include "ModuleController.h"
#include "FocusMotor.h"

void driveMotorXLoop(void *pvParameter)
{
    FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
    motor->accel.driveMotorLoop(Stepper::X);
}
void driveMotorYLoop(void *pvParameter)
{
    FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
    motor->accel.driveMotorLoop(Stepper::Y);
}
void driveMotorZLoop(void *pvParameter)
{
    FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
    motor->accel.driveMotorLoop(Stepper::Z);
}
void driveMotorALoop(void *pvParameter)
{
    FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
    motor->accel.driveMotorLoop(Stepper::A);
}

void AccelStep::setupAccelStepper()
{
    for (int i = 0; i < taskRunning.size(); i++)
    {
        taskRunning[i] = false;
    }
    if (pinConfig.I2C_SCL > 0)
    {
        if (pinConfig.MOTOR_A_STEP > -1)
        {
            steppers[Stepper::A] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_A_STEP, 104 | PIN_EXTERNAL_FLAG, -1, -1, true, _externalCallForPin);
        }
        if (pinConfig.MOTOR_X_STEP > -1)
        {
            steppers[Stepper::X] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_X_STEP, 101 | PIN_EXTERNAL_FLAG, -1, -1, true, _externalCallForPin);
            steppers[Stepper::X]->setEnablePin(100 | PIN_EXTERNAL_FLAG);
            steppers[Stepper::X]->setPinsInverted(false, false, true);
        }
        if (pinConfig.MOTOR_Y_STEP > -1)
        {
            steppers[Stepper::Y] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_Y_STEP, 102 | PIN_EXTERNAL_FLAG, -1, -1, true, _externalCallForPin);
        }
        if (pinConfig.MOTOR_Z_STEP > -1)
        {
            steppers[Stepper::Z] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_Z_STEP, 103 | PIN_EXTERNAL_FLAG, -1, -1, true, _externalCallForPin);
        }
    }
    else
    {
        if (pinConfig.AccelStepperMotorType == AccelStepper::DRIVER)
        {
            if (pinConfig.MOTOR_A_STEP > -1)
                steppers[Stepper::A] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_A_STEP, pinConfig.MOTOR_A_DIR);
            if (pinConfig.MOTOR_X_STEP > -1)
                steppers[Stepper::X] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_X_STEP, pinConfig.MOTOR_X_DIR);
            if (pinConfig.MOTOR_Y_STEP > -1)
                steppers[Stepper::Y] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_Y_STEP, pinConfig.MOTOR_Y_DIR);
            if (pinConfig.MOTOR_Z_STEP > -1)
                steppers[Stepper::Z] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_Z_STEP, pinConfig.MOTOR_Z_DIR);
        }
        else if (pinConfig.AccelStepperMotorType == AccelStepper::HALF4WIRE)
        {
            if (pinConfig.MOTOR_A_STEP > -1)
                steppers[Stepper::A] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.MOTOR_A_STEP, pinConfig.MOTOR_A_DIR, pinConfig.MOTOR_A_0, pinConfig.MOTOR_A_1);
            if (pinConfig.MOTOR_X_STEP > -1)
                steppers[Stepper::X] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.MOTOR_X_STEP, pinConfig.MOTOR_X_DIR, pinConfig.MOTOR_X_0, pinConfig.MOTOR_X_1);
            if (pinConfig.MOTOR_Y_STEP > -1)
                steppers[Stepper::Y] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.MOTOR_Y_STEP, pinConfig.MOTOR_Y_DIR, pinConfig.MOTOR_Y_0, pinConfig.MOTOR_Y_1);
            if (pinConfig.MOTOR_Z_STEP > -1)
                steppers[Stepper::Z] = new AccelStepper(AccelStepper::HALF4WIRE, pinConfig.MOTOR_Z_STEP, pinConfig.MOTOR_Z_DIR, pinConfig.MOTOR_Z_0, pinConfig.MOTOR_Z_1);
        }
    }

    /*
       Motor related settings
    */

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

            steppers[i]->setCurrentPosition(data[i]->currentPosition);
        }
    }
}

void AccelStep::startAccelStepper(int i)
{
    // if (!data[i]->stopped)
    //     stopAccelStepper(i);
    log_d("start rotator: motor:%i, isforver:%i, speed: %i, maxSpeed: %i, target pos: %i, isabsolute: %i, isacceleration: %i, acceleration: %i",
          i,
          data[i]->isforever,
          data[i]->speed,
          data[i]->maxspeed,
          data[i]->targetPosition,
          data[i]->absolutePosition,
          data[i]->isaccelerated,
          data[i]->acceleration);
    if (!data[i]->isforever)
    {
        // accelstepper wants in relative mode that targetpostion and speed point into same direction
        if (!data[i]->absolutePosition && ((data[i]->targetPosition < 0 && data[i]->speed > 0) || (data[i]->targetPosition > 0 && data[i]->speed < 0)))
            data[i]->speed *= -1;
        // in absolute mode speed direction is up to the target and current position difference
        if (data[i]->absolutePosition)
        {
            int speeddir = data[i]->targetPosition - data[i]->currentPosition;
            if ((speeddir > 0 && data[i]->speed < 0) || (speeddir < 0 && data[i]->speed > 0))
                data[i]->speed *= -1;
        }
    }
    steppers[i]->setMaxSpeed(data[i]->maxspeed);
    steppers[i]->setAcceleration(data[i]->acceleration);
    if (data[i]->absolutePosition)
    {
        if(data[i]->currentPosition == data[i]->targetPosition)
            return;
        // absolute position coordinates
        steppers[i]->moveTo(data[i]->targetPosition);
    }
    else
    {
        // relative position coordinates
        steppers[i]->move(data[i]->targetPosition);
    }
    data[i]->stopped = false;

    if (i == 0 && !taskRunning[i])
        xTaskCreate(&driveMotorALoop, "motor_task_A", 2024, NULL, 5, NULL);
    if (i == 1 && !taskRunning[i])
    {
        xTaskCreate(&driveMotorXLoop, "motor_task_X", 2024, NULL, 5, NULL);
        log_i("started x task");
    }
    // else
    //     log_i("x wont start");
    if (i == 2 && !taskRunning[i])
        xTaskCreate(&driveMotorYLoop, "motor_task_Y", 2024, NULL, 5, NULL);
    if (i == 3 && !taskRunning[i])
        xTaskCreate(&driveMotorZLoop, "motor_task_Z", 2024, NULL, 5, NULL);
}

void AccelStep::stopAccelStepper(int i)
{
    steppers[i]->stop();
    data[i]->isforever = false;
    data[i]->speed = 0;
    data[i]->currentPosition = steppers[i]->currentPosition();
    data[i]->stopped = true;
}

bool AccelStep::isRunning(int stepperid){
    AccelStepper *s = steppers[stepperid];
    MotorData *d = data[stepperid];
    return s->isRunning();
}

void AccelStep::driveMotorLoop(int stepperid)
{
    taskRunning[stepperid] = true;
    AccelStepper *s = steppers[stepperid];
    MotorData *d = data[stepperid];
    log_i("Start Task %i", stepperid);
    while (!d->stopped)
    {
        s->setMaxSpeed(d->maxspeed);
        if (d->isforever)
        {
            s->setSpeed(d->speed);
            s->runSpeed();
        }
        else
        {
            if (!data[stepperid]->isaccelerated)
                s->setSpeed(d->speed);
            if (!s->run())
                stopAccelStepper(stepperid);
            // checks if a stepper is still running
            //log_i("distance to go:%i", s->distanceToGo());
            if (s->distanceToGo() == 0 && !d->stopped)
            {
                log_i("stop stepper:%i", stepperid);
                // if not turn it off
                stopAccelStepper(stepperid);
            }
        }
        d->currentPosition = s->currentPosition();
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    d->stopped = true;
    log_i("Stop Task %i", stepperid);
    taskRunning[stepperid] = false;
    vTaskDelete(NULL);
}

void AccelStep::Enable(bool en)
{
    for (int i = 0; i < steppers.size(); i++)
    {
        if (steppers[i] != nullptr)
        {
            if (en)
            {
                steppers[i]->enableOutputs();
            }
            else
                steppers[i]->disableOutputs();
        }
    }
}

void AccelStep::updateData(int val)
{
    data[val]->currentPosition = steppers[val]->currentPosition();
}

void AccelStep::setExternalCallForPin(
    bool (*func)(uint8_t pin, uint8_t value))
{
    _externalCallForPin = func;
}
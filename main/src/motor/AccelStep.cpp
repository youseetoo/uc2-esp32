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

    if (pinConfig.I2C_SCL > 0)
    {
        if (pinConfig.MOTOR_A_STEP > -1)
        {
            steppers[Stepper::A] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_A_STEP, 104 | PIN_EXTERNAL_FLAG);
            steppers[Stepper::A]->setExternalCallForPin(_externalCallForPin);
        }
        if (pinConfig.MOTOR_X_STEP > -1)
        {
            steppers[Stepper::X] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_X_STEP, 101 | PIN_EXTERNAL_FLAG);
            steppers[Stepper::X]->setExternalCallForPin(_externalCallForPin);
            steppers[Stepper::X]->setEnablePin(100 | PIN_EXTERNAL_FLAG);
            steppers[Stepper::X]->setPinsInverted(false, false, false);
        }
        if (pinConfig.MOTOR_Y_STEP > -1)
        {
            steppers[Stepper::Y] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_Y_STEP, 102 | PIN_EXTERNAL_FLAG);
            steppers[Stepper::Y]->setExternalCallForPin(_externalCallForPin);
        }
        if (pinConfig.MOTOR_Z_STEP > -1)
        {
            steppers[Stepper::Z] = new AccelStepper(AccelStepper::DRIVER, pinConfig.MOTOR_Z_STEP, 103 | PIN_EXTERNAL_FLAG);
            steppers[Stepper::Z]->setExternalCallForPin(_externalCallForPin);
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
    if (pinConfig.MOTOR_A_DIR > -1)
        xTaskCreate(&driveMotorALoop, "motor_task_A", 1024, NULL, 5, NULL);
    if (pinConfig.MOTOR_X_DIR > -1)
        xTaskCreate(&driveMotorXLoop, "motor_task_X", 1024, NULL, 5, NULL);
    if (pinConfig.MOTOR_Y_DIR > -1)
        xTaskCreate(&driveMotorYLoop, "motor_task_Y", 1024, NULL, 5, NULL);
    if (pinConfig.MOTOR_Z_DIR > -1)
        xTaskCreate(&driveMotorZLoop, "motor_task_Z", 1024, NULL, 5, NULL);
}

void AccelStep::startAccelStepper(int i)
{
    log_d("start rotator: motor:%i, index: %i isforver:%i, speed: %i, maxSpeed: %i, steps: %i, isabsolute: %i, isacceleration: %i, acceleration: %i", i, data[i]->isforever, data[i]->speed, data[i]->maxspeed, data[i]->targetPosition, data[i]->absolutePosition, data[i]->isaccelerated, data[i]->acceleration);
    steppers[i]->setMaxSpeed(data[i]->maxspeed);
    steppers[i]->setAcceleration(data[i]->acceleration);
    if (data[i]->absolutePosition == 1)
    {
        // absolute position coordinates
        steppers[i]->moveTo(data[i]->targetPosition);
    }
    else if (!data[i]->isforever)
    {
        // relative coordinates
        steppers[i]->move(data[i]->targetPosition);
    }
    data[i]->stopped = false;
}

void AccelStep::stopAccelStepper(int i)
{
    steppers[i]->stop();
    data[i]->isforever = false;
    data[i]->speed = 0;
    data[i]->currentPosition = steppers[i]->currentPosition();
    data[i]->stopped = true;
}

void AccelStep::driveMotorLoop(int stepperid)
{
    AccelStepper *s = steppers[stepperid];
    MotorData *d = data[stepperid];
    for (;;)
    {
        s->setMaxSpeed(d->maxspeed);
        if (d->isforever)
        {
            s->setSpeed(d->speed);
            s->runSpeed();
        }
        else
        {

            if (d->absolutePosition)
            {
                // absolute position coordinates
                s->moveTo(d->targetPosition);
            }
            else
            {
                // relative position coordinates
                s->move(d->targetPosition);
            }
            // checks if a stepper is still running
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
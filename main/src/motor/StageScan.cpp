#include "StageScan.h"
#include "Arduino.h"
#include "FocusMotor.h"
#include "../i2c/tca_controller.h"
#ifdef LED_CONTROLLER
#include "../led/LedController.h"
#endif
#ifdef LASER_CONTROLLER
#include "../laser/LaserController.h"
#endif
#ifdef CAN_CONTROLLER
#include "../can/can_controller.h"
#endif
namespace StageScan
{
    StageScanningData stageScanningData;

    StageScanningData *getStageScanData()
    {
        return &stageScanningData;
    }

    void moveMotor(int stepPin, int dirPin, int steps, bool direction, int delayTimeStep)
    {
        steps = abs(steps);

        //  direction perhaps externally controlled
        if (pinConfig.I2C_SCL > -1)
        {
#ifdef USE_TCA
            tca_controller::setExternalPin(dirPin, direction);
#endif
        }
        else
        {
            digitalWrite(dirPin, direction ? HIGH : LOW);
        }

        for (int i = 0; i < steps; ++i)
        {
            digitalWrite(stepPin, HIGH);
            ets_delay_us(delayTimeStep); // Adjust delay for speed
            digitalWrite(stepPin, LOW);
            ets_delay_us(delayTimeStep); // Adjust delay for speed
        }
    }

    void writeToPin(int digitaloutid, int digitaloutval, int triggerdelay)
    {
        bool isInverted = pinConfig.CAMERA_TRIGGER_INVERTED;

        if (digitaloutval == -1)
        {
            // perform trigger
            digitalWrite(digitaloutid, isInverted ? LOW : HIGH);
            delay(triggerdelay);
            digitalWrite(digitaloutid, isInverted ? HIGH : LOW);
        }
        else
        {
            digitalWrite(digitaloutid, isInverted ? !digitaloutval : digitaloutval);
        }
    }

    void triggerOutput(int outputPin, int triggerTime = 10, int state = -1)
    {
        // if state is -1 we alternate from 0,1,0
        // Output trigger logic
        if (state == -1)
        {
            writeToPin(outputPin, 1, 0);
            ets_delay_us(triggerTime); // Adjust delay for speed
            writeToPin(outputPin, 0, 0);
        }
        else
        {
            writeToPin(outputPin, state, 0);
        }
    }

    void stageScan(bool isThread)
    {

        FocusMotor::setEnable(true);
        // Scanning logic
        int nStepsLine = stageScanningData.nStepsLine;
        int dStepsLine = stageScanningData.dStepsLine;
        int nStepsPixel = stageScanningData.nStepsPixel;
        int dStepsPixel = stageScanningData.dStepsPixel;
        int nTriggerPixel = stageScanningData.nTriggerPixel;
        int delayTimeStep = stageScanningData.delayTimeStep;
        int nFrames = stageScanningData.nFrames;

        int pinDirPixel = FocusMotor::getData()[Stepper::X]->dirPin;
        int pinDirLine = FocusMotor::getData()[Stepper::Y]->dirPin;
        int pinStpPixel = FocusMotor::getData()[Stepper::X]->stpPin;
        int pinStpLine = FocusMotor::getData()[Stepper::Y]->stpPin;
        int pinTrigPixel = FocusMotor::getData()[Stepper::X]->triggerPin;
        int pinTrigLine = FocusMotor::getData()[Stepper::Y]->triggerPin;
        int pinTrigFrame = FocusMotor::getData()[Stepper::Z]->triggerPin;

        for (int iFrame = 0; iFrame < nFrames; iFrame++)
        {
            // frameclock

            int stepCounterPixel = 0;
            int stepCounterLine = 0;
            bool directionX = 0;
            int iPixel = 0;

            // Set pins high simultaenously at frame start
            uint32_t mask = (1 << pinConfig.DIGITAL_OUT_1) | (1 << pinConfig.DIGITAL_OUT_2) | (1 << pinConfig.DIGITAL_OUT_3);
            GPIO.out_w1ts = mask; // all high

            for (int iLine = 0; iLine < nStepsLine; iLine += dStepsLine)
            {
                // pulses the line trigger (or switch off in the first round)
                triggerOutput(pinTrigLine);

                for (iPixel = 0; iPixel < nStepsPixel; iPixel += dStepsPixel)
                {

                    if (stageScanningData.stopped)
                    {
                        break;
                    }
                    // pulses the pixel trigger (or switch off in the first round)
                    triggerOutput(pinTrigPixel);

                    // Move X motor forward at even steps, backward at odd steps
                    // bool directionX = iLine % 2 == 0;
                    moveMotor(pinStpPixel, pinDirPixel, dStepsPixel, directionX, delayTimeStep);
                    // stepCounterPixel += (dStepsPixel * (directionX ? 1 : -1));
                }

                // move back x stepper by step counter
                moveMotor(pinStpPixel, pinDirPixel, iPixel, !directionX, (2 + delayTimeStep) * 10);

                // Move Y motor after each line
                bool directionY = 0;
                moveMotor(pinStpLine, pinDirLine, dStepsLine, directionY, delayTimeStep);
            }

            // Reset Position and move back to origin

            triggerOutput(pinTrigFrame, 0);

            moveMotor(pinStpLine, pinDirLine, nStepsLine, stepCounterLine > 0, (2 + delayTimeStep) * 10);
            FocusMotor::getData()[Stepper::X]->currentPosition -= stepCounterPixel;
            FocusMotor::getData()[Stepper::Y]->currentPosition -= stepCounterLine;
            ets_delay_us(10000); // Adjust delay for speed
        }

        if (isThread)
            vTaskDelete(NULL);
    }

    void stageScanThread(void *arg)
    {
#if defined(CAN_MASTER)
        stageScanCAN(true);
#else
        stageScan(true);
#endif
    }

    // -----------------------------------------------------------------------------
    // CAN-MASTER implementation – absolute positioning sent to the slaves
    // -----------------------------------------------------------------------------
    static inline void moveAbs(Stepper ax, int32_t pos, int speed = 20000, int acceleration = 1000000, int32_t timeout = 2000)
    {
#if defined CAN_CONTROLLER && !defined CAN_SLAVE_MOTOR
        auto *d = FocusMotor::getData()[ax];
        d->absolutePosition = 1;
        d->targetPosition = pos;
        d->speed = speed;
        d->isStop = 0;
        d->stopped = false;
        d->acceleration = acceleration;
        d->isforever = false;
        log_i("Moving axis %d to position %d with speed %d and acceleration %d, isAbs%d", ax, pos, speed, acceleration, d->absolutePosition);
        FocusMotor::startStepper(ax, 1);
        // give some time to send the command and wait for the motor to start e.g. 100ms
        vTaskDelay(pdMS_TO_TICKS(100));
        // we need a timeout to wait for the motor to stop
        int32_t timeStart = millis();
        while (1)
        {
            vTaskDelay(1);
            if (d->stopped or FocusMotor::getData()[ax]->currentPosition == pos or (millis() - timeStart) > timeout)
            { // !FocusMotor::isRunning(ax)){
                // motor is running, we can break the loop
                // Serial.println("Motor done, took us " + String(millis() - timeStart) + "ms");
                break;
            }
            // Serial.println("Motor stopped"); //TODO: This is not working as expected - I guess status is not correct here
        }
#endif
    }

    void stageScanCAN(bool isThread)
    {
#if defined CAN_CONTROLLER && !defined CAN_SLAVE_MOTOR
        auto &sd = StageScan::stageScanningData;
        // get current position
        long currentPosX = FocusMotor::getData()[Stepper::X]->currentPosition;
        long currentPosY = FocusMotor::getData()[Stepper::Y]->currentPosition;
        int32_t x0 = currentPosX;
        int32_t y0 = currentPosY;
        int32_t key_speed = sd.speed;
        int32_t key_acceleration = sd.acceleration;
        isRunning = true;

        // if we don't provide a start position, we take the current position and scan around it
        if (sd.xStart != 0)
            x0 = sd.xStart;
        if (sd.yStart != 0)
            y0 = sd.yStart;
        // set the stepper to absolute position
        pinMode(pinConfig.CAMERA_TRIGGER_PIN, OUTPUT);
        digitalWrite(pinConfig.CAMERA_TRIGGER_PIN, pinConfig.CAMERA_TRIGGER_INVERTED ? HIGH : LOW); // set the trigger pin to high
        log_i("Moving to start position X: %d, Y: %d, currentposition X: %d, Y: %d", x0, y0, currentPosX, currentPosY);
        moveAbs(Stepper::X, x0, key_speed, key_acceleration);
        moveAbs(Stepper::Y, y0, key_speed, key_acceleration);
        // move to the start position
        for (uint16_t iy = 0; iy < sd.nY && !sd.stopped; ++iy)
        {
            bool rev = (iy & 1);                                   // are we on an even or odd line?
            for (uint16_t ix = 0; ix < sd.nX && !sd.stopped; ++ix) //
            {
                log_i("Moving X to %d", x0 + int32_t(ix) * sd.xStep);
                uint16_t j = rev ? (sd.nX - 1 - ix) : ix; // reverse the order of the motion
                int32_t tgtX = x0 + int32_t(j) * sd.xStep;
                log_i("Moving X to %d", tgtX);
                moveAbs(Stepper::X, tgtX, key_speed, key_acceleration);
// if we have an illumination array, we need to set the led
#ifdef LED_CONTROLLER
                if (sd.ledarrayIntensity > 0)
                {
                    // TODO: Generalize this:
                    LedCommand cmd;
                    cmd.mode = LedMode::CIRCLE;
                    cmd.r = sd.ledarrayIntensity; // pinConfig.JOYSTICK_MAX_ILLU;
                    cmd.g = sd.ledarrayIntensity; // pinConfig.JOYSTICK_MAX_ILLU;
                    cmd.b = sd.ledarrayIntensity; // pinConfig.JOYSTICK_MAX_ILLU;
                    cmd.radius = 8;               // radius of the circle
                    cmd.ledIndex = 0;             // not used
                    cmd.region[0] = 0;            // not used
                    cmd.qid = 0;                  // not used
                    can_controller::sendLedCommandToCANDriver(cmd, pinConfig.CAN_ID_LED_0);
                    vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                    StageScan::triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                    vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));

                    // TODO: We need to generalize this led interface to make the same function for both I2C, CAN and native GPIO
                    // trigger LED on board so that we indicate we take images
                    if (0)
                    {
                        // this is probably expensive, so we only do it if we have a camera trigger
                        LedController::fillAll(sd.ledarrayIntensity, sd.ledarrayIntensity, sd.ledarrayIntensity);
                        LedController::fillAll(0, 0, 0);
                    }
                    cmd.r = 0;
                    cmd.g = 0;
                    cmd.b = 0; // switch off the LED
                    can_controller::sendLedCommandToCANDriver(cmd, pinConfig.CAN_ID_LED_0);
                }
#endif
#ifdef LASER_CONTROLLER
                for (int j = 0; j < 4; ++j)
                {
                    if (sd.lightsourceIntensities[j] > 0)
                    {
// TODO: We need to generalize this laser interface to make the same function for both I2C, CAN and native GPIO
#if defined CAN_CONTROLLER && not defined(CAN_SLAVE_LASER)
                        LaserData laserData;
                        laserData.LASERid = j;
                        laserData.LASERval = sd.lightsourceIntensities[j];
                        can_controller::sendLaserDataToCANDriver(laserData);
                        vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                        StageScan::triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                        vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
                        laserData.LASERval = 0;
                        can_controller::sendLaserDataToCANDriver(laserData);
#endif
                    }
                }
#endif
                // if we have a lightsource array, we still want to trigger the camera
                if (sd.ledarrayIntensity == 0 && sd.lightsourceIntensities[0] == 0 && sd.lightsourceIntensities[1] == 0 && sd.lightsourceIntensities[2] == 0 && sd.lightsourceIntensities[3] == 0)
                {
                    // trigger the camera
                    vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                    StageScan::triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                    vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
                }
            }
            if (iy + 1 == sd.nY || sd.stopped)
                break;

            // move to the next line
            log_i("Moving Y to %d", y0 + int32_t(iy + 1) * sd.yStep);
            moveAbs(Stepper::Y, y0 + int32_t(iy + 1) * sd.yStep, key_speed, key_acceleration);
            
        }

        // move back to the original position
        //moveAbs(Stepper::X, currentPosX, key_speed, key_acceleration, 0); // give it some time to move back
        //moveAbs(Stepper::Y, currentPosY, key_speed, key_acceleration,0);
        // Send a message over serial to indicate we are done
        cJSON *json = cJSON_CreateObject();
        cJSON *stagescan = cJSON_CreateObject();
        cJSON_AddItemToObject(json, "stagescan", stagescan);
        cJsonTool::setJsonInt(json, keyQueueID, sd.qid);
        cJsonTool::setJsonInt(json, "success", 1);
        Serial.println("++");
        char *ret = cJSON_PrintUnformatted(json);
        Serial.println(ret);
        cJSON_Delete(json);
        free(ret);
        Serial.println("--");

        if (isThread)
            vTaskDelete(NULL);
#endif
        isRunning = false;

    }
    // CAN_MASTER

} // namespace FocusMotor

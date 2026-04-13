#include "StageScan.h"
#include "Arduino.h"
#include "FocusMotor.h"
#include "../i2c/tca_controller.h"
#include "../../JsonKeys.h"
#include "../../cJsonTool.h"
#ifdef LED_CONTROLLER
#include "../led/LedController.h"
#endif
#ifdef LASER_CONTROLLER
#include "../laser/LaserController.h"
#endif
#ifdef CAN_BUS_ENABLED
#include "../can/can_controller.h"
#endif

namespace StageScan
{
    StageScanningData stageScanningData;

    StageScanningData *getStageScanData()
    {
        return &stageScanningData;
    }

    void setCoordinates(StagePosition *coords, int count)
    {
        // Clear existing coordinates first
        clearCoordinates();
        log_i("Setting %d coordinates for stage scanning", count);

        if (coords != nullptr && count > 0)
        {
            stageScanningData.coordinates = new StagePosition[count];
            for (int i = 0; i < count; i++)
            {
                log_i("Coordinate %d: x=%d, y=%d", i, coords[i].x, coords[i].y);
                stageScanningData.coordinates[i] = coords[i];
            }
            stageScanningData.coordinateCount = count;
            stageScanningData.useCoordinates = true;
        }
    }

    void clearCoordinates()
    {
        if (stageScanningData.coordinates != nullptr)
        {
            delete[] stageScanningData.coordinates;
            stageScanningData.coordinates = nullptr;
        }
        stageScanningData.coordinateCount = 0;
        stageScanningData.useCoordinates = false;
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
            // print "++{"cam":1}--" to serial to indicate a software trigger 
            ets_delay_us(4); // Adjust delay for speed
            Serial.println("++\n{\"cam\":1}\n--");
            writeToPin(outputPin, 0, 0);
        }
        else
        {
            writeToPin(outputPin, state, 0);
        }
    }

    // -----------------------------------------------------------------------------
    // CAN-MASTER implementation – absolute positioning sent to the slaves
    // -----------------------------------------------------------------------------
    static inline void moveAbs(Stepper ax, int32_t pos, int speed = 20000, int acceleration = 1000000, int32_t timeout = 2000)
    {
#ifndef CAN_RECEIVE_MOTOR
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
            if (d->stopped or FocusMotor::getData()[ax]->currentPosition == pos)
            { // !FocusMotor::isRunning(ax)){
                // motor is running, we can break the loop
                // Serial.println("Motor done, took us " + String(millis() - timeStart) + "ms");
                break;
            }
            if (millis() - timeStart > timeout)
            {
                log_e("Timeout while moving axis %d to position %d", ax, pos);
                break; // timeout reached
            }
            // Serial.println("Motor stopped"); //TODO: This is not working as expected - I guess status is not correct here
        }
#endif
    }

    // -----------------------------------------------------------------------------
    // Unified stageScan – works with both local GPIO drivers and remote CAN slaves.
    // FocusMotor::startStepper() dispatches to the right back-end automatically.
    // Use CAN_BUS_ENABLED as build flag to differentiate LED dispatch where needed.
    // -----------------------------------------------------------------------------
    void stageScan(bool isThread)
    {
        if (isRunning)
            return;
#ifndef CAN_RECEIVE_MOTOR
        auto &sd = StageScan::stageScanningData;

        FocusMotor::setEnable(true);

        // Trapezoidal motion profile: estimated travel time in ms
        auto calculateTimeToPosition = [](int32_t startPos, int32_t targetPos, int32_t speed, int32_t accel) -> uint32_t
        {
            int32_t distance = abs(targetPos - startPos);
            if (distance == 0)
                return 0;

            // Trapezoidal motion profile calculation
            // Time to accelerate to max speed
            float accelTime = (float)speed / (float)accel;
            // Distance covered during acceleration
            int32_t accelDist = (speed * speed) / (2 * accel);
            uint32_t totalTime;
            if (distance < 2 * accelDist)
            {
                float peakSpeed = sqrt((float)distance * (float)accel);
                totalTime = (uint32_t)(2.0f * peakSpeed / (float)accel * 1000.0f);
            }
            else
            {
                float constTime = (float)(distance - 2 * accelDist) / (float)speed;
                totalTime = (uint32_t)((2.0f * accelTime + constTime) * 1000.0f);
            }
            return totalTime;
        };

        // Agnostic LED helper: sends illumination command via CAN_BUS_ENABLED or local driver
        auto sendLed = [&](uint8_t intensity) {
#ifdef LED_CONTROLLER
            LedCommand cmd;
            cmd.mode = LedMode::CIRCLE;
            cmd.r = intensity;
            cmd.g = intensity;
            cmd.b = intensity;
            cmd.radius = 8;
            cmd.ledIndex = 0;
            cmd.region[0] = '\0';
            cmd.qid = 0;
#ifdef CAN_BUS_ENABLED
            can_controller::sendLedCommandToCANDriver(cmd, pinConfig.CAN_ID_LED_0);
#else
            LedController::execLedCommand(cmd);
#endif
#endif
        };

        bool zicZac = sd.zicZac;

        if (sd.useCoordinates && sd.coordinates != nullptr)
        {
            // ----------------------------------------------------------------
            // Coordinate-based scanning
            // ----------------------------------------------------------------
            isRunning = true;
            long currentPosX = FocusMotor::getData()[Stepper::X]->currentPosition;
            long currentPosY = FocusMotor::getData()[Stepper::Y]->currentPosition;
            long currentPosZ = FocusMotor::getData()[Stepper::Z]->currentPosition;
            pinMode(pinConfig.CAMERA_TRIGGER_PIN, OUTPUT);
            digitalWrite(pinConfig.CAMERA_TRIGGER_PIN, pinConfig.CAMERA_TRIGGER_INVERTED ? HIGH : LOW);
            log_i("Coordinate-based scan: %d coords, %d frames, nonstop=%d",
                  sd.coordinateCount, sd.nFrames, sd.nonstop);

            if (sd.nonstop)
            {
                // Continuous: kick motors toward final position, trigger at intermediate times
                int32_t finalX = sd.coordinates[sd.coordinateCount - 1].x;
                int32_t finalY = sd.coordinates[sd.coordinateCount - 1].y;
                int32_t startX = currentPosX;
                int32_t startY = currentPosY;
                int32_t startZ = currentPosZ;

                auto *dataX = FocusMotor::getData()[Stepper::X];
                auto *dataY = FocusMotor::getData()[Stepper::Y];
                dataX->absolutePosition = 1; dataX->targetPosition = finalX;
                dataX->speed = sd.speed;     dataX->isStop = 0;
                dataX->stopped = false;      dataX->acceleration = sd.acceleration;
                dataX->isforever = false;

                dataY->absolutePosition = 1; dataY->targetPosition = finalY;
                dataY->speed = sd.speed;     dataY->isStop = 0;
                dataY->stopped = false;      dataY->acceleration = sd.acceleration;
                dataY->isforever = false;

                uint32_t startTime = millis();
                FocusMotor::startStepper(Stepper::X, 1);
                FocusMotor::startStepper(Stepper::Y, 1);
                log_i("Continuous movement from (%ld,%ld) to (%d,%d)", currentPosX, currentPosY, finalX, finalY);

                for (int i = 0; i < sd.coordinateCount && !sd.stopped; i++)
                {
                    int32_t tX = sd.coordinates[i].x;
                    int32_t tY = sd.coordinates[i].y;
                    int32_t tZ = sd.coordinates[i].z;
                    uint32_t expectedTime = max(calculateTimeToPosition(startX, tX, sd.speed, sd.acceleration),
                                           max(calculateTimeToPosition(startY, tY, sd.speed, sd.acceleration),
                                               calculateTimeToPosition(startZ, tZ, sd.speed, sd.acceleration)));
                    uint32_t elapsedTime = millis() - startTime;
                    if (expectedTime > elapsedTime)
                    {
                        log_i("Position %d (%d,%d): waiting %u ms", i, tX, tY, expectedTime - elapsedTime);
                        vTaskDelay(pdMS_TO_TICKS(expectedTime - elapsedTime));
                    }
                    if (sd.stopped) break;
                    vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                    StageScan::triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                    vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
                }
            }
            else
            {
                // Stop-and-go: move to each coordinate, acquire, continue
                for (int i = 0; i < sd.coordinateCount && !sd.stopped; i++)
                {
                    int32_t tX = sd.coordinates[i].x;
                    int32_t tY = sd.coordinates[i].y;
                    int32_t tZ = sd.coordinates[i].z;
                    log_i("Moving to coordinate X:%d Y:%d Z:%d", tX, tY, tZ);
                    moveAbs(Stepper::X, tX, sd.speed, sd.acceleration);
                    moveAbs(Stepper::Y, tY, sd.speed, sd.acceleration);
                    moveAbs(Stepper::Z, tZ, sd.speed, sd.acceleration);

#ifdef LED_CONTROLLER
                    if (sd.ledarrayIntensity > 0)
                    {
                        sendLed(sd.ledarrayIntensity);
                        vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                        StageScan::triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                        vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
                        sendLed(0);
                    }
#endif
#ifdef LASER_CONTROLLER
                    for (int j = 0; j < 4; ++j)
                    {
                        if (sd.lightsourceIntensities[j] > 0)
                        {
                            // LaserController::setLaserVal dispatches to CAN or native PWM internally
                            LaserController::setLaserVal(j, sd.lightsourceIntensities[j]);
                            vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                            StageScan::triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                            vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
                            LaserController::setLaserVal(j, 0);
                        }
                    }
#endif
                    // No light source configured: still trigger camera
                    if (sd.ledarrayIntensity == 0 &&
                        sd.lightsourceIntensities[0] == 0 && sd.lightsourceIntensities[1] == 0 &&
                        sd.lightsourceIntensities[2] == 0 && sd.lightsourceIntensities[3] == 0)
                    {
                        vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                        StageScan::triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                        vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
                    }
                }
            }
        }
        else
        {
            // ----------------------------------------------------------------
            // Grid-based scanning (XYZ raster)
            // ----------------------------------------------------------------
            long currentPosX = FocusMotor::getData()[Stepper::X]->currentPosition;
            long currentPosY = FocusMotor::getData()[Stepper::Y]->currentPosition;
            long currentPosZ = FocusMotor::getData()[Stepper::Z]->currentPosition;
            int32_t x0 = currentPosX;
            int32_t y0 = currentPosY;
            int32_t z0 = currentPosZ;
            int32_t key_speed = sd.speed;
            int32_t key_acceleration = sd.acceleration;
            isRunning = true;

            if (sd.xStart != 0) x0 = sd.xStart;
            if (sd.yStart != 0) y0 = sd.yStart;
            if (sd.zStart != 0) z0 = sd.zStart;

            pinMode(pinConfig.CAMERA_TRIGGER_PIN, OUTPUT);
            digitalWrite(pinConfig.CAMERA_TRIGGER_PIN, pinConfig.CAMERA_TRIGGER_INVERTED ? HIGH : LOW);
            log_i("Grid scan start X:%d Y:%d Z:%d (current X:%ld Y:%ld Z:%ld) nonstop=%d",
                  x0, y0, z0, currentPosX, currentPosY, currentPosZ, sd.nonstop);
            moveAbs(Stepper::X, x0, key_speed, key_acceleration);
            moveAbs(Stepper::Y, y0, key_speed, key_acceleration);
            moveAbs(Stepper::Z, z0, key_speed, key_acceleration);

            if (sd.nonstop)
            {
                // Continuous: sweep X line while timing-based trigger
                log_i("Continuous grid scan mode");
                auto *dataX = FocusMotor::getData()[Stepper::X];
                auto *dataY = FocusMotor::getData()[Stepper::Y];

                for (uint16_t iy = 0; iy < sd.nY && !sd.stopped; ++iy)
                {
                    bool rev = (iy & 1);
                    int32_t targetY = y0 + int32_t(iy) * sd.yStep;
                    int32_t prevY   = y0 + int32_t(iy > 0 ? iy - 1 : 0) * sd.yStep;

                    if (iy > 0)
                    {
                        uint32_t yMoveTime = calculateTimeToPosition(prevY, targetY, key_speed, key_acceleration);
                        dataY->absolutePosition = 1; dataY->targetPosition = targetY;
                        dataY->speed = key_speed;    dataY->isStop = 0;
                        dataY->stopped = false;      dataY->acceleration = key_acceleration;
                        dataY->isforever = false;
                        FocusMotor::startStepper(Stepper::Y, 1);
                        log_i("Y line %d: %u ms", iy, yMoveTime);
                        vTaskDelay(pdMS_TO_TICKS(yMoveTime + 50));
                    }

                    int32_t startXLine = rev ? (x0 + int32_t(sd.nX - 1) * sd.xStep) : x0;
                    int32_t endXLine   = rev ? x0 : (x0 + int32_t(sd.nX - 1) * sd.xStep);
                    dataX->absolutePosition = 1; dataX->targetPosition = endXLine;
                    dataX->speed = key_speed;    dataX->isStop = 0;
                    dataX->stopped = false;      dataX->acceleration = key_acceleration;
                    dataX->isforever = false;

                    uint32_t lineStartTime = millis();
                    FocusMotor::startStepper(Stepper::X, 1);
                    log_i("X line scan %d -> %d", startXLine, endXLine);

                    for (uint16_t ix = 0; ix < sd.nX && !sd.stopped; ++ix)
                    {
                        uint16_t j = rev ? (sd.nX - 1 - ix) : ix;
                        int32_t tgtX = x0 + int32_t(j) * sd.xStep;
                        uint32_t expectedTime = calculateTimeToPosition(startXLine, tgtX, key_speed, key_acceleration);
                        uint32_t elapsedTime  = millis() - lineStartTime;
                        if (expectedTime > elapsedTime)
                            vTaskDelay(pdMS_TO_TICKS(expectedTime - elapsedTime));
                        if (sd.stopped) break;
                        log_i("Trigger grid (%d,%d) X=%d", ix, iy, tgtX);
                        vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                        StageScan::triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                        vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
                    }
                    if (sd.stopped) break;
                }
            }
            else
            {
                // Stop-and-go grid scan with optional Z stack and illumination
                for (uint16_t iy = 0; iy < sd.nY && !sd.stopped; ++iy)
                {
                    bool rev = (iy & 1);
                    for (uint16_t ix = 0; ix < sd.nX && !sd.stopped; ++ix)
                    {
                        uint16_t j = zicZac ? (rev ? (sd.nX - 1 - ix) : ix) : ix;
                        int32_t tgtX = x0 + int32_t(j) * sd.xStep;
                        log_i("Moving X to %d", tgtX);
                        uint32_t timeX = calculateTimeToPosition(x0 + int32_t(j) * sd.xStep,
                                                                 x0 + int32_t(j + 1) * sd.xStep,
                                                                 sd.speed, sd.acceleration);
                        moveAbs(Stepper::X, tgtX, key_speed, key_acceleration, timeX + 200);

                        for (uint16_t iz = 0; iz < sd.nZ && !sd.stopped; ++iz)
                        {
                            int32_t tgtZ = z0 + int32_t(iz) * sd.zStep;
                            log_i("Moving Z to %d", tgtZ);
                            uint32_t timeZ = calculateTimeToPosition(z0 + int32_t(iz) * sd.zStep,
                                                                     z0 + int32_t(iz + 1) * sd.zStep,
                                                                     key_speed, key_acceleration);
                            moveAbs(Stepper::Z, tgtZ, key_speed, key_acceleration, timeZ + 200);

#ifdef LED_CONTROLLER
                            if (sd.ledarrayIntensity > 0)
                            {
                                sendLed(sd.ledarrayIntensity);
                                vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                                StageScan::triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                                vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
                                log_i("LED triggered at intensity %d", sd.ledarrayIntensity);
                                sendLed(0);
                            }
#endif
#ifdef LASER_CONTROLLER
                            for (int jj = 0; jj < 5; ++jj)
                            {
                                if (sd.lightsourceIntensities[jj] > 0)
                                {
                                    log_i("Laser %d at intensity %d", jj, sd.lightsourceIntensities[jj]);
                                    // LaserController::setLaserVal dispatches to CAN or native PWM internally
                                    LaserController::setLaserVal(jj, sd.lightsourceIntensities[jj]);
                                    vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                                    StageScan::triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                                    vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
                                    LaserController::setLaserVal(jj, 0);
                                }
                                else
                                {
                                    log_i("Laser %d intensity 0, skipping", jj);
                                }
                            }
#endif
                            if (sd.ledarrayIntensity == 0 &&
                                sd.lightsourceIntensities[0] == 0 && sd.lightsourceIntensities[1] == 0 &&
                                sd.lightsourceIntensities[2] == 0 && sd.lightsourceIntensities[3] == 0 &&
                                sd.lightsourceIntensities[4] == 0)
                            {
                                vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                                StageScan::triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                                vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
                                log_i("Camera triggered at X:%d Y:%d Z:%d", tgtX, y0 + int32_t(iy) * sd.yStep, tgtZ);
                            }
                        }
                    }

                    if (iy + 1 == sd.nY || sd.stopped)
                        break;

                    log_i("Moving Y to %d", y0 + int32_t(iy + 1) * sd.yStep);
                    uint32_t timeY = calculateTimeToPosition(y0 + int32_t(iy) * sd.yStep,
                                                             y0 + int32_t(iy + 1) * sd.yStep,
                                                             key_speed, key_acceleration);
                    moveAbs(Stepper::Y, y0 + int32_t(iy + 1) * sd.yStep, key_speed, key_acceleration, timeY + 200);
                }
            }
        }

        // Completion message
        cJSON *json = cJSON_CreateObject();
        cJsonTool::setJsonBool(json, "stagescan", 1);
        cJsonTool::setJsonInt(json, keyQueueID, sd.qid);
        cJsonTool::setJsonInt(json, "success", 1);
        Serial.println("++");
        char *ret = cJSON_PrintUnformatted(json);
        Serial.println(ret);
        cJSON_Delete(json);
        free(ret);
        Serial.println("--");

        isRunning = false;
#endif // CAN_RECEIVE_MOTOR

        if (isThread)
            vTaskDelete(NULL);
    }

    void stageScanThread(void *arg)
    // Scanning examples:
    // Grid-based:       {"task": "/motor_act", "stagescan": {"xStart": 0, "yStart": 0, "zStart":0, "xStep": 500, "yStep": 500, "zStep":0, "nX": 2, "nY": 2, "nZ":1, "tPre": 50, "tPost": 50}}
    // Coordinate-based: {"task": "/motor_act", "stagescan": {"coordinates": [{"x": 100, "y": 200}, {"x": 300, "y": 400}], "delayTimeStep": 10, "nFrames": 1}}
    //
    // Motor commands are dispatched via FocusMotor::startStepper which handles both
    // local drivers (FastAccelStepper/AccelStepper) and remote back-ends (CAN/I2C)
    // transparently.  Use the CAN_BUS_ENABLED build flag to differentiate LED dispatch.
    {
        stageScan(true);
    }

} // namespace StageScan
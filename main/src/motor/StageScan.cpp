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
            ets_delay_us(triggerTime); // Adjust delay for speed
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
#if defined CAN_BUS_ENABLED && !defined CAN_RECEIVE_MOTOR
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

    void stageScan(bool isThread)
    {
        FocusMotor::setEnable(true);

        // Check if we should use coordinate-based scanning

        if (stageScanningData.useCoordinates && stageScanningData.coordinates != nullptr)
        {
            // Coordinate-based scanning using only camera trigger pin
            pinMode(pinConfig.CAMERA_TRIGGER_PIN, OUTPUT);
            digitalWrite(pinConfig.CAMERA_TRIGGER_PIN, pinConfig.CAMERA_TRIGGER_INVERTED ? HIGH : LOW);

            log_i("Starting coordinate-based stage scanning with %d coordinates", stageScanningData.coordinateCount);
            for (int iFrame = 0; iFrame < stageScanningData.nFrames; iFrame++)
            {
                if (stageScanningData.stopped)
                {
                    break;
                }

                // Store current position to return to later
                int startX = FocusMotor::getData()[Stepper::X]->currentPosition;
                int startY = FocusMotor::getData()[Stepper::Y]->currentPosition;

                for (int i = 0; i < stageScanningData.coordinateCount; i++)
                {
                    if (stageScanningData.stopped)
                    {
                        break;
                    }

                    // Move to target coordinate using absolute positioning
                    int targetX = stageScanningData.coordinates[i].x;
                    int targetY = stageScanningData.coordinates[i].y;
                    log_i("Moving to coordinate %d: (%d, %d)", i, targetX, targetY);
#if defined CAN_BUS_ENABLED && !defined CAN_RECEIVE_MOTOR
                    // Use CAN moveAbs for absolute positioning
                    moveAbs(Stepper::X, targetX, stageScanningData.speed, stageScanningData.acceleration);
                    moveAbs(Stepper::Y, targetY, stageScanningData.speed, stageScanningData.acceleration);

                    // Timing for CAN-based systems
                    vTaskDelay(pdMS_TO_TICKS(stageScanningData.delayTimePreTrigger));
                    triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, stageScanningData.delayTimeTrigger);
                    vTaskDelay(pdMS_TO_TICKS(stageScanningData.delayTimePostTrigger));
#else
                    // Use direct motor control for non-CAN systems
                    FocusMotor::moveMotor(targetX, Stepper::X, false); // absolute positioning
                    while (FocusMotor::isRunning(Stepper::X))
                    {
                        delay(1);
                    }

                    FocusMotor::moveMotor(targetY, Stepper::Y, false); // absolute positioning
                    while (FocusMotor::isRunning(Stepper::Y))
                    {
                        delay(1);
                    }

                    // Trigger camera at this position
                    triggerOutput(pinConfig.CAMERA_TRIGGER_PIN);
                    ets_delay_us(stageScanningData.delayTimeStep * 1000);
#endif
                }

                // Return to start position if not CAN-based (CAN systems handle this automatically)
#if !defined(CAN_BUS_ENABLED) || defined(CAN_RECEIVE_MOTOR)
                if (startX != FocusMotor::getData()[Stepper::X]->currentPosition)
                {
                    FocusMotor::moveMotor(startX, Stepper::X, false);
                    while (FocusMotor::isRunning(Stepper::X))
                    {
                        delay(1);
                    }
                }

                if (startY != FocusMotor::getData()[Stepper::Y]->currentPosition)
                {
                    FocusMotor::moveMotor(startY, Stepper::Y, false);
                    while (FocusMotor::isRunning(Stepper::Y))
                    {
                        delay(1);
                    }
                }
#endif
            }
        }

        if (isThread)
            vTaskDelete(NULL);
    }

    void stageScanCAN(bool isThread)
    {
#if defined CAN_BUS_ENABLED && !defined CAN_RECEIVE_MOTOR
        auto &sd = StageScan::stageScanningData;

        // Check if we should use coordinate-based scanning
        if (sd.useCoordinates && sd.coordinates != nullptr)
        {
            // Coordinate-based CAN scanning
            isRunning = true;

            // Store current position
            long currentPosX = FocusMotor::getData()[Stepper::X]->currentPosition;
            long currentPosY = FocusMotor::getData()[Stepper::Y]->currentPosition;
            long currentPosZ = FocusMotor::getData()[Stepper::Z]->currentPosition;

            pinMode(pinConfig.CAMERA_TRIGGER_PIN, OUTPUT);
            digitalWrite(pinConfig.CAMERA_TRIGGER_PIN, pinConfig.CAMERA_TRIGGER_INVERTED ? HIGH : LOW);
            log_i("Starting coordinate-based CAN stage scanning with %d coordinates and %d frames, nonstop=%d", sd.coordinateCount, sd.nFrames, sd.nonstop);

            if (sd.nonstop)
            {
                // Continuous movement mode: start moving to final position and trigger at intermediate positions
                log_i("Using continuous movement mode");

                // Helper function to calculate time to reach a position with trapezoidal motion profile
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
                        // Triangular profile (never reaches max speed)
                        float peakSpeed = sqrt(distance * accel);
                        totalTime = (uint32_t)(2.0f * peakSpeed / accel * 1000.0f); // Convert to ms
                    }
                    else
                    {
                        // Trapezoidal profile
                        float constTime = (float)(distance - 2 * accelDist) / (float)speed;
                        totalTime = (uint32_t)((2.0f * accelTime + constTime) * 1000.0f); // Convert to ms
                    }

                    return totalTime;
                };

                // Start both motors moving to the final coordinate
                int32_t finalX = sd.coordinates[sd.coordinateCount - 1].x;
                int32_t finalY = sd.coordinates[sd.coordinateCount - 1].y;
                int32_t startX = currentPosX;
                int32_t startY = currentPosY;

                auto *dataX = FocusMotor::getData()[Stepper::X];
                auto *dataY = FocusMotor::getData()[Stepper::Y];
                dataX->absolutePosition = 1;
                dataX->targetPosition = finalX;
                dataX->speed = sd.speed;
                dataX->isStop = 0;
                dataX->stopped = false;
                dataX->acceleration = sd.acceleration;
                dataX->isforever = false;

                dataY->absolutePosition = 1;
                dataY->targetPosition = finalY;
                dataY->speed = sd.speed;
                dataY->isStop = 0;
                dataY->stopped = false;
                dataY->acceleration = sd.acceleration;
                dataY->isforever = false;

                uint32_t startTime = millis();
                FocusMotor::startStepper(Stepper::X, 1);
                FocusMotor::startStepper(Stepper::Y, 1);

                log_i("Starting continuous movement from (%d, %d) to (%d, %d)", startX, startY, finalX, finalY);

                // Trigger at each coordinate position based on calculated timing
                for (int i = 0; i < sd.coordinateCount && !sd.stopped; i++)
                {
                    int32_t targetX = sd.coordinates[i].x;
                    int32_t targetY = sd.coordinates[i].y;

                    // Calculate expected time to reach this position from start
                    uint32_t timeX = calculateTimeToPosition(startX, targetX, sd.speed, sd.acceleration);
                    uint32_t timeY = calculateTimeToPosition(startY, targetY, sd.speed, sd.acceleration);
                    // Use the maximum time (limiting axis)
                    uint32_t expectedTime = max(timeX, timeY);

                    // Wait until the expected time has elapsed
                    uint32_t currentTime = millis();
                    uint32_t elapsedTime = currentTime - startTime;

                    if (expectedTime > elapsedTime)
                    {
                        uint32_t waitTime = expectedTime - elapsedTime;
                        log_i("Position %d (%d, %d): waiting %u ms (expected %u ms, elapsed %u ms)",
                              i, targetX, targetY, waitTime, expectedTime, elapsedTime);
                        vTaskDelay(pdMS_TO_TICKS(waitTime));
                    }
                    else
                    {
                        log_i("Position %d (%d, %d): already reached (expected %u ms, elapsed %u ms)",
                              i, targetX, targetY, expectedTime, elapsedTime);
                    }

                    if (sd.stopped)
                        break;

                    // Trigger camera at this position
                    vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                    StageScan::triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                    vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
                }
            }
            else
            {
                // Original mode: stop at each position
                for (int i = 0; i < sd.coordinateCount && !sd.stopped; i++)
                {
                    // Move to coordinate position
                    int32_t targetX = sd.coordinates[i].x;
                    int32_t targetY = sd.coordinates[i].y;

                    log_i("Moving to coordinate X: %d, Y: %d", targetX, targetY);
                    moveAbs(Stepper::X, targetX, sd.speed, sd.acceleration);
                    moveAbs(Stepper::Y, targetY, sd.speed, sd.acceleration);

                    // LED/Laser control and camera triggering
#ifdef LED_CONTROLLER
                    if (sd.ledarrayIntensity > 0)
                    {
                        LedCommand cmd;
                        cmd.mode = LedMode::CIRCLE;
                        cmd.r = sd.ledarrayIntensity;
                        cmd.g = sd.ledarrayIntensity;
                        cmd.b = sd.ledarrayIntensity;
                        cmd.radius = 8;
                        cmd.ledIndex = 0;
                        cmd.region[0] = 0;
                        cmd.qid = 0;
                        can_controller::sendLedCommandToCANDriver(cmd, pinConfig.CAN_ID_LED_0);
                        vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                        StageScan::triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                        vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));

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
#if defined CAN_BUS_ENABLED && not defined(CAN_RECEIVE_LASER)
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
                    // If no LED/Laser, still trigger camera
                    if (sd.ledarrayIntensity == 0 && sd.lightsourceIntensities[0] == 0 &&
                        sd.lightsourceIntensities[1] == 0 && sd.lightsourceIntensities[2] == 0 && sd.lightsourceIntensities[3] == 0)
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
            // Original grid-based CAN scanning
            long currentPosX = FocusMotor::getData()[Stepper::X]->currentPosition;
            long currentPosY = FocusMotor::getData()[Stepper::Y]->currentPosition;
            long currentPosZ = FocusMotor::getData()[Stepper::Z]->currentPosition;
            int32_t x0 = currentPosX;
            int32_t y0 = currentPosY;
            int32_t z0 = currentPosZ;
            int32_t key_speed = sd.speed;
            int32_t key_acceleration = sd.acceleration;
            isRunning = true;

            // if we don't provide a start position, we take the current position and scan around it
            if (sd.xStart != 0)
                x0 = sd.xStart;
            if (sd.yStart != 0)
                y0 = sd.yStart;
            if (sd.zStart != 0)
                z0 = sd.zStart;
            // set the stepper to absolute position
            pinMode(pinConfig.CAMERA_TRIGGER_PIN, OUTPUT);
            digitalWrite(pinConfig.CAMERA_TRIGGER_PIN, pinConfig.CAMERA_TRIGGER_INVERTED ? HIGH : LOW); // set the trigger pin to high
            log_i("Moving to start position X: %d, Y: %d, Z: %d, currentposition X: %d, Y: %d, Z: %d, nonstop=%d", x0, y0, z0, currentPosX, currentPosY, currentPosZ, sd.nonstop);
            moveAbs(Stepper::X, x0, key_speed, key_acceleration);
            moveAbs(Stepper::Y, y0, key_speed, key_acceleration);
            moveAbs(Stepper::Z, z0, key_speed, key_acceleration);

            if (sd.nonstop)
            {
                // Continuous movement mode for grid-based scanning
                log_i("Using continuous movement mode for grid scanning");

                // Helper function to calculate time to reach a position with trapezoidal motion profile
                auto calculateTimeToPosition = [](int32_t startPos, int32_t targetPos, int32_t speed, int32_t accel) -> uint32_t
                {
                    int32_t distance = abs(targetPos - startPos);
                    if (distance == 0)
                        return 0;

                    // Trapezoidal motion profile calculation
                    float accelTime = (float)speed / (float)accel;
                    int32_t accelDist = (speed * speed) / (2 * accel);

                    uint32_t totalTime;
                    if (distance < 2 * accelDist)
                    {
                        // Triangular profile
                        float peakSpeed = sqrt(distance * accel);
                        totalTime = (uint32_t)(2.0f * peakSpeed / accel * 1000.0f);
                    }
                    else
                    {
                        // Trapezoidal profile
                        float constTime = (float)(distance - 2 * accelDist) / (float)speed;
                        totalTime = (uint32_t)((2.0f * accelTime + constTime) * 1000.0f);
                    }

                    return totalTime;
                };

                auto *dataX = FocusMotor::getData()[Stepper::X];
                auto *dataY = FocusMotor::getData()[Stepper::Y];

                for (uint16_t iy = 0; iy < sd.nY && !sd.stopped; ++iy)
                {
                    bool rev = (iy & 1);
                    int32_t targetY = y0 + int32_t(iy) * sd.yStep;
                    int32_t currentY = y0 + int32_t(iy > 0 ? iy - 1 : 0) * sd.yStep;

                    // Calculate Y movement time
                    uint32_t yMoveTime = 0;
                    if (iy > 0)
                    {
                        yMoveTime = calculateTimeToPosition(currentY, targetY, key_speed, key_acceleration);

                        dataY->absolutePosition = 1;
                        dataY->targetPosition = targetY;
                        dataY->speed = key_speed;
                        dataY->isStop = 0;
                        dataY->stopped = false;
                        dataY->acceleration = key_acceleration;
                        dataY->isforever = false;
                        FocusMotor::startStepper(Stepper::Y, 1);

                        log_i("Y movement to line %d: %u ms", iy, yMoveTime);
                        // Wait for Y movement to complete
                        vTaskDelay(pdMS_TO_TICKS(yMoveTime + 50)); // Add 50ms buffer
                    }

                    // Start continuous X movement for entire line
                    int32_t startXLine = rev ? (x0 + int32_t(sd.nX - 1) * sd.xStep) : x0;
                    int32_t endXLine = rev ? x0 : (x0 + int32_t(sd.nX - 1) * sd.xStep);

                    dataX->absolutePosition = 1;
                    dataX->targetPosition = endXLine;
                    dataX->speed = key_speed;
                    dataX->isStop = 0;
                    dataX->stopped = false;
                    dataX->acceleration = key_acceleration;
                    dataX->isforever = false;

                    uint32_t lineStartTime = millis();
                    FocusMotor::startStepper(Stepper::X, 1);

                    log_i("Starting X line scan from %d to %d", startXLine, endXLine);

                    // Trigger at each position along the line based on timing
                    for (uint16_t ix = 0; ix < sd.nX && !sd.stopped; ++ix)
                    {
                        uint16_t j = rev ? (sd.nX - 1 - ix) : ix;
                        int32_t tgtX = x0 + int32_t(j) * sd.xStep;

                        // Calculate expected time to reach this X position from start of line
                        uint32_t expectedTime = calculateTimeToPosition(startXLine, tgtX, key_speed, key_acceleration);
                        uint32_t currentTime = millis();
                        uint32_t elapsedTime = currentTime - lineStartTime;

                        if (expectedTime > elapsedTime)
                        {
                            uint32_t waitTime = expectedTime - elapsedTime;
                            vTaskDelay(pdMS_TO_TICKS(waitTime));
                        }

                        if (sd.stopped)
                            break;

                        // Trigger camera
                        log_i("Triggering at grid position (%d,%d): X=%d", ix, iy, tgtX);
                        vTaskDelay(pdMS_TO_TICKS(sd.delayTimePreTrigger));
                        StageScan::triggerOutput(pinConfig.CAMERA_TRIGGER_PIN, sd.delayTimeTrigger);
                        vTaskDelay(pdMS_TO_TICKS(sd.delayTimePostTrigger));
                    }

                    if (sd.stopped)
                        break;
                }
            }
            else
            {
                // Original mode: stop at each position
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

                        // Iterate over Z positions if needed
                        for (uint16_t iz = 0; iz < sd.nZ && !sd.stopped; ++iz)
                        {
                            int32_t tgtZ = z0 + int32_t(iz) * sd.zStep;
                            log_i("Moving Z to %d", tgtZ);
                            moveAbs(Stepper::Z, tgtZ, key_speed, key_acceleration);

// Iterate over all illumination settings                            
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
                            for (int j = 0; j < 5; ++j)
                            {
                                if (sd.lightsourceIntensities[j] > 0)
                                {
// TODO: We need to generalize this laser interface to make the same function for both I2C, CAN and native GPIO
#if defined CAN_BUS_ENABLED && not defined(CAN_RECEIVE_LASER)
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
                    }

                    if (iy + 1 == sd.nY || sd.stopped)
                        break;

                    // move to the next line
                    log_i("Moving Y to %d", y0 + int32_t(iy + 1) * sd.yStep);
                    moveAbs(Stepper::Y, y0 + int32_t(iy + 1) * sd.yStep, key_speed, key_acceleration);
                }
            }
        }

        // Send completion message (for both coordinate and grid modes)
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

    void stageScanThread(void *arg)
    // Grid-based scanning examples:
    // {"task": "/motor_act", "stagescan": {"nStepsLine": 100, "dStepsLine": 1, "nTriggerLine": 1, "nStepsPixel": 100, "dStepsPixel": 1, "nTriggerPixel": 1, "delayTimeStep": 5, "stopped": 0, "nFrames": 3000}}
    // Coordinate-based scanning examples:
    // {"task": "/motor_act", "stagescan": {"coordinates": [{"x": 100, "y": 200}, {"x": 300, "y": 400}], "delayTimeStep": 10, "nFrames": 1}}
    //
    // CAN/I2C Integration:
    // Coordinate-based scanning uses absolute positioning which automatically routes motor commands
    // through the appropriate communication layer:
    // - CAN_BUS_ENABLED: Commands sent via CAN to distributed motor controllers using moveAbs()
    // - I2C_MASTER: Commands sent via I2C to slave controllers
    // - Direct GPIO: Falls back to direct stepper control when neither CAN nor I2C is configured
    {
#if defined(CAN_SEND_COMMANDS)
        stageScanCAN(true);
#else
        stageScan(true);
#endif
    }

} // namespace StageScan
#include "can_controller.h"
#include "CanOtaHandler.h"
#include "CanOtaStreaming.h"
#include <PinConfig.h>
#include "Wire.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <WiFi.h>
#include <WiFiAP.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <esp_task_wdt.h>

#include "esp_log.h"
#include "esp_debug_helpers.h"
#include <algorithm>

#include "JsonKeys.h"
#include <Preferences.h>
#include "../serial/SerialProcess.h"
#ifdef DIAL_CONTROLLER
#include "../dial/DialController.h"
#endif
#ifdef LED_CONTROLLER
using namespace LedController;
#endif
using namespace FocusMotor;

#ifdef TMC_CONTROLLER
#include "../tmc/TMCController.h"
#endif

#ifdef LINEAR_ENCODER_CONTROLLER
#include "../encoder/LinearEncoderController.h"
#endif

namespace can_controller
{
    // Device CAN ID - defined here, declared extern in header
    uint8_t device_can_id = 0;

    // debug state - defined once here (declared extern in header)
    bool debugState = false;
    
    // ISO-TP separation time (ms) - adaptive based on debug mode
    // Debug mode: 30ms (to handle log_i overhead)
    // Production mode: 2ms (fast transfer, ~5 min for 1MB)
    uint8_t separationTimeMin = 2;  // Default to fast mode

    CanIsoTp isoTpSender;
    MessageData txData, rxData;
    static QueueHandle_t sendQueue;
    QueueHandle_t recieveQueue;
    // Queue size for CAN messages - increased for OTA transfers
    // Each ISO-TP chunk can generate multiple queue entries
    int CAN_QUEUE_SIZE = 10;
    
    // OTA status tracking
    static bool isOtaActive = false;
    static bool otaServerStarted = false;
    static unsigned long otaStartTime = 0;
    static uint32_t otaTimeout = 0;
    static char currentOtaSsid[MAX_SSID_LENGTH] = {0};

    // Motor Set status ( motorSettingsSent[axis] = true if sent )
    static bool motorSettingsSent[MOTOR_AXIS_COUNT] = {false};

    // for A,X,Y,Z intialize the I2C addresses
    uint8_t CAN_MOTOR_IDs[] = {
        pinConfig.CAN_ID_MOT_A,
        pinConfig.CAN_ID_MOT_X,
        pinConfig.CAN_ID_MOT_Y,
        pinConfig.CAN_ID_MOT_Z,
        pinConfig.CAN_ID_MOT_B,
        pinConfig.CAN_ID_MOT_C,
        pinConfig.CAN_ID_MOT_D,
        pinConfig.CAN_ID_MOT_E,
        pinConfig.CAN_ID_MOT_F,
        pinConfig.CAN_ID_MOT_G};

    // for 0,1, 2, 3 intialize the CAN LAser addresses
    uint8_t CAN_LASER_IDs[] = {
        pinConfig.CAN_ID_LASER_0,
        pinConfig.CAN_ID_LASER_1,
        pinConfig.CAN_ID_LASER_2,
        pinConfig.CAN_ID_LASER_3,
        pinConfig.CAN_ID_LASER_4};

    // for galvo devices - currently only one
    uint8_t CAN_GALVO_IDs[] = {
        pinConfig.CAN_ID_GALVO_0};

    // create an array of available CAN IDs that will be scanned later
    const int MAX_CAN_DEVICES = 20;                    // Maximum number of expected devices
    uint8_t availableCANids[MAX_CAN_DEVICES] = {0};    // Array to store available (online) CAN IDs
    uint8_t nonAvailableCANids[MAX_CAN_DEVICES] = {0}; // Array to store non-working CAN IDs
    int currentCANidListEntry = 0;                     // Variable to keep track of number of devices found
    
    // Scan response tracking
    static ScanResponse scanResponses[MAX_CAN_DEVICES];
    static int scanResponseCount = 0;
    static bool scanInProgress = false;
    
    // Scan result pending flag (similar to laser controller pattern)
    static bool scanResultPending = false;
    static int scanPendingQid = 0;

    void parseLEDData(uint8_t *data, size_t size, uint8_t txID)
    {
#ifdef LED_CONTROLLER
        if (size == sizeof(LedCommand))
        {
            LedCommand ledCmd;
            memcpy(&ledCmd, data, sizeof(ledCmd));

            if (debugState)
                log_i("Received LEDCommand from CAN: qid=%u, mode=%u, color=(%u,%u,%u), radius=%u, region=%s, ledIndex=%u",
                      ledCmd.qid, ledCmd.mode, ledCmd.r, ledCmd.g, ledCmd.b,
                      ledCmd.radius, ledCmd.region, ledCmd.ledIndex);

            // TODO: handle or apply the LED command on this device, e.g. call your LED code here
            // For example, if this node actually controls the LED array, you'd convert ledCmd.mode
            // back to your internal LedMode, then call your local 'LedController::execLedCommand(...)'.
            LedController::execLedCommand(ledCmd);
        }
        else
        {
            log_e("Error: Received LED data with invalid size=%u from ID=%u", size, txID);
        }
#endif
    }

    void parseMotorAndHomeData(uint8_t *data, size_t size, uint8_t txID)
    {
#ifdef MOTOR_CONTROLLER

        // Parse as MotorData
        if (debugState)
            log_i("Received MotorData from CAN %i, size: %i, txID: %i: %i", size, txID);
        if (size == sizeof(MotorData))
        {
            MotorData receivedMotorData;
            memcpy(&receivedMotorData, data, sizeof(MotorData));
            // assign the received data to the motor to MotorData *data[4];
            Stepper mStepper = static_cast<Stepper>(pinConfig.REMOTE_MOTOR_AXIS_ID); // default axis as motor driver is running its motor on (1)
            // FocusMotor::setData(pinConfig.REMOTE_MOTOR_AXIS_ID, &receivedMotorData);
            FocusMotor::getData()[mStepper]->qid = receivedMotorData.qid;
            FocusMotor::getData()[mStepper]->isEnable = receivedMotorData.isEnable;
            FocusMotor::getData()[mStepper]->targetPosition = receivedMotorData.targetPosition;
            FocusMotor::getData()[mStepper]->absolutePosition = receivedMotorData.absolutePosition;
            FocusMotor::getData()[mStepper]->speed = receivedMotorData.speed;
            FocusMotor::getData()[mStepper]->acceleration = receivedMotorData.acceleration;
            FocusMotor::getData()[mStepper]->isforever = receivedMotorData.isforever;
            FocusMotor::getData()[mStepper]->isStop = receivedMotorData.isStop;
            // Prevent the motor from getting stuck with zero acceleration
            if (FocusMotor::getData()[mStepper]->acceleration <= 0)
            {
                FocusMotor::getData()[mStepper]->acceleration = MAX_ACCELERATION_A;
            }

            // CRITICAL: Clear stale state flags that can block motor start.
            // These flags are NOT included in the field-by-field copy from the master.
            // After homing or hard-limit events on the slave, they may prevent motor start.
            if (!receivedMotorData.isStop)
            {
                FocusMotor::getData()[mStepper]->stopped = false;
                FocusMotor::getData()[mStepper]->isHoming = false;
                FocusMotor::getData()[mStepper]->hardLimitTriggered = false;
            }

            if (debugState)
                log_i(
                    "Received MotorData from CAN, isEnable: %d, targetPosition: %d, absolutePosition: %d, speed: %d, acceleration: %d, isforever: %d, isStop: %d",
                    static_cast<int>(receivedMotorData.isEnable),  // bool
                    receivedMotorData.targetPosition,              // long
                    receivedMotorData.absolutePosition,            // bool
                    receivedMotorData.speed,                       // long
                    receivedMotorData.acceleration,                // long
                    static_cast<int>(receivedMotorData.isforever), // bool
                    static_cast<int>(receivedMotorData.isStop));   // bool

            FocusMotor::toggleStepper(mStepper, FocusMotor::getData()[mStepper]->isStop, 0);
        }
        else if (size == sizeof(MotorDataReduced))
        {
            // use only the reduced data
            MotorDataReduced receivedMotorData;
            memcpy(&receivedMotorData, data, sizeof(MotorDataReduced));
            // assign the received data to the motor to MotorData *data[4];
            Stepper mStepper = static_cast<Stepper>(pinConfig.REMOTE_MOTOR_AXIS_ID);
            FocusMotor::getData()[mStepper]->targetPosition = receivedMotorData.targetPosition;
            FocusMotor::getData()[mStepper]->isforever = receivedMotorData.isforever;
            FocusMotor::getData()[mStepper]->absolutePosition = receivedMotorData.absolutePosition;
            FocusMotor::getData()[mStepper]->speed = receivedMotorData.speed;
            FocusMotor::getData()[mStepper]->isStop = receivedMotorData.isStop;

            // CRITICAL: MotorDataReduced does NOT include state flags that can block motor start.
            // After homing or hard-limit events, these flags may be stale and prevent the motor
            // from starting. Clear them when we receive a start command (isStop=false).
            if (!receivedMotorData.isStop)
            {
                FocusMotor::getData()[mStepper]->stopped = false;
                FocusMotor::getData()[mStepper]->isHoming = false;
                FocusMotor::getData()[mStepper]->hardLimitTriggered = false;
            }

            // Ensure acceleration is valid (not included in reduced data, could be 0 after boot)
            if (FocusMotor::getData()[mStepper]->acceleration <= 0)
            {
                FocusMotor::getData()[mStepper]->acceleration = MAX_ACCELERATION_A;
            }

            if (debugState)
                log_i("Received MotorDataReduced from CAN, targetPos: %i, isforever: %i, absPos: %i, speed: %i, isStop: %i, accel: %i",
                      receivedMotorData.targetPosition, receivedMotorData.isforever,
                      receivedMotorData.absolutePosition, receivedMotorData.speed,
                      receivedMotorData.isStop, FocusMotor::getData()[mStepper]->acceleration);

            FocusMotor::toggleStepper(mStepper, FocusMotor::getData()[mStepper]->isStop, 0);
        }
        else if (size == sizeof(MotorSettings))
        {
            // Receive motor configuration settings
            MotorSettings receivedMotorSettings;
            memcpy(&receivedMotorSettings, data, sizeof(MotorSettings));
            Stepper mStepper = static_cast<Stepper>(pinConfig.REMOTE_MOTOR_AXIS_ID);
            
            // Apply settings to the motor
            FocusMotor::getData()[mStepper]->directionPinInverted = receivedMotorSettings.directionPinInverted;
            FocusMotor::getData()[mStepper]->joystickDirectionInverted = receivedMotorSettings.joystickDirectionInverted;
            FocusMotor::getData()[mStepper]->isaccelerated = receivedMotorSettings.isaccelerated;
            FocusMotor::getData()[mStepper]->isEnable = receivedMotorSettings.isEnable;
            FocusMotor::getData()[mStepper]->maxspeed = receivedMotorSettings.maxspeed;
            FocusMotor::getData()[mStepper]->acceleration = receivedMotorSettings.acceleration;
            FocusMotor::getData()[mStepper]->isTriggered = receivedMotorSettings.isTriggered;
            FocusMotor::getData()[mStepper]->offsetTrigger = receivedMotorSettings.offsetTrigger;
            FocusMotor::getData()[mStepper]->triggerPeriod = receivedMotorSettings.triggerPeriod;
            FocusMotor::getData()[mStepper]->triggerPin = receivedMotorSettings.triggerPin;
            FocusMotor::getData()[mStepper]->dirPin = receivedMotorSettings.dirPin;
            FocusMotor::getData()[mStepper]->stpPin = receivedMotorSettings.stpPin;
            FocusMotor::getData()[mStepper]->maxPos = receivedMotorSettings.maxPos;
            FocusMotor::getData()[mStepper]->minPos = receivedMotorSettings.minPos;
            FocusMotor::getData()[mStepper]->softLimitEnabled = receivedMotorSettings.softLimitEnabled;
            FocusMotor::getData()[mStepper]->encoderBasedMotion = receivedMotorSettings.encoderBasedMotion;
            FocusMotor::getData()[mStepper]->hardLimitEnabled = receivedMotorSettings.hardLimitEnabled;
            FocusMotor::getData()[mStepper]->hardLimitPolarity = receivedMotorSettings.hardLimitPolarity;
            
            if (debugState)
                log_i("Received MotorSettings from CAN, maxspeed: %i, acceleration: %i, softLimitEnabled: %i, hardLimitEnabled: %i, hardLimitPolarity: %i", 
                      receivedMotorSettings.maxspeed, receivedMotorSettings.acceleration, receivedMotorSettings.softLimitEnabled,
                      receivedMotorSettings.hardLimitEnabled, receivedMotorSettings.hardLimitPolarity);
        }
        else if (size == sizeof(HomeData))
        {
            // Parse as HomeData
            HomeData receivedHomeData;
            memcpy(&receivedHomeData, data, sizeof(HomeData));
            // assign the received data to the motor to MotorData *data[4];
            Stepper mStepper = static_cast<Stepper>(pinConfig.REMOTE_MOTOR_AXIS_ID);
            int homeTimeout = receivedHomeData.homeTimeout;
            int homeSpeed = receivedHomeData.homeSpeed;
            int homeMaxspeed = receivedHomeData.homeMaxspeed;
            int homeDirection = receivedHomeData.homeDirection;
            int homeEndStopPolarity = receivedHomeData.homeEndStopPolarity;
            int homeEndOffset = receivedHomeData.homeEndOffset;
            bool usePreciseHoming = receivedHomeData.precise;
            
            if (debugState)
                log_i("Received HomeData from CAN, homeTimeout: %i, homeSpeed: %i, homeMaxspeed: %i, homeDirection: %i, homeEndStopPolarity: %i, homeEndOffset: %i, precise: %i", 
                      homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity, homeEndOffset, usePreciseHoming);
            
            if (usePreciseHoming) {
                #ifdef LINEAR_ENCODER_CONTROLLER
                // Use encoder-based stall detection for homing
                int homingSpeed = homeSpeed * homeDirection;
                log_i("Starting encoder-based homing via CAN for axis %d: speed=%d", mStepper, homingSpeed);
                LinearEncoderController::homeAxis(homingSpeed, mStepper);
                #else
                log_w("Encoder-based homing requested via CAN but LINEAR_ENCODER_CONTROLLER not available");
                HomeMotor::startHome(mStepper, homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity, homeEndOffset, 0);
                #endif
            } else {
                HomeMotor::startHome(mStepper, homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity, homeEndOffset, 0);
            }
        }
        else if (size == sizeof(StopHomeCommand))
        {
            // Parse as StopHomeCommand
            StopHomeCommand receivedStopCmd;
            memcpy(&receivedStopCmd, data, sizeof(StopHomeCommand));
            Stepper mStepper = static_cast<Stepper>(pinConfig.REMOTE_MOTOR_AXIS_ID);
            
            if (debugState)
                log_i("Received StopHomeCommand from CAN for axis: %i", mStepper);
            
            HomeMotor::stopHome(mStepper);
        }
        #ifdef TMC_CONTROLLER
        else if (size == sizeof(TMCData))
        {
            // parse incoming TMC Data and apply that to the TMC driver
            if (debugState)
                log_i("Received TMCData from CAN, size: %i, txID: %i", size, txID);
            TMCData receivedTMCData;
            memcpy(&receivedTMCData, data, sizeof(TMCData));
            if (debugState)
                log_i("Received TMCData from CAN, msteps: %i, rms_current: %i, stall_value: %i, sgthrs: %i, semin: %i, semax: %i, sedn: %i, tcoolthrs: %i, blank_time: %i, toff: %i", receivedTMCData.msteps, receivedTMCData.rms_current, receivedTMCData.stall_value, receivedTMCData.sgthrs, receivedTMCData.semin, receivedTMCData.semax, receivedTMCData.sedn, receivedTMCData.tcoolthrs, receivedTMCData.blank_time, receivedTMCData.toff);
            TMCController::applyParamsToDriver(receivedTMCData, true);
        }
        #endif
        else if (size == sizeof(MotorDataValueUpdate))
        {
            // Minimal single-value update for every part from the MotorData struct
            // This is used to update the motor data from the CAN bus - fast?
            MotorDataValueUpdate upd;
            memcpy(&upd, data, sizeof(upd));

            // Which motor? e.g. remote axis:
            Stepper mStepper = static_cast<Stepper>(pinConfig.REMOTE_MOTOR_AXIS_ID);

            // Write the new value directly:
            uint8_t *base = reinterpret_cast<uint8_t *>(FocusMotor::getData()[mStepper]);
            *reinterpret_cast<int32_t *>(base + upd.offset) = upd.value;

            if (debugState)
                log_i("Received MotorDataValueUpdate from CAN, offset: %i, value: %i", upd.offset, upd.value);
            // Only toggle the motor if offset corresponds to 'isStop' otherwise the motor will run multiple times with the same instructions
            if (upd.offset == offsetof(MotorData, isStop))
            {
                log_i("Received MotorDataValueUpdate from CAN, isStop: %i", upd.value);
                FocusMotor::toggleStepper(mStepper, FocusMotor::getData()[mStepper]->isStop, 0);
            }
        }
        else if (size == sizeof(SoftLimitData))
        {
            // Parse as SoftLimitData
            SoftLimitData receivedSoftLimit;
            memcpy(&receivedSoftLimit, data, sizeof(SoftLimitData));
            
            // The axis in SoftLimitData is the logical axis from master's perspective,
            // but we apply it to our local REMOTE_MOTOR_AXIS_ID
            Stepper mStepper = static_cast<Stepper>(pinConfig.REMOTE_MOTOR_AXIS_ID);
            
            if (debugState)
                log_i("Received SoftLimitData from CAN for axis %i: min=%ld, max=%ld, enabled=%u", 
                      receivedSoftLimit.axis, (long)receivedSoftLimit.minPos, 
                      (long)receivedSoftLimit.maxPos, receivedSoftLimit.enabled);
            
            // Apply soft limits to local motor
            FocusMotor::setSoftLimits(mStepper, receivedSoftLimit.minPos, 
                                     receivedSoftLimit.maxPos, receivedSoftLimit.enabled != 0);
            
            // Store in preferences for persistence
            Preferences preferences;
            preferences.begin("UC2", false);
            preferences.putInt(("min" + String(mStepper)).c_str(), receivedSoftLimit.minPos);
            preferences.putInt(("max" + String(mStepper)).c_str(), receivedSoftLimit.maxPos);
            preferences.putBool(("isen" + String(mStepper)).c_str(), receivedSoftLimit.enabled != 0);
            preferences.end();
        }
        else
        {
            if (debugState)
                log_e("Error: Incorrect data size received in CAN from address %u. Data size is %u", txID, size);
        }
#endif
    }

    int CANid2axis(uint8_t id)
    {
        // function that goes from CAN IDs to motor axis
        for (int i = 0; i < MOTOR_AXIS_COUNT; i++)
        {
            if (id == CAN_MOTOR_IDs[i])
            {
                return i;
            }
        }
        return -1;
    }

    void parseMotorAndHomeState(uint8_t *data, size_t size, uint8_t txID)
    {
#ifdef MOTOR_CONTROLLER
        // Parse as MotorState
        if (size == sizeof(MotorState))
        {
            // this is: The remote motor driver sends a MotorState to the central node which has to update the internal state
            // update position and is running-state
            MotorState receivedMotorState;
            memcpy(&receivedMotorState, data, sizeof(MotorState));

            if (debugState)
                log_i("Received MotorState from CAN, currentPosition: %i, isRunning: %i from axis: %i", receivedMotorState.currentPosition, receivedMotorState.isRunning, receivedMotorState.axis);
            int mStepper = receivedMotorState.axis;
            // check within bounds
            if (mStepper < 0 || mStepper >= MOTOR_AXIS_COUNT)
            {
                if (debugState)
                    log_e("Error: Received MotorState from CAN with invalid axis %i", mStepper);
                return;
            }
            FocusMotor::getData()[mStepper]->currentPosition = receivedMotorState.currentPosition;
            FocusMotor::getData()[mStepper]->stopped = !receivedMotorState.isRunning;
            FocusMotor::sendMotorPos(mStepper, 0);
            log_i("Motor %i: Received MotorState from CAN, currentPosition: %i, isRunning: %i", mStepper, receivedMotorState.currentPosition, receivedMotorState.isRunning);
        }
        else if (size == sizeof(HomeState))
        {
#ifdef HOME_MOTOR
            // Parse as HomeState
            HomeState receivedHomeState;
            memcpy(&receivedHomeState, data, sizeof(HomeState));
            int mStepper = receivedHomeState.axis;
            if (debugState)
                log_i("Received HomeState from CAN, isHoming: %i, homeInEndposReleaseMode: %i from axis: %i", receivedHomeState.isHoming, receivedHomeState.homeInEndposReleaseMode, mStepper);
            // check if mStepper is inside the range of the motors
            if (mStepper < 0 || mStepper > 3)
            {
                if (debugState)
                    log_e("Error: Received HomeState from CAN with invalid axis %i", mStepper);
                return;
            }
            HomeMotor::getHomeData()[mStepper]->homeIsActive = receivedHomeState.isHoming;
            HomeMotor::getHomeData()[mStepper]->homeInEndposReleaseMode = receivedHomeState.homeInEndposReleaseMode;
            FocusMotor::getData()[mStepper]->currentPosition = receivedHomeState.currentPosition;
            HomeMotor::sendHomeDone(mStepper);
#endif
        }
        else
        {
            if (debugState)
                log_e("Error: Incorrect data size received in CAN from address %u. Data size is %u", txID, size);
        }
#endif
    }

    void parseLaserData(uint8_t *data, size_t size, uint8_t txID)
    {
#ifdef LASER_CONTROLLER
        // Parse as LaserData
        LaserData laser;
        if (size >= sizeof(laser))
        {
            memcpy(&laser, data, sizeof(laser));
            // Do something with laser data
            if (1) // debugState)
                log_i("Laser intensity: %d, Laserid: %d", laser.LASERval, laser.LASERid);
            // assign PWM channesl to the laserid
            LaserController::setLaserVal(laser.LASERid, laser.LASERval);
        }
        else
        {
            if (1) // debugState)
                log_e("Error: Incorrect data size received in CAN from address %u. Data size is %u", txID, size);
        }
#endif
    }

    void parseGalvoData(uint8_t *data, size_t size, uint8_t txID)
    {
#ifdef GALVO_CONTROLLER
        // Parse as GalvoData
        GalvoData galvo;
        if (size >= sizeof(galvo))
        {
            memcpy(&galvo, data, sizeof(galvo));
            // Apply galvo settings
            if (debugState)
                log_i("Received galvo data: X_MIN=%d, X_MAX=%d, Y_MIN=%d, Y_MAX=%d, nx=%d, sample_period_us=%d, nFrames=%d, isRunning=%s",
                      galvo.X_MIN, galvo.X_MAX, galvo.Y_MIN, galvo.Y_MAX, galvo.STEP, galvo.tPixelDwelltime, galvo.nFrames,
                      galvo.isRunning ? "true" : "false");

            // Create JSON object in the format that processCommand() expects
            // {"task": "/galvo_act", "config": {...}, "qid": 123}
            cJSON *galvoJson = cJSON_CreateObject();
            cJSON_AddStringToObject(galvoJson, "task", "/galvo_act");
            cJSON_AddNumberToObject(galvoJson, "qid", galvo.qid);
            
            // Add config object with the new field names
            cJSON *config = cJSON_CreateObject();
            cJSON_AddNumberToObject(config, "x_min", galvo.X_MIN);
            cJSON_AddNumberToObject(config, "x_max", galvo.X_MAX);
            cJSON_AddNumberToObject(config, "y_min", galvo.Y_MIN);
            cJSON_AddNumberToObject(config, "y_max", galvo.Y_MAX);
            cJSON_AddNumberToObject(config, "nx", galvo.STEP);
            cJSON_AddNumberToObject(config, "ny", galvo.STEP);  // ny = nx (square scan)
            cJSON_AddNumberToObject(config, "sample_period_us", galvo.tPixelDwelltime);
            cJSON_AddNumberToObject(config, "frame_count", galvo.nFrames);
            cJSON_AddBoolToObject(config, "bidirectional", galvo.bidirectional);
            
            // Add new timing parameters
            cJSON_AddNumberToObject(config, "pre_samples", galvo.pre_samples);
            cJSON_AddNumberToObject(config, "fly_samples", galvo.fly_samples);
            cJSON_AddNumberToObject(config, "line_settle_samples", galvo.line_settle_samples);
            cJSON_AddNumberToObject(config, "trig_delay_us", galvo.trig_delay_us);
            cJSON_AddNumberToObject(config, "trig_width_us", galvo.trig_width_us);
            cJSON_AddNumberToObject(config, "enable_trigger", galvo.enable_trigger);
            cJSON_AddNumberToObject(config, "apply_x_lut", 0);  // Default: no LUT
            
            cJSON_AddItemToObject(galvoJson, "config", config);
            
            // Check if we should stop instead
            if (!galvo.isRunning)
            {
                // Send stop command instead
                cJSON_Delete(galvoJson);
                galvoJson = cJSON_CreateObject();
                cJSON_AddStringToObject(galvoJson, "task", "/galvo_act");
                cJSON_AddBoolToObject(galvoJson, "stop", true);
            }

            // Execute galvo action
            GalvoController::act(galvoJson);

            cJSON_Delete(galvoJson);
        }
        else
        {
            if (debugState)
                log_e("Error: Incorrect galvo data size received in CAN from address %u. Expected %u, got %u", txID, sizeof(galvo), size);
        }
#endif
    }

    void dispatchIsoTpData(pdu_t &pdu)
    {
        // Parse the received data
        uint8_t rxID = pdu.rxId;  // ID from which the message was sent
        uint8_t txID = pdu.txId;  // ID to which the message was sent
        uint8_t *data = pdu.data; // Data buffer
        size_t size = pdu.len;    // Data size
        if (debugState)
            log_i("CAN RXID: %u, TXID: %u, size: %u, own id: %u", rxID, txID, size, device_can_id);

        // CRITICAL: Route motor data by struct size BEFORE checking message type IDs.
        // Motor data (MotorData, MotorDataReduced, MotorSettings, HomeData, StopHomeCommand)
        // is sent as raw structs WITHOUT message type headers. The first bytes are data fields
        // (e.g., targetPosition in MotorDataReduced) that can coincidentally equal message type
        // IDs. Example: targetPosition=3200 -> LSB=0x80=SCAN_REQUEST, causing motor commands
        // to be misrouted as scan requests! Also affects OTA_CAN(0x62-69), OTA_STREAM(0x70-76).
        // By routing known motor struct sizes first, we prevent ALL such false matches.
        if (rxID == device_can_id &&
            (rxID == pinConfig.CAN_ID_MOT_A || rxID == pinConfig.CAN_ID_MOT_X ||
             rxID == pinConfig.CAN_ID_MOT_Y || rxID == pinConfig.CAN_ID_MOT_Z))
        {
            if (size == sizeof(MotorData) || size == sizeof(MotorDataReduced) ||
                size == sizeof(MotorSettings) || size == sizeof(HomeData) ||
                size == sizeof(StopHomeCommand))
            {
                parseMotorAndHomeData(data, size, rxID);
                return;
            }
            // For other sizes (e.g., 1-byte restart, typed messages like OTA/SCAN),
            // fall through to message type ID checks below.
        }

        // Check for message type identifier at the beginning
        if (size > 0)
        {
            CANMessageTypeID msgType = static_cast<CANMessageTypeID>(data[0]);
            
            // Handle OTA start command
            if (msgType == CANMessageTypeID::OTA_START && size >= (sizeof(CANMessageTypeID) + sizeof(OtaWifiCredentials)))
            {
                if (debugState)
                    log_i("Received OTA_START command from master");
                
                OtaWifiCredentials otaCreds;
                memcpy(&otaCreds, &data[1], sizeof(OtaWifiCredentials));
                handleOtaCommand(&otaCreds);
                return; // Function will restart device, so we return here
            }
            
            // Handle OTA acknowledgment (master side)
            #ifdef CAN_SEND_COMMANDS
            if (msgType == CANMessageTypeID::OTA_ACK && size >= (sizeof(CANMessageTypeID) + sizeof(OtaAck)))
            {
                OtaAck ack;
                memcpy(&ack, &data[1], sizeof(OtaAck));
                
                const char* statusMsg[] = {"Success", "WiFi connection failed", "OTA start failed"};
                log_i("Received OTA_ACK from CAN ID %u: %s (IP: %u.%u.%u.%u)", 
                      ack.canId, statusMsg[ack.status], 
                      ack.ipAddress[0], ack.ipAddress[1], ack.ipAddress[2], ack.ipAddress[3]);
                
                // Send JSON response via serial
                cJSON *root = cJSON_CreateObject();
                if (root != NULL)
                {
                    cJSON *otaObj = cJSON_CreateObject();
                    if (otaObj != NULL)
                    {
                        cJSON_AddItemToObject(root, "ota", otaObj);
                        cJSON_AddNumberToObject(otaObj, "canId", ack.canId);
                        cJSON_AddNumberToObject(otaObj, "status", ack.status);
                        cJSON_AddStringToObject(otaObj, "statusMsg", statusMsg[ack.status]);
                        
                        // Add IP address as string
                        char ipStr[16];
                        snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u", 
                                ack.ipAddress[0], ack.ipAddress[1], ack.ipAddress[2], ack.ipAddress[3]);
                        cJSON_AddStringToObject(otaObj, "ip", ipStr);
                        
                        // Add hostname
                        char hostname[32];
                        snprintf(hostname, sizeof(hostname), "UC2-CAN-%X.local", ack.canId);
                        cJSON_AddStringToObject(otaObj, "hostname", hostname);
                        
                        // Serialize and send
                        char *jsonString = cJSON_PrintUnformatted(root);
                        if (jsonString != NULL)
                        {
                            SerialProcess::safeSendJsonString(jsonString);
                            free(jsonString);
                        }
                        cJSON_Delete(root);
                    }
                    else
                    {
                        cJSON_Delete(root);
                    }
                }
                return;
            }
            #endif
            
            // Handle CAN-based OTA messages (firmware transfer over CAN)
            // OTA_CAN_* message types: 0x62-0x69
            if (msgType >= OTA_CAN_START && msgType <= OTA_CAN_STATUS)
            {
                // Route to CAN OTA handler
                // For slave devices: handle START, DATA, VERIFY, FINISH, ABORT, STATUS
                // For master: handle ACK, NAK responses from slaves
                can_ota::handleCanOtaMessage(static_cast<uint8_t>(msgType), data + 1, size - 1, txID);
                return;
            }
            
            // Handle CAN OTA STREAMING messages (high-speed mode)
            // STREAM_* message types: 0x70-0x76
            if (static_cast<uint8_t>(msgType) >= 0x70 && static_cast<uint8_t>(msgType) <= 0x76)
            {
                log_i("Received CAN OTA streaming message type %u from CAN ID %u, size: %u", static_cast<uint8_t>(msgType), txID, size);
                can_ota_stream::handleStreamMessage(static_cast<uint8_t>(msgType), data + 1, size - 1, txID);
                return;
            }
        }

        // Handle SCAN_RESPONSE (master side) - collect device info
        #ifdef CAN_SEND_COMMANDS
        if (size > 0)
        {
            CANMessageTypeID msgType = static_cast<CANMessageTypeID>(data[0]);
            if (msgType == CANMessageTypeID::SCAN_RESPONSE && 
                size >= (sizeof(CANMessageTypeID) + sizeof(ScanResponse)))
            {
                if (scanInProgress)
                {
                    ScanResponse scanResp;
                    memcpy(&scanResp, &data[1], sizeof(ScanResponse));
                    
                    if (debugState)
                        log_i("Received SCAN_RESPONSE from CAN ID %u (type: %u, status: %u)", 
                              scanResp.canId, scanResp.deviceType, scanResp.status);
                    
                    // Add to scan responses if not already present
                    bool alreadyExists = false;
                    for (int i = 0; i < scanResponseCount; i++)
                    {
                        if (scanResponses[i].canId == scanResp.canId)
                        {
                            alreadyExists = true;
                            break;
                        }
                    }
                    
                    if (!alreadyExists && scanResponseCount < MAX_CAN_DEVICES)
                    {
                        scanResponses[scanResponseCount++] = scanResp;
                        
                        // Also add to availableCANids list
                        bool inAvailableList = false;
                        for (int i = 0; i < MAX_CAN_DEVICES; i++)
                        {
                            if (availableCANids[i] == scanResp.canId)
                            {
                                inAvailableList = true;
                                break;
                            }
                        }
                        if (!inAvailableList)
                        {
                            for (int i = 0; i < MAX_CAN_DEVICES; i++)
                            {
                                if (availableCANids[i] == 0)
                                {
                                    availableCANids[i] = scanResp.canId;
                                    break;
                                }
                            }
                        }
                    }
                    return; // Don't process further
                }
            }
        }
        #endif

        // Handle SCAN_REQUEST (slave side) - respond with device info
        // CRITICAL: size == 1 check prevents false matches with motor data.
        // SCAN_REQUEST from master is always exactly 1 byte (0x80).
        // Without size check, MotorDataReduced with targetPosition=3200 (LSB=0x80)
        // would be misrouted as SCAN_REQUEST.
        if (size == 1 && rxID == device_can_id)
        {
            CANMessageTypeID msgType = static_cast<CANMessageTypeID>(data[0]);
            if (msgType == CANMessageTypeID::SCAN_REQUEST)
            {
                if (debugState)
                    log_i("Received SCAN_REQUEST from master, responding...");
                
                // Add random delay (0-50ms) to prevent all devices responding at once
                uint32_t randomDelay = esp_random() % 50;
                vTaskDelay(randomDelay / portTICK_PERIOD_MS);
                
                ScanResponse scanResp;
                scanResp.canId = device_can_id;
                
                // Determine device type based on CAN ID ranges
                if (device_can_id >= 10 && device_can_id <= 15)
                    scanResp.deviceType = 0; // Motor
                else if (device_can_id >= 20 && device_can_id <= 25)
                    scanResp.deviceType = 1; // Laser
                else if (device_can_id >= 30 && device_can_id <= 35)
                    scanResp.deviceType = 2; // LED or Galvo
                else if (device_can_id == pinConfig.CAN_ID_CENTRAL_NODE)
                    scanResp.deviceType = 4; // Master
                else
                    scanResp.deviceType = 0xFF; // Unknown
                
                // Set status (0=idle, 1=busy, 0xFF=error)
                #ifdef MOTOR_CONTROLLER
                bool anyMotorRunning = false;
                for (int i = 0; i < MOTOR_AXIS_COUNT; i++)
                {
                    if (isMotorRunning(i))
                    {
                        anyMotorRunning = true;
                        break;
                    }
                }
                scanResp.status = anyMotorRunning ? 1 : 0;
                #else
                scanResp.status = 0; // Default to idle
                #endif
                
                // Prepare message with SCAN_RESPONSE message type
                uint8_t buffer[sizeof(CANMessageTypeID) + sizeof(ScanResponse)];
                buffer[0] = static_cast<uint8_t>(CANMessageTypeID::SCAN_RESPONSE);
                memcpy(&buffer[1], &scanResp, sizeof(ScanResponse));
                
                // Send response back to master
                sendCanMessage(pinConfig.CAN_ID_CENTRAL_NODE, buffer, sizeof(buffer));
                return;
            }
        }

        // this is coming from the central node, so slaves should react
        /*
        FROM MASTER SENDER => perform an action remotly
        */
        // if the message was sent from the central node, we need to parse the data and perform an action (e.g. _act , _get) on the slave side
        // assuming the slave is a motor, we can parse the data and send it to the motor

        // Check if the message is a restart signal
        if (size == sizeof(uint8_t) && rxID == device_can_id) // Assuming an empty message indicates a restart
        {
            // parse the parameter
            // if pdu => 0 => restart
            uint8_t canCMD;
            memcpy(&canCMD, data, sizeof(uint8_t));
            log_i("Received generic command from CAN ID: %u, command: %u", rxID, canCMD);
            if (canCMD == 0)
            {
                // Restart the device
                if (debugState)
                    log_i("Received restart signal from CAN ID: %u", rxID);
                // Perform the restart
                esp_restart();
                return;
            }
            else if (canCMD == 1)
            {
                // Restart the device
                if (debugState)
                    log_i("Received restart signal from CAN ID: %u", rxID);
                // Perform the restart
                esp_restart();
                return;
            }
            else
            {
                if (debugState)
                    log_e("Error: Received invalid restart signal from CAN ID: %u", rxID);
            }
            return;
        }

        if (rxID == device_can_id && (rxID == pinConfig.CAN_ID_MOT_A || rxID == pinConfig.CAN_ID_MOT_X || rxID == pinConfig.CAN_ID_MOT_Y || rxID == pinConfig.CAN_ID_MOT_Z))
        {
            parseMotorAndHomeData(data, size, rxID);
        }
#ifdef GALVO_CONTROLLER
        // Support for galvo controllers
        else if (rxID == device_can_id && rxID == pinConfig.CAN_ID_GALVO_0)
        {
            parseGalvoData(data, size, rxID);
        }
#endif
#if defined(LASER_CONTROLLER) && !defined(LED_CONTROLLER)
        // Support for laser-only controllers
        else if (rxID == device_can_id &&
                 (rxID >= pinConfig.CAN_ID_LASER_0 && rxID <= pinConfig.CAN_ID_LASER_4))
        {
            parseLaserData(data, size, rxID);
        }
#endif
        else if (rxID == device_can_id && (size == sizeof(MotorState) or size == sizeof(HomeState))) // this is coming from the X motor
        {
            /*
            FROM THE DEVICES => update the state
            */
            parseMotorAndHomeState(data, size, rxID);
        }
#if defined(LED_CONTROLLER) && defined(LASER_CONTROLLER)
        // Support for illumination board that handles both LED and laser commands
        // The device can receive messages addressed to either primary or secondary CAN ID
        else if ((rxID == device_can_id) ||
                 (pinConfig.CAN_ID_SECONDARY != 0 && rxID == pinConfig.CAN_ID_SECONDARY))
        {
            if (size == sizeof(LedCommand))
            {
                parseLEDData(data, size, rxID);
            }
            else if (size == sizeof(LaserData))
            {
                parseLaserData(data, size, rxID);
            }
        }
#elif defined(LED_CONTROLLER)
        // Support for LED-only controllers
        else if (rxID == device_can_id && rxID == pinConfig.CAN_ID_LED_0 && size == sizeof(LedCommand))
        {
            parseLEDData(data, size, rxID);
        }
#endif
        else
        {
            if (debugState)
                log_e("Error: Received data from unknown CAN address %u", rxID);
        }
    }

    uint8_t getCANAddressPreferences()
    {
        // TODO: This is expensive, we should load that only when necessary( e.g. startup, when changed!!)
        Preferences preferences;
        preferences.begin("CAN", false);
        // if value not present yet, initialize:
        if (!preferences.isKey("address"))
        {
            preferences.putUInt("address", pinConfig.CAN_ID_CURRENT);
        }
        // TODO: double check if the master really has the correct address

        uint8_t address = preferences.getUInt("address", pinConfig.CAN_ID_CURRENT);
        preferences.end();
        return address;
    }

    uint8_t getCANAddress()
    {
        return getCANAddressPreferences();
    }

    void setCANAddress(uint8_t address)
    {
        // set the CAN address to the preferences (e.g. map axis)
        // TODO: Decide if we want to disable changing the can address if it'S a master
        if (address == pinConfig.CAN_ID_CENTRAL_NODE)
        {
            if (debugState)
                log_e("Error: Cannot set CAN address to the central node");
            address = pinConfig.CAN_ID_CENTRAL_NODE;
        }
        else
        {
            if (debugState)
                log_i("Setting CAN address to %u", address);
        }
        if (debugState)
            log_i("Setting CAN address to %u", address);
        Preferences preferences;
        preferences.begin("CAN", false);
        preferences.putUInt("address", address);
        preferences.end();
    }

    bool isIDInAvailableCANDevices(uint8_t idToCheck)
    {
        return true;  // TODO: this is reverse logic anyway?
        /* // TODO: not sure if this is actually needed 
        for (int i = 0; i < MAX_CAN_DEVICES; i++) // Iterate through the array
        {
            // check if ID is in nonAvailableCANids
            if (nonAvailableCANids[i] == idToCheck)
            {
                return true; // Address found in unavailable list
            }
        }
        return false; // Address not found in unavailable list
        */
    }
    
    bool isCANDeviceOnline(uint8_t canId)
    {
        for (int i = 0; i < MAX_CAN_DEVICES; i++)
        {
            if (availableCANids[i] == canId)
            {
                return true; // Device is online
            }
        }
        return false; // Device not found in available list
    }
    
    void addCANDeviceToAvailableList(uint8_t canId)
    {
        // Check if already in list
        if (isCANDeviceOnline(canId))
            return;
            
        // Add to first empty slot
        for (int i = 0; i < MAX_CAN_DEVICES; i++)
        {
            if (availableCANids[i] == 0)
            {
                availableCANids[i] = canId;
                if (debugState)
                    log_i("Added CAN ID %u to available devices list", canId);
                break;
            }
        }
    }
    
    void removeCANDeviceFromAvailableList(uint8_t canId)
    {
        for (int i = 0; i < MAX_CAN_DEVICES; i++)
        {
            if (availableCANids[i] == canId)
            {
                availableCANids[i] = 0;
                if (debugState)
                    log_i("Removed CAN ID %u from available devices list", canId);
                break;
            }
        }
    }

    void canSendTask(void *pvParameters)
    {
        pdu_t pdu;
        while (true)
        {
            if (xQueueReceive(sendQueue, &pdu, portMAX_DELAY) == pdTRUE)
            {
                // IMPORTANT: Save original data pointer before send() modifies it
                // The ISO-TP send() increments pdu.data as it sends consecutive frames
                uint8_t* originalDataPtr = pdu.data;
                
                int ret = isoTpSender.send(&pdu);
                if (ret != 0)
                {
                    if (debugState){
                        log_e("Error sending CAN message to %u with content: %u, ret: %d", pdu.txId, pdu.data, ret);
                    }
                }
                else
                {
                    if (debugState)
                        log_i("Sent CAN message to %u", pdu.txId);
                }
                // CRITICAL: Free the ORIGINAL data buffer pointer (not the modified one)
                if (originalDataPtr != nullptr)
                {
                    free(originalDataPtr);
                }
            }
            vTaskDelay(1); // give other tasks cputime
        }
    }

    // generic sender function
    int sendCanMessage(uint8_t receiverID, const uint8_t *data, size_t size)
    {
        /*
        This sends a message to the CAN bus via ISO-TP
        - If the receiverID is in the list of non-working motors, the message will not be sent
        - The Broadcast receiverID is 0 -> TODO: Not implemented yet
        - The Master receiverID is CAN_ID_CENTRAL_NODE 0x100 => 256
        - The Motor receiverIDs are CAN_ID_MOT_A, CAN_ID_MOT_X, CAN_ID_MOT_Y, CAN_ID_MOT_Z
        - The Laser receiverIDs are CAN_ID_LASER_0, CAN_ID_LASER_1, CAN_ID_LASER_2, CAN_ID_LASER_3, CAN_ID_LASER_4
        */
        lastSend = millis();
        // check if the receiverID is in the list of non-working motors

        if (!isIDInAvailableCANDevices(receiverID))
        {
            if (debugState) log_e("Error: ReceiverID %u is in the list of non-working motors", receiverID);
        }

        pdu_t txPdu;
        txPdu.data = (uint8_t *)malloc(size);
        if (txPdu.data == nullptr)
        {
            if (debugState) log_e("Error: Unable to allocate memory for txPdu.data");
            return -1;
        }
        memcpy(txPdu.data, data, size);
        // txPdu.data = (uint8_t *)data;
        // txPdu.data = data;
        txPdu.len = size;
        txPdu.txId = receiverID;    // the target ID
        txPdu.rxId = device_can_id; // the current ID
        // int ret = isoTpSender.send(&txPdu);

        // check if the queue is full - if yes, remove the oldest message
        if (uxQueueMessagesWaiting(sendQueue) == CAN_QUEUE_SIZE - 1)
        {
            pdu_t newPdu;
            xQueueReceive(sendQueue, &newPdu, portMAX_DELAY);
            if (newPdu.data != nullptr)
            {
                free(newPdu.data);
                newPdu.data = nullptr;
            }
        }

        // enqueue the message
        if (xQueueSend(sendQueue, &txPdu, 0) != pdPASS)
        {
            if (debugState)  log_w("Queue full! Dropping CAN message to %u", receiverID);
            free(txPdu.data);
            return -1;
        }
        else
        {
            if (debugState) log_i("Sending CAN message to %u", receiverID);
            // cast the structure to a byte array from the motor array
            // Print the data as MotorData
            /*
            MotorData *motorData = (MotorData *)txPdu.data;
            if (debugState) log_i("Sending MotorData to CAN, isEnable: %d, targetPosition: %d, absolutePosition: %d, speed: %d, acceleration: %d, isforever: %d, isStop: %d",
                  static_cast<int>(motorData->isEnable),
                  motorData->targetPosition,
                  motorData->absolutePosition,
                  motorData->speed,
                  motorData->acceleration,
                  static_cast<int>(motorData->isforever),
                  static_cast<int>(motorData->isStop));
                  */
        }

        return 0;
    }

    // Alias for sendCanMessage - used by CanOtaHandler for ISO-TP transmission
    int sendIsoTpData(uint8_t receiverID, const uint8_t *data, size_t size)
    {
        return sendCanMessage(receiverID, data, size);
    }

    // generic receiver function
    int receiveCanMessage(uint8_t rxID)
    {
        /*
        This receives a message from the CAN bus via ISO-TP
        - If the senderID is in the list of non-working motors, the message will not be received
        - The Broadcast senderID is 0 -> TODO: Not implemented yet
        - The Master senderID is CAN_ID_CENTRAL_NODE 0x100 => 256
        - The Motor senderIDs are CAN_ID_MOT_A, CAN_ID_MOT_X, CAN_ID_MOT_Y, CAN_ID_MOT_Z
        - The Laser senderIDs are CAN_ID_LASER_0, CAN_ID_LASER_1, CAN_ID_LASER_2, CAN_ID_LASER_3, CAN_ID_LASER_4

        We are checcking against the rxID

        senderID: the ID of the sender
        data: the data buffer to store the received data
        rxPdu: the pdu_t struct to store the received data
        txPdu: the pdu_t struct to store the data to be sent
        rxId: for whom the data is intended (i.e. the receiver ID)
        txId: who is sending the data (i.e. the transmitter ID)
        */
        int ret;
        // receive data from any node
        pdu_t rxPdu;
        // get filled on recieve
        // rxPdu.data = genericDataPtr;
        // rxPdu.len = sizeof(genericDataPtr);
        rxPdu.rxId = rxID; // broadcast => 0 - listen to all ids
        rxPdu.txId = 0;    // it really (!!!) doesn't matter when receiving frames, could use anything - but we use the current id
        ret = isoTpSender.receive(&rxPdu, 50);
        // check if remote ID is in the list of non-working motors - if yes and we received data, we should remove it from the list
        if (ret == 0 && isIDInAvailableCANDevices(rxID))
        {
            // remove the senderID from the list of non-working motors
            for (int i = 0; i < MAX_CAN_DEVICES; i++) // Iterate through the array
            {
                // check if ID is in nonAvailableCANids
                if (nonAvailableCANids[i] == rxID)
                {
                    if (debugState)
                        log_i("Removing %u from the list of non-working motors", rxID);
                    nonAvailableCANids[i] = 0; // Address found
                }
            }
        }
        if (ret == 0)
        {
            if (uxQueueMessagesWaiting(recieveQueue) == CAN_QUEUE_SIZE - 1)
            {
                if (debugState)
                    log_i("adding to recieveQueue");
                pdu_t newPdu;
                xQueueReceive(recieveQueue, &newPdu, portMAX_DELAY);
                if (newPdu.data != nullptr)
                {
                    free(newPdu.data);
                    newPdu.data = nullptr;
                }
            }
            if (xQueueSend(recieveQueue, &rxPdu, 0) != pdPASS)
            {
                if (debugState)
                    log_w("Queue full! Dropping CAN message to %u", rxID);
                return -1;
            }
            // this has txPdu => 0
            // this has rxPdu => frame.identifier (which is the ID from the current device)
        }
        return ret;
    }

    // Multi-address receive function
    int receiveCanMessage(uint8_t *rxIDs, uint8_t numIDs)
    {
        int ret;
        // receive data from any node
        pdu_t rxPdu;
        // No need to set rxPdu.rxId since we're passing multiple IDs to the receive function
        rxPdu.txId = 0; // it really (!!!) doesn't matter when receiving frames, could use anything
        ret = isoTpSender.receive(&rxPdu, rxIDs, numIDs, 50);

        // check if remote ID is in the list of non-working motors - if yes and we received data, we should remove it from the list
        if (ret == 0)
        {
            // The actual received ID is now in rxPdu.rxId (set by the multi-address receive function)
            if (isIDInAvailableCANDevices(rxPdu.rxId))
            {
                // remove the senderID from the list of non-working motors
                for (int i = 0; i < MAX_CAN_DEVICES; i++) // Iterate through the array
                {
                    // check if ID is in nonAvailableCANids
                    if (nonAvailableCANids[i] == rxPdu.rxId)
                    {
                        if (debugState)
                            log_i("Removing %u from the list of non-working motors", rxPdu.rxId);
                        nonAvailableCANids[i] = 0; // Address found
                    }
                }
            }

            if (uxQueueMessagesWaiting(recieveQueue) == CAN_QUEUE_SIZE - 1)
            {
                if (debugState)
                    log_i("adding to recieveQueue");
                pdu_t newPdu;
                xQueueReceive(recieveQueue, &newPdu, portMAX_DELAY);
                if (newPdu.data != nullptr)
                {
                    free(newPdu.data);
                    newPdu.data = nullptr;
                }
            }
            if (xQueueSend(recieveQueue, &rxPdu, 0) != pdPASS)
            {
                if (debugState)
                    log_i("Failed to send to recieveQueue");
                // free memory if not added to queue
                if (rxPdu.data != nullptr)
                {
                    free(rxPdu.data);
                    rxPdu.data = nullptr;
                }
            }
            // this has txPdu => 0
            // this has rxPdu => frame.identifier (which is the ID from the current device)
        }
        return ret;
    }

    void processCanMsgTask(void *p)
    {
        pdu_t rxPdu;
        while (true)
        {
            if (xQueueReceive(recieveQueue, &rxPdu, portMAX_DELAY) == pdTRUE)
            {
                // Process directly using rxPdu - no need to copy
                if (rxPdu.len > 0 && rxPdu.data != nullptr)
                {
                    // Dispatch directly with the original data
                    dispatchIsoTpData(rxPdu);
                    
                    // CRITICAL: Free the ISO-TP allocated buffer after processing
                    free(rxPdu.data);
                    rxPdu.data = nullptr;
                }
                else
                {
                    dispatchIsoTpData(rxPdu);
                }
            }
            vTaskDelay(1);
        }
    }

    void recieveTask(void *p)
    {
        // Check if there's a new message
        // pdu->txId = <other node's ID>
        // pdu->rxId = <my ID>
        // receive only messages that are sent to the current address
        //     int receiveCanMessage(uint8_t rxID, uint8_t *data)
        while (true)
        {
            // Prepare list of CAN IDs to listen to
            uint8_t rxIDs[2];
            uint8_t numIDs = 1;
            rxIDs[0] = device_can_id;

            // If secondary CAN address is configured and different from primary, add it to the list
            if (pinConfig.CAN_ID_SECONDARY != 0 && pinConfig.CAN_ID_SECONDARY != device_can_id)
            {
                rxIDs[1] = pinConfig.CAN_ID_SECONDARY;
                numIDs = 2;
            }

// Listen to all configured CAN addresses in one call
#ifdef CAN_MULTIADDRESS
            int mError = receiveCanMessage(rxIDs, numIDs);
#else
            int mError = receiveCanMessage(device_can_id);
#endif

            vTaskDelay(1);
        }
    }

    void setup()
    {
        // Create a mutex for the CAN bus
        debugState = pinConfig.DEBUG_CAN_ISO_TP;
            
        device_can_id = getCANAddress();
        log_i("Setting up CAN controller on port TX: %d, RX: %d using address: %u", pinConfig.CAN_TX, pinConfig.CAN_RX, device_can_id);
        sendQueue = xQueueCreate(CAN_QUEUE_SIZE, sizeof(pdu_t));
        recieveQueue = xQueueCreate(CAN_QUEUE_SIZE, sizeof(pdu_t));
        if(0)
        {
        xTaskCreate(canSendTask, "CAN_SendTask", 4096, NULL, 1, NULL);
        // Increased stack sizes for OTA handling which uses Update.write(), logging, etc. // TODO: PROBLEMATIC: WE SHOULD DO THAT ONCE WE START OTA!
        xTaskCreate(recieveTask, "CAN_RecieveTask", 6144, NULL, 1, NULL);
        xTaskCreate(processCanMsgTask, "CAN_RecieveProcessTask", 8192, NULL, 1, NULL);
        }
        else{
        
        xTaskCreate(canSendTask, "CAN_SendTask", 4096, NULL, 1, NULL);
        xTaskCreate(recieveTask, "CAN_RecieveTask", 4096, NULL, 1, NULL);
        xTaskCreate(processCanMsgTask, "CAN_RecieveProcessTask", 4096, NULL, 1, NULL);
        
       }
        if (sendQueue == nullptr)
        {
            if (debugState)
                log_e("Failed to create CAN mutex or queue!");
            ESP.restart();
        }

        // Indicate if pins are actually working by toggling them on/off 
        

        // Initialize CAN bus
        if (!isoTpSender.begin(500, pinConfig.CAN_TX, pinConfig.CAN_RX))
        {
            if (debugState)
                log_e("Failed to initialize CAN bus");
            return;
        }
        // Get and log CAN bus status info and print twai_get_status_info()
        twai_status_info_t status_info;
        twai_get_status_info(&status_info);
        log_i("CAN Controller Task Name:");
        log_i("  State: %d", status_info.state);

        if (debugState)
            log_i("CAN bus initialized with address %u on pins RX: %u, TX: %u", getCANAddress(), pinConfig.CAN_RX, pinConfig.CAN_TX);

        // Initialize streaming OTA subsystem
        can_ota_stream::init();

        // now we should announce that we are ready to receive data to the master (e.g. send the current address)
        sendCanMessage(pinConfig.CAN_ID_CENTRAL_NODE, &device_can_id, sizeof(device_can_id));
    }

    int act(cJSON *doc)
    {
        /*
         extract the address and set it to the preferences
        pinConfig.CAN_ID_CURRENT
        should be decimal
        {"task":"/can_act", "address": 10}
        {"task":"/can_get", "address": 1}
        {"task":"/can_act", "scan": true}
        List of CAN Addresses is in the PinConfig.h
            uint8_t CAN_ID_MOT_A = 10
            uint8_t CAN_ID_MOT_X = 11
            uint8_t CAN_ID_MOT_Y = 12
            uint8_t CAN_ID_MOT_Z = 13

            uint8_t CAN_ID_LASER_0 = 20
            uint8_t CAN_ID_LASER_1 = 21
            uint8_t CAN_ID_LASER_2 = 22
            uint8_t CAN_ID_LASER_3 = 23
            uint8_t CAN_ID_LASER_4 = 24
        */
        cJSON *address = cJSON_GetObjectItem(doc, "address");
        int qid = cJsonTool::getJsonInt(doc, "qid");

        if (address != NULL)
        {
            setCANAddress(address->valueint);
            device_can_id = address->valueint;
            if (debugState)
                log_i("Set CAN address to %u", address->valueint);
            return 1;
        }

        // reset the list of non available CAN IDs
        // {"task":"/can_act", "resetlist": true}
        cJSON *reset = cJSON_GetObjectItem(doc, "resetlist");
        if (reset != NULL)
        {
            memset(nonAvailableCANids, 0, sizeof(nonAvailableCANids));
            currentCANidListEntry = 0;
            return 1;
        }
        // activating/deactivating debugging messaging (i.e. DEBUG_CAN_ISO_TP flag)
        // {"task": "/can_act", "debug": true}
        cJSON *debug = cJSON_GetObjectItem(doc, "debug");
        if (debug != NULL)
        {
            debugState = cJSON_IsTrue(debug);
            if (debugState){
                // Debug mode: 30ms separation time to handle log_i overhead
                log_d("Enabling debug mode with extended separation time for ISO-TP frames");
                separationTimeMin = 30;
            }
            else{
                // Production mode: 2ms for fast transfer (~5 min for 1MB)
                log_d("Disabling debug mode, setting separation time to minimum for fast ISO-TP transfer");
                separationTimeMin = 2;
            }
            log_i("Set DEBUG_CAN_ISO_TP to %s, separationTimeMin=%dms", debugState ? "true" : "false", separationTimeMin);
            return 1;
        }

        // sending reboot signal to remote CAN device
        // {"task": "/can_act", "restart": 10, "qid":1} // CAN_ADDRESS is the remote CAN ID to which the client listens to
        cJSON *state = cJSON_GetObjectItem(doc, "restart");
        if (state != NULL)
        {
            int canID = state->valueint; // Access the valueint directly from the "restart" object
            sendCANRestartByID(canID);   // Send the restart signal to the specified CAN ID
            
            // Reset motor settings flag for the restarted device
            // The device will need to receive settings again after restart
            int axis = CANid2axis(canID);
            if (axis >= 0 && axis < MOTOR_AXIS_COUNT)
            {
                resetMotorSettingsFlag(axis);
                if (debugState)
                    log_i("Reset settings flag for axis %i after restart command", axis);
            }
        }

        // OTA update command to remote CAN device
        // {"task": "/can_act", "ota": {"canid": 11, "ssid": "MyWiFi", "password": "MyPassword", "timeout": 300000}}
        cJSON *ota = cJSON_GetObjectItem(doc, "ota");
        if (ota != NULL)
        {
            #ifdef CAN_SEND_COMMANDS
            // Extract parameters
            cJSON *canIdObj = cJSON_GetObjectItem(ota, "canid");
            cJSON *ssidObj = cJSON_GetObjectItem(ota, "ssid");
            cJSON *passwordObj = cJSON_GetObjectItem(ota, "password");
            cJSON *timeoutObj = cJSON_GetObjectItem(ota, "timeout");

            if (canIdObj == NULL || ssidObj == NULL || passwordObj == NULL)
            {
                log_e("OTA command missing required parameters (canid, ssid, password)");
                return -1;
            }

            uint8_t targetCanId = canIdObj->valueint;
            const char* ssid = cJSON_GetStringValue(ssidObj);
            const char* password = cJSON_GetStringValue(passwordObj);
            uint32_t timeout = (timeoutObj != NULL) ? timeoutObj->valueint : 300000; // Default 5 minutes

            log_i("Sending OTA command to CAN ID %u (SSID: %s, timeout: %lu ms)", 
                  targetCanId, ssid, timeout);

            int result = sendOtaStartCommandToSlave(targetCanId, ssid, password, timeout);
            if (result == 0)
            {
                log_i("OTA command sent successfully to CAN ID %u", targetCanId);
            }
            else
            {
                log_e("Failed to send OTA command to CAN ID %u", targetCanId);
            }
            return result;
            #else
            log_w("OTA command received but CAN_SEND_COMMANDS not defined");
            return -1;
            #endif
        }

        // CAN device scan command
        // {"task": "/can_act", "scan": true, "qid": 1}
        cJSON *scan = cJSON_GetObjectItem(doc, "scan");
        if (scan != NULL && cJSON_IsTrue(scan))
        {
            #ifdef CAN_SEND_COMMANDS
            log_i("Starting CAN network scan...");
            
            // Reset all motor settings flags before scan
            // New or restarted devices will need settings
            resetAllMotorSettingsFlags();
            
            // Set flag to perform scan in loop() and send results
            scanResultPending = true;
            scanPendingQid = qid;
            
            // Return qid to trigger acknowledgment immediately
            return qid;
            #else
            log_w("CAN scan command received but CAN_SEND_COMMANDS not defined");
            return -1;
            #endif
        }

        // test partial update on the motor data (sendMotorSpeedToCanDriver(uint8_t axis, int32_t newSpeed))
        cJSON *motor = cJSON_GetObjectItem(doc, "motor");
        if (motor != NULL)
        {
            // {"task":"/can_act", "motor": {"axis": 1, "speed": 1000, "position": 1000, "acceleration": 1000, "direction": 1, "isforever": 1}}
            // get the axis, speed, position, acceleration, direction and isforever from the json object
            int axis = cJSON_GetObjectItem(motor, "axis")->valueint;
            int32_t newSpeed = cJSON_GetObjectItem(motor, "speed")->valueint;
            int32_t newPos = cJSON_GetObjectItem(motor, "position")->valueint;
            int32_t newAcc = cJSON_GetObjectItem(motor, "acceleration")->valueint;
            int32_t newDir = cJSON_GetObjectItem(motor, "direction")->valueint;
            int32_t newIsforever = cJSON_GetObjectItem(motor, "isforever")->valueint;

            // check if the axis is in the range of the motors
            if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
            {
                if (debugState)
                    log_e("Error: Axis %i is out of range", axis);
                return -1;
            }
            // check if the motor is available
            if (isIDInAvailableCANDevices(CAN_MOTOR_IDs[axis]))
            {
                if (debugState)
                    log_e("Error: Motor %i is not available", CAN_MOTOR_IDs[axis]);
                return -1;
            }
            // send the data to the motor
            sendMotorSingleValue(axis, offsetof(MotorData, speed), newSpeed);
            sendMotorSingleValue(axis, offsetof(MotorData, isforever), newIsforever);
            /*
            sendMotorSingleValue(axis, offsetof(MotorData, targetPosition), newPos);
            sendMotorSingleValue(axis, offsetof(MotorData, acceleration), newAcc);
            sendMotorSingleValue(axis, offsetof(MotorData, isforever), newIsforever);
            */
        }

        else if (debugState)
            log_i("Motor json is null");
        return qid;
    }

    uint8_t axis2id(int axis)
    {
        if (axis >= 0 && axis < MOTOR_AXIS_COUNT)
        {
            return CAN_MOTOR_IDs[axis];
        }
        return 0;
    }

    int startStepper(MotorData *data, int axis, int reduced)
    {
#ifdef MOTOR_CONTROLLER
        if (getData()[axis] != nullptr)
        {
            // Send motor settings on first motor start if not already sent
            if (!motorSettingsSent[axis] && axis < MOTOR_AXIS_COUNT)
            {
                MotorSettings settings = extractMotorSettings(*getData()[axis]);
                sendMotorSettingsToCANDriver(settings, axis);
                // Note: motorSettingsSent[axis] is set to true inside sendMotorSettingsToCANDriver
            }
            
            // positionsPushedToDial = false;
            if (debugState)
                log_i("Starting motor on axis %i with speed %i, targetPosition %i, reduced: %i", axis, getData()[axis]->speed, getData()[axis]->targetPosition, reduced);
            getData()[axis]->isStop = false; // ensure isStop is false
            getData()[axis]->stopped = false;
            int err = sendMotorDataToCANDriver(*getData()[axis], axis, reduced);
            if (err != 0)
            {
                if (debugState)
                    log_e("Error starting motor on axis %i, we have to add this to the list of non-working motors", axis);
            }
            return err;
        }
        else
        {
            if (debugState)
                log_e("Error: MotorData is null for axis %i", axis);
            return -1;
        }
#else
        return -1;
#endif
    }

    void stopStepper(Stepper axis)
    {
// stop the motor
#ifdef MOTOR_CONTROLLER
        if (debugState)
            log_i("Stopping motor on axis %i", axis);
        getData()[axis]->isStop = true;
        getData()[axis]->stopped = true;
        int err = sendMotorDataToCANDriver(*getData()[axis], axis, true);
        if (err != 0)
        {
            if (debugState)
                log_e("Error starting motor on axis %i, we have to add this to the list of non-working motors", axis);
        }
#endif
    }

    void sendMotorStateToCANMaster(MotorData motorData)
    {
        // send motor state to master via I2C
        uint8_t slave_addr = device_can_id;
        uint8_t receiverID = pinConfig.CAN_ID_CENTRAL_NODE; // mixed?
        MotorState motorState;
        motorState.currentPosition = motorData.currentPosition;
        motorState.isRunning = !motorData.stopped;
        motorState.axis = CANid2axis(slave_addr);
        int err = sendCanMessage(receiverID, (uint8_t *)&motorState, sizeof(MotorState));
        if (err != 0)
        {
            if (debugState)
                log_e("Error sending motor state to CAN master at address %i", slave_addr);
        }
        else
        {
            // if (debugState) log_i("MotorState to master at address %i, currentPosition: %i, isRunning: %i, size %i", slave_addr, motorState.currentPosition, motorState.isRunning, dataSize);
        }
    }

    void sendMotorStateToMaster()
    {
// send the motor state to the master
#ifdef MOTOR_CONTROLLER
        sendMotorStateToCANMaster(*getData()[pinConfig.REMOTE_MOTOR_AXIS_ID]);
#endif
    }

    bool isMotorRunning(int axis)
    {
#ifdef MOTOR_CONTROLLER
        bool mIsRunning = !getData()[axis]->isStop;
        // log_i("Motor %i is running: %i", axis, !mIsRunning);
        return !mIsRunning;
#else
        return false;
#endif
    }

    int sendMotorDataToCANDriver(MotorData motorData, uint8_t axis, int reduced)
    {
        /*
        reduced:
            0 => Full MotorData
            1 => MotorDataReduced
            2 => Single Value Updates
        */
        // send motor data to slave via I2C
        uint8_t slave_addr = axis2id(axis);

        int err = 0;
        if (reduced == 0)
        {
            // Fully MotorData
            log_i("Sending MotorData to axis: %i, isStop: %i", axis, motorData.isStop);
            // Cast the structure to a byte array
            err = sendCanMessage(slave_addr, (uint8_t *)&motorData, sizeof(MotorData));
        }
        else if (reduced == 1)
        {
            // Reduced MotorData
            log_i("Reducing MotorData to axis: %i at address: %u, isStop: %i", axis, slave_addr, motorData.isStop);
            MotorDataReduced reducedData;
            reducedData.targetPosition = motorData.targetPosition;
            reducedData.isforever = motorData.isforever;
            reducedData.absolutePosition = motorData.absolutePosition;
            reducedData.speed = motorData.speed;
            reducedData.isStop = motorData.isStop;

            err = sendCanMessage(slave_addr, (uint8_t *)&reducedData, sizeof(MotorDataReduced));
        }
        else if (reduced == 2)
        {
            // Single Value Updates
            log_i("Sending SignleMotorDataValueUpdate to axis: %i", axis);
            // We treat only the speed, stop and targetPosition as single value updates
            if (motorData.speed != 0)
            {
                if (debugState)
                    log_i("Sending MotorDataValueUpdate to axis: %i with speed: %i", axis, motorData.speed);
                err = sendMotorSingleValue(axis, offsetof(MotorData, speed), motorData.speed) + err;
                err = sendMotorSingleValue(axis, offsetof(MotorData, isforever), motorData.isforever) + err;
            }
            if (motorData.targetPosition != 0 and !motorData.absolutePosition)
            {
                if (debugState)
                    log_i("Sending MotorDataValueUpdate to axis: %i with targetPosition: %i", axis, motorData.targetPosition);
                err = sendMotorSingleValue(axis, offsetof(MotorData, targetPosition), motorData.targetPosition);
                err = sendMotorSingleValue(axis, offsetof(MotorData, absolutePosition), motorData.absolutePosition) + err;
                err = sendMotorSingleValue(axis, offsetof(MotorData, isforever), false) + err;
            }
            // if (debugState) log_i("Sending MotorDataValueUpdate to axis: %i with isStop: %i", axis, motorData.isStop);
            err = sendMotorSingleValue(axis, offsetof(MotorData, isStop), motorData.isStop) + err;
        }
        if (err != 0)
        {
            if (debugState)
                log_e("Error sending motor data to CAN slave at address %i", slave_addr);
        }
        else
        {
            // if (debugState) log_i("MotorData to axis: %i, at address %i, isStop: %i, speed: %i, targetPosition:%i, reduced %i, stopped %i, isaccel: %i, accel: %i, isEnable: %i, isForever %i, size %i", axis, slave_addr, motorData.isStop, motorData.speed, motorData.targetPosition, reduced, motorData.stopped, motorData.isaccelerated, motorData.acceleration, motorData.isEnable, motorData.isforever, dataSize);
        }
        return err;
    }

    MotorSettings extractMotorSettings(const MotorData& motorData)
    {
        // Extract settings from MotorData struct
        MotorSettings settings;
        settings.directionPinInverted = motorData.directionPinInverted;
        settings.joystickDirectionInverted = motorData.joystickDirectionInverted;
        settings.isaccelerated = motorData.isaccelerated;
        settings.isEnable = motorData.isEnable;
        settings.maxspeed = motorData.maxspeed;
        settings.acceleration = motorData.acceleration;
        settings.isTriggered = motorData.isTriggered;
        settings.offsetTrigger = motorData.offsetTrigger;
        settings.triggerPeriod = motorData.triggerPeriod;
        settings.triggerPin = motorData.triggerPin;
        settings.dirPin = motorData.dirPin;
        settings.stpPin = motorData.stpPin;
        settings.maxPos = motorData.maxPos;
        settings.minPos = motorData.minPos;
        settings.softLimitEnabled = motorData.softLimitEnabled;
        settings.encoderBasedMotion = motorData.encoderBasedMotion;
        settings.hardLimitEnabled = motorData.hardLimitEnabled;
        settings.hardLimitPolarity = motorData.hardLimitPolarity;
        return settings;
    }

    int sendMotorSettingsToCANDriver(MotorSettings motorSettings, uint8_t axis)
    {
        // Send motor configuration settings to slave via CAN
        // This should be called once during initialization or when settings change
        uint8_t slave_addr = axis2id(axis);
        
        if (debugState)
            log_i("Sending MotorSettings to axis: %i, maxspeed: %i, acceleration: %i, softLimitEnabled: %i", 
                  axis, motorSettings.maxspeed, motorSettings.acceleration, motorSettings.softLimitEnabled);
        
        int err = sendCanMessage(slave_addr, (uint8_t *)&motorSettings, sizeof(MotorSettings));
        
        if (err != 0)
        {
            if (debugState)
                log_e("Error sending motor settings to CAN slave at address %i", slave_addr);
        }
        else
        {
            if (debugState)
                log_i("MotorSettings sent to CAN slave at address %i", slave_addr);
            // Mark settings as sent for this axis
            if (axis < MOTOR_AXIS_COUNT)
                motorSettingsSent[axis] = true;
        }
        
        return err;
    }
    
    void resetMotorSettingsFlag(uint8_t axis)
    {
        // Reset flag to force settings resend on next motor start
        if (axis < MOTOR_AXIS_COUNT)
        {
            motorSettingsSent[axis] = false;
            if (debugState)
                log_i("Reset motor settings flag for axis %i - will resend on next start", axis);
        }
    }
    
    void resetAllMotorSettingsFlags()
    {
        // Reset all flags - useful after system-wide events like CAN bus reset
        for (int i = 0; i < MOTOR_AXIS_COUNT; i++)
        {
            motorSettingsSent[i] = false;
        }
        if (debugState)
            log_i("Reset all motor settings flags - will resend on next start");
    }

    int sendCANRestartByID(uint8_t canID)
    {
        // send a restart signal to the remote CAN device
        if (debugState)
            log_i("Sending CAN restart signal to ID: %u", canID);
        uint8_t receiverID = canID;
        uint8_t data = 0; // empty data; 0 stands for restart
        return sendCanMessage(receiverID, reinterpret_cast<uint8_t *>(&data), sizeof(data));
    }

    int sendMotorSingleValue(uint8_t axis, uint16_t offset, int32_t newVal)
    {
        // Fill the update struct
        MotorDataValueUpdate update;
        update.offset = offset;
        update.value = newVal;

        if (debugState)
            log_i("Sending MotorDataValueUpdate to axis: %i with offset: %i, value: %i", axis, offset, newVal);
        // Call your existing CAN function
        uint8_t receiverID = axis2id(axis);
        return sendCanMessage(receiverID, reinterpret_cast<uint8_t *>(&update), sizeof(update));
    }

    int sendMotorSpeedToCanDriver(uint8_t axis, int32_t newSpeed)
    {
        // send motor speed to slave via I2C
        if (debugState)
            log_i("Sending MotorData to axis: %i with speed: %i", axis, newSpeed);
        return sendMotorSingleValue(axis, offsetof(MotorData, speed), newSpeed);
    }

    int sendEncoderBasedMotionToCanDriver(uint8_t axis, bool encoderBasedMotion)
    {
        // send encoder-based motion flag to slave via CAN
        if (debugState)
            log_i("Sending encoderBasedMotion to axis: %i with value: %i", axis, encoderBasedMotion);
        return sendMotorSingleValue(axis, offsetof(MotorData, encoderBasedMotion), encoderBasedMotion ? 1 : 0);
    }

    void sendHomeDataToCANDriver(HomeData homeData, uint8_t axis)
    {
        // send home data to slave via
        uint8_t slave_addr = axis2id(axis);
        if (debugState)
            log_i("Sending HomeData to axis: %i with parameters: speed %i, maxspeed %i, direction %i, endstop polarity %i, endoffset %i", axis, homeData.homeSpeed, homeData.homeMaxspeed, homeData.homeDirection, homeData.homeEndStopPolarity, homeData.homeEndOffset);
        // TODO: if we do homing on that axis the first time it mysteriously fails so we send it twice..
        int err = sendCanMessage(slave_addr, (uint8_t *)&homeData, sizeof(HomeData));
        if (err != 0)
        {
            if (debugState)
                log_e("Error sending home data to CAN slave at address %i", slave_addr);
        }
        else
        {
            if (debugState)
                log_i("Home data sent to CAN slave at address %i", slave_addr);
        }
    }

    void sendStopHomeToCANDriver(uint8_t axis)
    {
        // Send stop home command to slave via CAN
        uint8_t slave_addr = axis2id(axis);
        StopHomeCommand stopCmd;
        stopCmd.axis = axis;
        
        if (debugState)
            log_i("Sending StopHomeCommand to axis: %i", axis);
        
        int err = sendCanMessage(slave_addr, (uint8_t *)&stopCmd, sizeof(StopHomeCommand));
        if (err != 0)
        {
            if (debugState)
                log_e("Error sending stop home command to CAN slave at address %i", slave_addr);
        }
        else
        {
            if (debugState)
                log_i("Stop home command sent to CAN slave at address %i", slave_addr);
        }
    }

    int sendSoftLimitsToCANDriver(int32_t minPos, int32_t maxPos, bool enabled, uint8_t axis)
    {
        // Send soft limits configuration to slave via CAN
        // Use MotorSettings struct for this instead of individual values
        MotorSettings settings = extractMotorSettings(*FocusMotor::getData()[axis]);
        settings.minPos = minPos;
        settings.maxPos = maxPos;
        settings.softLimitEnabled = enabled;
        
        if (debugState)
            log_i("Updating soft limits for axis %i: min=%ld, max=%ld, enabled=%u", 
                  axis, (long)minPos, (long)maxPos, enabled);
        
        return sendMotorSettingsToCANDriver(settings, axis);
    }
    
    int sendSoftLimitsToCANDriver_LEGACY(int32_t minPos, int32_t maxPos, bool enabled, uint8_t axis)
    {
        // LEGACY: send soft limits configuration to slave via CAN using old method
        uint8_t slave_addr = axis2id(axis);
        
        SoftLimitData softLimitData;
        softLimitData.axis = axis;
        softLimitData.minPos = minPos;
        softLimitData.maxPos = maxPos;
        softLimitData.enabled = enabled ? 1 : 0;
        
        if (debugState)
            log_i("Sending SoftLimitData to axis: %i (CAN ID: %u), min: %ld, max: %ld, enabled: %u", 
                  axis, slave_addr, (long)minPos, (long)maxPos, softLimitData.enabled);
        
        int err = sendCanMessage(slave_addr, (uint8_t *)&softLimitData, sizeof(SoftLimitData));
        if (err != 0)
        {
            if (debugState)
                log_e("Error sending soft limits to CAN slave at address %i", slave_addr);
        }
        else
        {
            if (debugState)
                log_i("Soft limits sent to CAN slave at address %i", slave_addr);
        }
        return err;
    }

    void sendHomeStateToMaster(HomeState homeState)
    {
        // send home state to master via I2C
        uint8_t slave_addr = device_can_id;
        homeState.axis = CANid2axis(slave_addr);
        uint8_t receiverID = pinConfig.CAN_ID_CENTRAL_NODE;
        int err = sendCanMessage(receiverID, (uint8_t *)&homeState, sizeof(HomeState));
        if (err != 0)
        {
            if (debugState)
                log_e("Error sending home state to CAN master at address %i", receiverID);
        }
        else
        {
            /* if (debugState) log_i("HomeState to master at address %i, isHoming: %i, homeInEndposReleaseMode: %i, size %i, axis: %i",
                  receiverID,
                  homeState.isHoming,
                  homeState.homeInEndposReleaseMode,
                  dataSize,
                  homeState.axis);
                  */
        }
    }

#ifdef LED_CONTROLLER
    int sendLedCommandToCANDriver(LedCommand cmd, uint8_t targetID)
    {
        /*
            uint16_t qid;	// user-assigned ID
    LedMode mode;	// see enum above
    uint8_t r;		// color R
    uint8_t g;		// color G
    uint8_t b;		// color B
    uint8_t radius; // used for RINGS or CIRCLE
    char region[8]; // "left","right","top","bottom" (for HALVES)
    // For SINGLE pixel
    uint16_t ledIndex;
    // For an 'ARRAY' of pix
    */
        LedCommand canCmd;
        canCmd.qid = cmd.qid;
        canCmd.mode = static_cast<LedMode>(cmd.mode);
        canCmd.r = cmd.r;
        canCmd.g = cmd.g;
        canCmd.b = cmd.b;
        canCmd.radius = cmd.radius;
        if (debugState)
            log_i("Sending LED command to CAN driver, targetID: %u, mode: %u, r: %u, g: %u, b: %u, radius: %u", targetID, cmd.mode, cmd.r, cmd.g, cmd.b, cmd.radius);
        strncpy(canCmd.region, cmd.region, sizeof(canCmd.region) - 1);
        canCmd.region[sizeof(canCmd.region) - 1] = '\0';
        canCmd.ledIndex = cmd.ledIndex;

        // Then use your existing sendCanMessage(...)
        return sendCanMessage(targetID, reinterpret_cast<uint8_t *>(&canCmd), sizeof(canCmd));
    }
#endif

#ifdef TMC_CONTROLLER
    void sendTMCDataToCANDriver(TMCData tmcData, int axis)
    {
        // send TMC Data to remote Motor
        uint8_t slave_addr = axis2id(axis);
        if (debugState)
            log_i("Sending TMCData to axis: %i", axis);
        int err = sendCanMessage(slave_addr, (uint8_t *)&tmcData, sizeof(TMCData));
        if (err != 0)
        {
            if (debugState)
                log_e("Error sending TMC data to CAN slave at address %i", slave_addr);
        }
        else
        {
            // if (debugState) log_i("TMCData to axis: %i, at address %i, msteps: %i, rms_current: %i, stall_value: %i, sgthrs: %i, semin: %i, semax: %i, size %i", axis, slave_addr, tmcData.msteps, tmcData.rms_current, tmcData.stall_value, tmcData.sgthrs, tmcData.semin, tmcData.semax, dataSize);
        }
    }
#endif

    void sendLaserDataToCANDriver(LaserData laserData)
    {
        // send laser data to master via I2C
        // convert the laserID to the CAN address
        int laserID = laserData.LASERid;
        uint8_t receiverID = CAN_LASER_IDs[pinConfig.REMOTE_LASER_ID];
        if (0)
        {
            // TODO: this is only if we wanted to spread the lasers over different CAN addresses // TODO: check if this is necessary at one point
            receiverID = CAN_LASER_IDs[laserID];
        }

        uint8_t *dataPtr = (uint8_t *)&laserData;
        int dataSize = sizeof(LaserData);
        int err = sendCanMessage(receiverID, dataPtr, dataSize);
        if (err != 0)
        {
            if (debugState)
                log_e("Error sending laser data to CAN master at address %i", receiverID);
        }
        else
        {
            if (debugState)
                log_i("LaserData to master at address %i, laser intensity: %i, size %i", receiverID, laserData.LASERval, dataSize);
        }
    }

    cJSON *get(cJSON *ob)
    {
        // {"task":"/can_get", "address": 1}
        cJSON *doc = cJSON_CreateObject();

        // Statt ob direkt hinzuzufügen, duplizieren oder als Referenz hinzufügen
        cJSON_AddItemToObject(doc, "input", cJSON_Duplicate(ob, true));

        // add the current CAN address
        int addr = device_can_id;
        cJSON_AddNumberToObject(doc, "address", addr);
        // retreive address from prerfereces
        Preferences preferences;
        preferences.begin("CAN", false);
        int addr_pref = preferences.getUInt("address", device_can_id);
        preferences.end();
        cJSON_AddNumberToObject(doc, "addresspref", addr_pref);

        int addr_getcan = getCANAddress();
        cJSON_AddNumberToObject(doc, "addressgetcan", addr_getcan);

        // add the list of non-working CAN IDs
        // Convert uint8_t array to int array for cJSON_CreateIntArray
        int nonworkingIntArray[MAX_CAN_DEVICES];
        for (int i = 0; i < MAX_CAN_DEVICES; ++i)
        {
            nonworkingIntArray[i] = static_cast<int>(nonAvailableCANids[i]);
        }
        cJSON *nonworkingArray = cJSON_CreateIntArray(nonworkingIntArray, MAX_CAN_DEVICES);
        cJSON_AddItemToObject(doc, "nonworking", nonworkingArray);

        // CAN Bus Health Diagnostics - read TWAI driver status
        // This helps diagnose bus issues like devices bombarding the bus
        twai_status_info_t status;
        if (twai_get_status_info(&status) == ESP_OK) {
            cJSON *busHealth = cJSON_CreateObject();
            
            // State: 0=STOPPED, 1=RUNNING, 2=BUS_OFF, 3=RECOVERING
            const char* stateStr = "unknown";
            switch (status.state) {
                case TWAI_STATE_STOPPED: stateStr = "stopped"; break;
                case TWAI_STATE_RUNNING: stateStr = "running"; break;
                case TWAI_STATE_BUS_OFF: stateStr = "bus_off"; break;
                case TWAI_STATE_RECOVERING: stateStr = "recovering"; break;
            }
            cJSON_AddStringToObject(busHealth, "state", stateStr);
            
            // Error counters - high values indicate bus problems
            cJSON_AddNumberToObject(busHealth, "tx_error_counter", status.tx_error_counter);
            cJSON_AddNumberToObject(busHealth, "rx_error_counter", status.rx_error_counter);
            
            // Queue status - high values may indicate congestion
            cJSON_AddNumberToObject(busHealth, "msgs_to_tx", status.msgs_to_tx);
            cJSON_AddNumberToObject(busHealth, "msgs_to_rx", status.msgs_to_rx);
            
            // Missed messages - indicates buffer overflow / bus flooding
            cJSON_AddNumberToObject(busHealth, "tx_failed_count", status.tx_failed_count);
            cJSON_AddNumberToObject(busHealth, "rx_missed_count", status.rx_missed_count);
            cJSON_AddNumberToObject(busHealth, "rx_overrun_count", status.rx_overrun_count);
            cJSON_AddNumberToObject(busHealth, "arb_lost_count", status.arb_lost_count);
            cJSON_AddNumberToObject(busHealth, "bus_error_count", status.bus_error_count);
            
            cJSON_AddItemToObject(doc, "bus_health", busHealth);
            
            // Also print to serial for immediate diagnostics
            Serial.printf("++\n{\"can_bus_health\": {\"state\": \"%s\", \"tx_err\": %u, \"rx_err\": %u, \"rx_missed\": %u, \"bus_err\": %u}}\n--\n",
                          stateStr, status.tx_error_counter, status.rx_error_counter, 
                          status.rx_missed_count, status.bus_error_count);
        }

        // add the list of available (online) CAN IDs
        int availableIntArray[MAX_CAN_DEVICES];
        for (int i = 0; i < MAX_CAN_DEVICES; ++i)
        {
            availableIntArray[i] = static_cast<int>(availableCANids[i]);
        }
        cJSON *availableArray = cJSON_CreateIntArray(availableIntArray, MAX_CAN_DEVICES);
        cJSON_AddItemToObject(doc, "available", availableArray);

        // add the pins for RX/TX to the CAN bus
        cJSON_AddNumberToObject(doc, "rx", pinConfig.CAN_RX);
        cJSON_AddNumberToObject(doc, "tx", pinConfig.CAN_TX);

        return doc;
    }

#ifdef GALVO_CONTROLLER
    void sendGalvoDataToCANDriver(GalvoData galvoData)
    {
        // send galvo data to slave via CAN
        uint8_t receiverID = CAN_GALVO_IDs[0]; // Currently only one galvo device supported

        uint8_t *dataPtr = (uint8_t *)&galvoData;
        int dataSize = sizeof(GalvoData);
        int err = sendCanMessage(receiverID, dataPtr, dataSize);
        if (err != 0)
        {
            if (debugState)
                log_e("Error sending galvo data to CAN address %u: %d", receiverID, err);
        }
        else
        {
            if (debugState)
                log_i("Galvo data sent to CAN address %u successfully", receiverID);
        }
    }

    void sendGalvoStateToMaster(GalvoData galvoData)
    {
        // send galvo state back to master (from slave)
        uint8_t receiverID = pinConfig.CAN_ID_CENTRAL_NODE; // Send to master

        uint8_t *dataPtr = (uint8_t *)&galvoData;
        int dataSize = sizeof(GalvoData);
        int err = sendCanMessage(receiverID, dataPtr, dataSize);
        if (err != 0)
        {
            if (debugState)
                log_e("Error sending galvo state to master CAN address %u: %d", receiverID, err);
        }
        else
        {
            if (debugState)
                log_i("Galvo state sent to master CAN address %u successfully", receiverID);
        }
    }
#endif


    // ========================================================================
    // OTA Functions
    // ========================================================================

    /**
     * @brief Send OTA start command to a specific CAN slave
     * @param slaveID CAN ID of the target slave device
     * @param ssid WiFi SSID to connect to
     * @param password WiFi password
     * @param timeout_ms OTA server timeout in milliseconds (default 5 minutes)
     * @return 0 on success, -1 on error
     */
    int sendOtaStartCommandToSlave(uint8_t slaveID, const char* ssid, const char* password, uint32_t timeout_ms)
    {
        OtaWifiCredentials otaCreds;
        memset(&otaCreds, 0, sizeof(OtaWifiCredentials));
        
        // Copy SSID and password, ensuring null termination
        strncpy(otaCreds.ssid, ssid, MAX_SSID_LENGTH - 1);
        otaCreds.ssid[MAX_SSID_LENGTH - 1] = '\0';
        
        strncpy(otaCreds.password, password, MAX_PASSWORD_LENGTH - 1);
        otaCreds.password[MAX_PASSWORD_LENGTH - 1] = '\0';
        
        otaCreds.timeout_ms = timeout_ms;
        
        // Prepare message with OTA_START message type
        uint8_t buffer[sizeof(CANMessageTypeID) + sizeof(OtaWifiCredentials)];
        buffer[0] = static_cast<uint8_t>(CANMessageTypeID::OTA_START);
        memcpy(&buffer[1], &otaCreds, sizeof(OtaWifiCredentials));
        
        int err = sendCanMessage(slaveID, buffer, sizeof(buffer));
        if (err != 0)
        {
            if (debugState)
                log_e("Error sending OTA command to CAN slave %u", slaveID);
            return -1;
        }
        else
        {
            if (debugState)
                log_i("OTA start command sent to CAN slave %u (SSID: %s, timeout: %lu ms)", 
                      slaveID, ssid, timeout_ms);
            return 0;
        }
    }

    /**
     * @brief Handle incoming OTA command on slave device
     * @param otaCreds Pointer to OTA WiFi credentials structure
     */
    void handleOtaCommand(OtaWifiCredentials* otaCreds)
    {
        if (otaCreds == nullptr)
        {
            log_e("OTA credentials are null");
            sendOtaAck(2); // OTA start failed
            return;
        }

        if (otaCreds->timeout_ms < 300000)
        {
            log_w("OTA timeout too low (%lu ms), setting to minimum 30000 ms", otaCreds->timeout_ms);
            otaCreds->timeout_ms = 300000; // Minimum 30 seconds
        }

        log_i("Received OTA command - SSID: %s, timeout: %lu ms", 
              otaCreds->ssid, otaCreds->timeout_ms);

        // Check if OTA is already active with the same SSID
        if (isOtaActive && WiFi.status() == WL_CONNECTED && otaServerStarted)
        {
            // Check if it's the same SSID
            if (strcmp(currentOtaSsid, otaCreds->ssid) == 0)
            {
                log_i("OTA already active with same SSID, WiFi connected, and OTA server running - sending success ACK");
                sendOtaAck(0); // Success - already in OTA mode
                return; // Don't restart the OTA process
            }
            else
            {
                log_i("OTA active but different SSID requested - stopping OTA server and reconnecting");
                // Stop existing OTA server
                ArduinoOTA.end();
                otaServerStarted = false;
                // Continue to reconnect with new credentials
            }
        }

        // Mark OTA as active and save SSID
        isOtaActive = true;
        // Note: otaStartTime will be set after OTA server is successfully started
        strncpy(currentOtaSsid, otaCreds->ssid, MAX_SSID_LENGTH - 1);
        currentOtaSsid[MAX_SSID_LENGTH - 1] = '\0';

        // Close any ongoing WiFi connection
        WiFi.disconnect(true);
        delay(100);

        // Connect to WiFi with provided credentials
        log_i("Connecting to WiFi: %s", otaCreds->ssid);
        WiFi.mode(WIFI_STA);
        WiFi.begin(otaCreds->ssid, otaCreds->password);

        // Wait for connection (max 30 seconds)
        int connectTimeout = 30000;
        unsigned long startTime = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < connectTimeout)
        {
            delay(500);
            log_i("Connecting to WiFi...");
        }

        if (WiFi.status() != WL_CONNECTED)
        {
            log_e("Failed to connect to WiFi: %s", otaCreds->ssid);
            isOtaActive = false; // Reset OTA status on failure
            currentOtaSsid[0] = '\0'; // Clear SSID
            sendOtaAck(1); // WiFi connection failed
            return;
        }

        log_i("Connected to WiFi! IP: %s", WiFi.localIP().toString().c_str());
        sendOtaAck(0); // Success
        
        // Only setup OTA server if not already running

        // Generate hostname based on CAN address
        String hostname = "UC2-CAN-" + String(device_can_id, HEX);
        hostname.toUpperCase();

        // Set up mDNS responder
        if (MDNS.begin(hostname.c_str()))
        {
            log_i("mDNS responder started: %s.local", hostname.c_str());
        }
        else
        {
            log_e("Error starting mDNS");
        }

        // Configure ArduinoOTA
        ArduinoOTA.setHostname(hostname.c_str());
        ArduinoOTA.setMdnsEnabled(true);
        
        // Set MD5 hash to verify firmware integrity (optional but recommended)
        // Note: This will be checked against the MD5 sent in the OTA invitation
        
        ArduinoOTA.onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
            {
                type = "sketch";
                // Stop all running tasks to free up resources
                // and prevent conflicts during OTA update
            }
            else // U_SPIFFS
            {
                type = "filesystem";
                // Unmount SPIFFS if mounted
            }
            log_i("Start updating %s", type.c_str());
        });

        ArduinoOTA.onEnd([]() {
            log_i("\nOTA Update completed");
        });

        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            // Feed the task watchdog during long OTA writes to avoid WDT aborts
            esp_task_wdt_reset();

            // Compute percent safely
            int percent = 0;
            if (total != 0) percent = (int)((uint32_t)progress * 100 / (uint32_t)total);

            static unsigned long lastPrint = 0;
            if (millis() - lastPrint > 1000) {
                log_i("OTA Progress: %d%%", percent);
                lastPrint = millis();
            }
        });

        ArduinoOTA.onError([](ota_error_t error) {
            log_e("OTA Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR)
                log_e("Auth Failed");
            else if (error == OTA_BEGIN_ERROR)
                log_e("Begin Failed - Check if partition table has OTA partitions");
            else if (error == OTA_CONNECT_ERROR)
                log_e("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR)
                log_e("Receive Failed");
            else if (error == OTA_END_ERROR)
                log_e("End Failed - Firmware validation failed or incomplete");
        });

        ArduinoOTA.begin();
        
        // Store timeout for later checking (non-blocking)
        // IMPORTANT: Set these BEFORE marking OTA as started to avoid race condition
        otaTimeout = otaCreds->timeout_ms;
        if (otaTimeout == 0) {
            otaTimeout = 3000000; // Default 5 minutes
        }
        otaStartTime = millis();
        
        // Mark OTA server as started AFTER all variables are set
        otaServerStarted = true;
        log_i("OTA server started on %s.local (%s)", hostname.c_str(), WiFi.localIP().toString().c_str());
        
        // Don't block here - return so CAN communication can continue
        // OTA will be handled in a separate loop function
    }

    /**
     * @brief Send OTA acknowledgment back to master
     * @param status 0 = success, 1 = WiFi failed, 2 = OTA start failed
     */
    void sendOtaAck(uint8_t status)
    {
        OtaAck ack;
        ack.status = status;
        ack.canId = device_can_id;
        
        // Get IP address if connected to WiFi
        if (WiFi.status() == WL_CONNECTED)
        {
            IPAddress ip = WiFi.localIP();
            ack.ipAddress[0] = ip[0];
            ack.ipAddress[1] = ip[1];
            ack.ipAddress[2] = ip[2];
            ack.ipAddress[3] = ip[3];
        }
        else
        {
            // If not connected, set IP to 0.0.0.0
            ack.ipAddress[0] = 0;
            ack.ipAddress[1] = 0;
            ack.ipAddress[2] = 0;
            ack.ipAddress[3] = 0;
        }

        uint8_t buffer[sizeof(CANMessageTypeID) + sizeof(OtaAck)];
        buffer[0] = static_cast<uint8_t>(CANMessageTypeID::OTA_ACK);
        memcpy(&buffer[1], &ack, sizeof(OtaAck));

        int err = sendCanMessage(pinConfig.CAN_ID_CENTRAL_NODE, buffer, sizeof(buffer));
        if (err == 0)
        {
            if (debugState)
                log_i("OTA ACK sent to master (status: %u, IP: %u.%u.%u.%u)", 
                      status, ack.ipAddress[0], ack.ipAddress[1], ack.ipAddress[2], ack.ipAddress[3]);
        }
        else
        {
            if (debugState)
                log_e("Failed to send OTA ACK to master");
        }
    }

    /**
     * @brief Non-blocking OTA handler - call this in main loop
     * Handles ArduinoOTA and checks for timeout
     */
    void handleOtaLoop()
    {
        if (!isOtaActive || !otaServerStarted)
        {
            return; // OTA not active, nothing to do
        }

        // Handle OTA updates
        ArduinoOTA.handle();

        // Check for timeout
        if (millis() - otaStartTime > otaTimeout)
        {
            log_i("OTA timeout reached after %lu ms, restarting...", otaTimeout);
            
            // Clean up
            ArduinoOTA.end();
            WiFi.disconnect(true);
            
            // Reset OTA status
            isOtaActive = false;
            otaServerStarted = false;
            currentOtaSsid[0] = '\0';
            
            // Restart the device
            delay(1000);
            esp_restart();
        }
    }

    /**
     * @brief Scan for available CAN devices on the network
     * @return cJSON array containing discovered devices with their CAN IDs, types, and status
     * 
     * Uses broadcast approach: sends SCAN_REQUEST to multiple CAN IDs in ranges:
     * 10-15 (motors), 20-25 (lasers), 30-35 (LEDs/galvos)
     * All devices respond with random delays to prevent collisions
     * Total scan time: ~500ms for all ranges
     */
    cJSON* scanCanDevices()
    {
        const uint8_t scanRanges[][2] = {
            {10, 15},  // Motor controllers
            {20, 25},  // Laser controllers
            {30, 35}   // LED/Galvo controllers
        };
        const int numRanges = 3;
        const int scanDuration_ms = 500; // Total time to wait for all responses
        
        cJSON *devicesArray = cJSON_CreateArray();
        if (devicesArray == NULL)
        {
            log_e("Failed to create JSON array for scan results");
            return NULL;
        }
        
        log_i("Starting CAN device scan (broadcast mode)...");
        
        // Clear previous scan results // TODO: Not working anymore - fix it
        scanResponseCount = 0;
        memset(scanResponses, 0, sizeof(scanResponses));
        scanInProgress = true;
        
        // Prepare scan request message
        uint8_t scanRequest[1];
        scanRequest[0] = static_cast<uint8_t>(CANMessageTypeID::SCAN_REQUEST);
        
        // Send broadcast to all CAN IDs in ranges
        for (int rangeIdx = 0; rangeIdx < numRanges; rangeIdx++)
        {
            uint8_t startId = scanRanges[rangeIdx][0];
            uint8_t endId = scanRanges[rangeIdx][1];
            
            for (uint8_t canId = startId; canId <= endId; canId++)
            {
                // Skip if this is our own ID
                if (canId == device_can_id)
                    continue;
                
                // Send scan request to this CAN ID (broadcast)
                if (debugState)
                    log_i("Broadcasting SCAN_REQUEST to CAN ID: %u", canId);
                
                sendCanMessage(canId, scanRequest, sizeof(scanRequest));
                
                // Small delay between broadcasts to avoid overwhelming the bus
                vTaskDelay(50 / portTICK_PERIOD_MS); // TODO: adjust delay as needed to not overflow the queue
            }
        }
        
        // Wait for responses to arrive (they will be collected by dispatchIsoTpData)
        log_i("Waiting %d ms for scan responses...", scanDuration_ms);
        unsigned long startTime = millis();
        while ((millis() - startTime) < scanDuration_ms)
        {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            // Responses are being collected in the background by dispatchIsoTpData
        }
        
        scanInProgress = false;
        
        // Build JSON response from collected scan responses
        log_i("Scan completed. Processing %d responses...", scanResponseCount);
        for (int i = 0; i < scanResponseCount; i++)
        {
            ScanResponse *resp = &scanResponses[i];
            
            cJSON *deviceObj = cJSON_CreateObject();
            if (deviceObj != NULL)
            {
                cJSON_AddNumberToObject(deviceObj, "canId", resp->canId);
                cJSON_AddNumberToObject(deviceObj, "deviceType", resp->deviceType);
                cJSON_AddNumberToObject(deviceObj, "status", resp->status);
                
                // Add human-readable device type string
                const char* deviceTypeStr = "unknown";
                switch (resp->deviceType)
                {
                    case 0: deviceTypeStr = "motor"; break;
                    case 1: deviceTypeStr = "laser"; break;
                    case 2: deviceTypeStr = "led"; break;
                    case 3: deviceTypeStr = "galvo"; break;
                    case 4: deviceTypeStr = "master"; break;
                }
                cJSON_AddStringToObject(deviceObj, "deviceTypeStr", deviceTypeStr);
                
                // Add human-readable status string
                const char* statusStr = "unknown";
                if (resp->status == 0) statusStr = "idle";
                else if (resp->status == 1) statusStr = "busy";
                else if (resp->status == 0xFF) statusStr = "error";
                cJSON_AddStringToObject(deviceObj, "statusStr", statusStr);
                
                cJSON_AddItemToArray(devicesArray, deviceObj);
                
                log_i("Found device: CAN ID %u, Type: %s, Status: %s", 
                      resp->canId, deviceTypeStr, statusStr);
            }
        }
        
        log_i("CAN device scan completed. Found %d devices.", cJSON_GetArraySize(devicesArray));
        return devicesArray;
    }
    
    void sendScanResults(int qid)
    {
        // Perform the scan
        cJSON *devicesArray = scanCanDevices();
        
        if (devicesArray != NULL)
        {
            // Create response JSON
            cJSON *response = cJSON_CreateObject();
            if (response != NULL)
            {
                cJSON_AddItemToObject(response, "scan", devicesArray);
                cJSON_AddNumberToObject(response, "qid", qid);
                cJSON_AddNumberToObject(response, "count", cJSON_GetArraySize(devicesArray));
                
                // Serialize to string
                char *jsonString = cJSON_PrintUnformatted(response);
                if (jsonString != NULL)
                {
                    // Send scan results via serial
                    SerialProcess::safeSendJsonString(jsonString);
                    free(jsonString);
                }
                cJSON_Delete(response);
            }
            else
            {
                cJSON_Delete(devicesArray);
            }
        }
        else
        {
            log_e("Failed to perform CAN scan");
        }
    }
    
    /**
     * @brief Handle CAN OTA commands from serial interface
     * 
     * JSON format:
     * {"task":"/can_ota", "cmd":"start|data|verify|finish|abort|status", "slaveId":X, ...}
     * 
     * Commands:
     * - start: {"cmd":"start", "slaveId":X, "size":N, "chunks":N, "chunkSize":N, "md5":"..."}
     * - data:  {"cmd":"data", "slaveId":X, "chunk":N, "crc":N, "data":[...]}
     * - verify: {"cmd":"verify", "slaveId":X, "md5":"..."}
     * - finish: {"cmd":"finish", "slaveId":X}
     * - abort: {"cmd":"abort", "slaveId":X}
     * - status: {"cmd":"status", "slaveId":X}
     * 
     * @param doc JSON document containing the command
     * @return cJSON* Response object with status
     */
    cJSON* actCanOta(cJSON* doc)
    {
        cJSON* response = cJSON_CreateObject();
        if (response == NULL) {
            return NULL;
        }
        
        int result = can_ota::actFromJson(doc);
        
        if (result >= 0) {
            cJSON_AddBoolToObject(response, "success", true);
            cJSON_AddNumberToObject(response, "qid", result);
        } else {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddNumberToObject(response, "error", result);
            cJSON_AddStringToObject(response, "message", "CAN OTA command failed");
        }
        
        return response;
    }

    cJSON *actCanOtaStream(cJSON* doc)
    {
        cJSON* response = cJSON_CreateObject();
        if (response == NULL) {
            return NULL;
        }
        
        int result = can_ota_stream::actFromJsonStreaming(doc);
        
        if (result >= 0) {
            cJSON_AddBoolToObject(response, "success", true);
            cJSON_AddNumberToObject(response, "qid", result);
        } else {
            cJSON_AddBoolToObject(response, "success", false);
            cJSON_AddNumberToObject(response, "error", result);
            cJSON_AddStringToObject(response, "message", "CAN OTA Stream command failed");
        }
        
        return response;
    }
    
    void loop()
    {
        // Check for pending scan results and send them
        if (scanResultPending)
        {
            sendScanResults(scanPendingQid);
            scanResultPending = false; // Clear the flag after sending
            scanPendingQid = 0; // Reset qid
        }
        
        // Handle CAN OTA timeout checking
        can_ota::loop();
        
        // Handle streaming OTA timeout checking
        can_ota_stream::loop();
    }

} // namespace can_controller

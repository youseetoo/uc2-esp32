#include "can_controller.h"
#include <PinConfig.h>
#include "Wire.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_debug_helpers.h"
#include <algorithm>

#include "JsonKeys.h"
#include <Preferences.h>
#ifdef DIAL_CONTROLLER
#include "../dial/DialController.h"
#endif
#ifdef LED_CONTROLLER
using namespace LedController;
#endif
using namespace FocusMotor;

namespace can_controller
{

    CanIsoTp isoTpSender;
    MessageData txData, rxData;
    static QueueHandle_t sendQueue;
    QueueHandle_t recieveQueue;
    uint8_t device_can_id = 0;
    int CAN_QUEUE_SIZE = 5;

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
    uint8_t nonAvailableCANids[MAX_CAN_DEVICES] = {0}; // Array to store found I2C addresses
    int currentCANidListEntry = 0;                     // Variable to keep track of number of devices found

    void parseLEDData(uint8_t *data, size_t size, uint8_t txID)
    {
#ifdef LED_CONTROLLER
        if (size == sizeof(LedCommand))
        {
            LedCommand ledCmd;
            memcpy(&ledCmd, data, sizeof(ledCmd));

            if (pinConfig.DEBUG_CAN_ISO_TP)
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
        if (pinConfig.DEBUG_CAN_ISO_TP)
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
            // prevent the motor from getting stuck
            if (FocusMotor::getData()[mStepper]->acceleration <= 0)
            {
                FocusMotor::getData()[mStepper]->acceleration = MAX_ACCELERATION_A;
            }
            /*
            if (FocusMotor::getData()[mStepper]->speed == 0) // in case
            {
                FocusMotor::getData()[mStepper]->speed = 1000;
            }
            */
            if (pinConfig.DEBUG_CAN_ISO_TP)
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
            FocusMotor::toggleStepper(mStepper, FocusMotor::getData()[mStepper]->isStop, 0);
            // if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Received MotorData reduced from CAN, targetPosition: %i, isforever: %i, absolutePosition: %i, speed: %i, isStop: %i", receivedMotorData.targetPosition, receivedMotorData.isforever, receivedMotorData.absolutePosition, receivedMotorData.speed, receivedMotorData.isStop);
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
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Received HomeData from CAN, homeTimeout: %i, homeSpeed: %i, homeMaxspeed: %i, homeDirection: %i, homeEndStopPolarity: %i", homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity);
            HomeMotor::startHome(mStepper, homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity, 0, false);
        }
        else if (size == sizeof(TMCData))
        {
            // parse incoming TMC Data and apply that to the TMC driver
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Received TMCData from CAN, size: %i, txID: %i", size, txID);
            TMCData receivedTMCData;
            memcpy(&receivedTMCData, data, sizeof(TMCData));
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Received TMCData from CAN, msteps: %i, rms_current: %i, stall_value: %i, sgthrs: %i, semin: %i, semax: %i, sedn: %i, tcoolthrs: %i, blank_time: %i, toff: %i", receivedTMCData.msteps, receivedTMCData.rms_current, receivedTMCData.stall_value, receivedTMCData.sgthrs, receivedTMCData.semin, receivedTMCData.semax, receivedTMCData.sedn, receivedTMCData.tcoolthrs, receivedTMCData.blank_time, receivedTMCData.toff);
            TMCController::applyParamsToDriver(receivedTMCData, true);
        }
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

            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Received MotorDataValueUpdate from CAN, offset: %i, value: %i", upd.offset, upd.value);
            // Only toggle the motor if offset corresponds to 'isStop' otherwise the motor will run multiple times with the same instructions
            if (upd.offset == offsetof(MotorData, isStop))
            {
                log_i("Received MotorDataValueUpdate from CAN, isStop: %i", upd.value);
                FocusMotor::toggleStepper(mStepper, FocusMotor::getData()[mStepper]->isStop, 0);
            }
        }
        else
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
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

            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Received MotorState from CAN, currentPosition: %i, isRunning: %i from axis: %i", receivedMotorState.currentPosition, receivedMotorState.isRunning, receivedMotorState.axis);
            int mStepper = receivedMotorState.axis;
            // check within bounds
            if (mStepper < 0 || mStepper >= MOTOR_AXIS_COUNT)
            {
                if (pinConfig.DEBUG_CAN_ISO_TP)
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
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Received HomeState from CAN, isHoming: %i, homeInEndposReleaseMode: %i from axis: %i", receivedHomeState.isHoming, receivedHomeState.homeInEndposReleaseMode, mStepper);
            // check if mStepper is inside the range of the motors
            if (mStepper < 0 || mStepper > 3)
            {
                if (pinConfig.DEBUG_CAN_ISO_TP)
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
            if (pinConfig.DEBUG_CAN_ISO_TP)
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
            if (1) // pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Laser intensity: %d, Laserid: %d", laser.LASERval, laser.LASERid);
            // assign PWM channesl to the laserid
            LaserController::setLaserVal(laser.LASERid, laser.LASERval);
        }
        else
        {
            if (1) // pinConfig.DEBUG_CAN_ISO_TP)
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
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Received galvo data: X_MIN=%d, X_MAX=%d, Y_MIN=%d, Y_MAX=%d, STEP=%d, tPixelDwelltime=%d, nFrames=%d, fastMode=%s",
                      galvo.X_MIN, galvo.X_MAX, galvo.Y_MIN, galvo.Y_MAX, galvo.STEP, galvo.tPixelDwelltime, galvo.nFrames,
                      galvo.fastMode ? "true" : "false");

            // Create JSON object and call galvo controller
            cJSON *galvoJson = cJSON_CreateObject();
            cJSON_AddNumberToObject(galvoJson, "qid", galvo.qid);
            cJSON_AddNumberToObject(galvoJson, "X_MIN", galvo.X_MIN);
            cJSON_AddNumberToObject(galvoJson, "X_MAX", galvo.X_MAX);
            cJSON_AddNumberToObject(galvoJson, "Y_MIN", galvo.Y_MIN);
            cJSON_AddNumberToObject(galvoJson, "Y_MAX", galvo.Y_MAX);
            cJSON_AddNumberToObject(galvoJson, "STEP", galvo.STEP);
            cJSON_AddNumberToObject(galvoJson, "tPixelDwelltime", galvo.tPixelDwelltime);
            cJSON_AddNumberToObject(galvoJson, "nFrames", galvo.nFrames);
            cJSON_AddBoolToObject(galvoJson, "fastMode", galvo.fastMode);

            // Execute galvo action
            GalvoController::act(galvoJson);

            cJSON_Delete(galvoJson);
        }
        else
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
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
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("CAN RXID: %u, TXID: %u, size: %u, own id: %u", rxID, txID, size, device_can_id);

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
                if (pinConfig.DEBUG_CAN_ISO_TP)
                    log_i("Received restart signal from CAN ID: %u", rxID);
                // Perform the restart
                esp_restart();
                return;
            }
            else if (canCMD == 1)
            {
                // Restart the device
                if (pinConfig.DEBUG_CAN_ISO_TP)
                    log_i("Received restart signal from CAN ID: %u", rxID);
                // Perform the restart
                esp_restart();
                return;
            }
            else
            {
                if (pinConfig.DEBUG_CAN_ISO_TP)
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
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_e("Error: Received data from unknown CAN address %u", rxID);
        }
    }

    uint8_t getCANAddressPreferences()
    {
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
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_e("Error: Cannot set CAN address to the central node");
            address = pinConfig.CAN_ID_CENTRAL_NODE;
        }
        else
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Setting CAN address to %u", address);
        }
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Setting CAN address to %u", address);
        Preferences preferences;
        preferences.begin("CAN", false);
        preferences.putUInt("address", address);
        preferences.end();
    }

    bool isIDInAvailableCANDevices(uint8_t idToCheck)
    {
        for (int i = 0; i < MAX_CAN_DEVICES; i++) // Iterate through the array
        {
            // check if ID is in nonAvailableCANids
            if (nonAvailableCANids[i] == idToCheck)
            {
                return true; // Address found
            }
        }
        return false; // Address not found
    }

    void canSendTask(void *pvParameters)
    {
        pdu_t pdu;
        while (true)
        {
            if (xQueueReceive(sendQueue, &pdu, portMAX_DELAY) == pdTRUE)
            {
                if (isoTpSender.send(&pdu) != 0)
                {
                    if (pinConfig.DEBUG_CAN_ISO_TP)
                        log_e("Error sending CAN message to %u", pdu.txId);
                }
                else
                {
                    if (pinConfig.DEBUG_CAN_ISO_TP)
                        log_i("Sent CAN message to %u", pdu.txId);
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

        if (isIDInAvailableCANDevices(receiverID))
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_e("Error: ReceiverID %u is in the list of non-working motors", receiverID);
        }

        pdu_t txPdu;
        txPdu.data = (uint8_t *)malloc(size);
        if (txPdu.data == nullptr)
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_e("Error: Unable to allocate memory for txPdu.data");
            return -1;
        }
        memcpy(txPdu.data, data, size);
        // txPdu.data = (uint8_t *)data;
        // txPdu.data = data;
        txPdu.len = size;
        txPdu.txId = receiverID;    // the target ID
        txPdu.rxId = device_can_id; // the current ID
        // int ret = isoTpSender.send(&txPdu);

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

        if (xQueueSend(sendQueue, &txPdu, 0) != pdPASS)
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_w("Queue full! Dropping CAN message to %u", receiverID);
            free(txPdu.data);
            return -1;
        }
        else
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Sending CAN message to %u", receiverID);
            // cast the structure to a byte array from the motor array
            // Print the data as MotorData
            /*
            MotorData *motorData = (MotorData *)txPdu.data;
            if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Sending MotorData to CAN, isEnable: %d, targetPosition: %d, absolutePosition: %d, speed: %d, acceleration: %d, isforever: %d, isStop: %d",
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
                    if (pinConfig.DEBUG_CAN_ISO_TP)
                        log_i("Removing %u from the list of non-working motors", rxID);
                    nonAvailableCANids[i] = 0; // Address found
                }
            }
        }
        if (ret == 0)
        {
            if (uxQueueMessagesWaiting(recieveQueue) == CAN_QUEUE_SIZE - 1)
            {
                if (pinConfig.DEBUG_CAN_ISO_TP)
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
                if (pinConfig.DEBUG_CAN_ISO_TP)
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
                        if (pinConfig.DEBUG_CAN_ISO_TP)
                            log_i("Removing %u from the list of non-working motors", rxPdu.rxId);
                        nonAvailableCANids[i] = 0; // Address found
                    }
                }
            }

            if (uxQueueMessagesWaiting(recieveQueue) == CAN_QUEUE_SIZE - 1)
            {
                if (pinConfig.DEBUG_CAN_ISO_TP)
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
                if (pinConfig.DEBUG_CAN_ISO_TP)
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
                // Make a local copy of rxPdu
                pdu_t copy = rxPdu;
                if (copy.len > 0 && copy.data != nullptr)
                {
                    // Allocate space and copy the payload
                    // Serial.printf("About to malloc: %u bytes\n", (unsigned)copy.len);
                    copy.data = (uint8_t *)malloc(copy.len);
                    if (copy.data)
                    {
                        memcpy(copy.data, rxPdu.data, copy.len);
                        // Now dispatch using the copy
                        dispatchIsoTpData(copy);
                        free(copy.data);
                    }
                }
                else
                {
                    dispatchIsoTpData(copy);
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
        device_can_id = getCANAddress();
        sendQueue = xQueueCreate(CAN_QUEUE_SIZE, sizeof(pdu_t));
        recieveQueue = xQueueCreate(CAN_QUEUE_SIZE, sizeof(pdu_t));
        xTaskCreate(canSendTask, "CAN_SendTask", 4096, NULL, 1, NULL);
        xTaskCreate(recieveTask, "CAN_RecieveTask", 4096, NULL, 1, NULL);
        xTaskCreate(processCanMsgTask, "CAN_RecieveProcessTask", 4096, NULL, 1, NULL);

        if (sendQueue == nullptr)
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_e("Failed to create CAN mutex or queue!");
            ESP.restart();
        }
        // Initialize CAN bus
        if (!isoTpSender.begin(500, pinConfig.CAN_TX, pinConfig.CAN_RX))
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_e("Failed to initialize CAN bus");
            return;
        }

        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("CAN bus initialized with address %u on pins RX: %u, TX: %u", getCANAddress(), pinConfig.CAN_RX, pinConfig.CAN_TX);

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
            if (pinConfig.DEBUG_CAN_ISO_TP)
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

        // sending reboot signal to remote CAN device
        // {"task": "/can_act", "restart": 10, "qid":1} // CAN_ADDRESS is the remote CAN ID to which the client listens to
        cJSON *state = cJSON_GetObjectItem(doc, "restart");
        if (state != NULL)
        {
            int canID = state->valueint; // Access the valueint directly from the "restart" object
            sendCANRestartByID(canID);   // Send the restart signal to the specified CAN ID
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
                if (pinConfig.DEBUG_CAN_ISO_TP)
                    log_e("Error: Axis %i is out of range", axis);
                return -1;
            }
            // check if the motor is available
            if (isIDInAvailableCANDevices(CAN_MOTOR_IDs[axis]))
            {
                if (pinConfig.DEBUG_CAN_ISO_TP)
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

        else if (pinConfig.DEBUG_CAN_ISO_TP)
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
            // positionsPushedToDial = false;
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Starting motor on axis %i with speed %i, targetPosition %i, reduced: %i", axis, getData()[axis]->speed, getData()[axis]->targetPosition, reduced);
            getData()[axis]->isStop = false; // ensure isStop is false
            getData()[axis]->stopped = false;
            int err = sendMotorDataToCANDriver(*getData()[axis], axis, reduced);
            if (err != 0)
            {
                if (pinConfig.DEBUG_CAN_ISO_TP)
                    log_e("Error starting motor on axis %i, we have to add this to the list of non-working motors", axis);
            }
            return err;
        }
        else
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
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
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Stopping motor on axis %i", axis);
        getData()[axis]->isStop = true;
        getData()[axis]->stopped = true;
        int err = sendMotorDataToCANDriver(*getData()[axis], axis, true);
        if (err != 0)
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
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
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_e("Error sending motor state to CAN master at address %i", slave_addr);
        }
        else
        {
            // if (pinConfig.DEBUG_CAN_ISO_TP) log_i("MotorState to master at address %i, currentPosition: %i, isRunning: %i, size %i", slave_addr, motorState.currentPosition, motorState.isRunning, dataSize);
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
                if (pinConfig.DEBUG_CAN_ISO_TP)
                    log_i("Sending MotorDataValueUpdate to axis: %i with speed: %i", axis, motorData.speed);
                err = sendMotorSingleValue(axis, offsetof(MotorData, speed), motorData.speed) + err;
                err = sendMotorSingleValue(axis, offsetof(MotorData, isforever), motorData.isforever) + err;
            }
            if (motorData.targetPosition != 0 and !motorData.absolutePosition)
            {
                if (pinConfig.DEBUG_CAN_ISO_TP)
                    log_i("Sending MotorDataValueUpdate to axis: %i with targetPosition: %i", axis, motorData.targetPosition);
                err = sendMotorSingleValue(axis, offsetof(MotorData, targetPosition), motorData.targetPosition);
                err = sendMotorSingleValue(axis, offsetof(MotorData, absolutePosition), motorData.absolutePosition) + err;
                err = sendMotorSingleValue(axis, offsetof(MotorData, isforever), false) + err;
            }
            // if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Sending MotorDataValueUpdate to axis: %i with isStop: %i", axis, motorData.isStop);
            err = sendMotorSingleValue(axis, offsetof(MotorData, isStop), motorData.isStop) + err;
        }
        if (err != 0)
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_e("Error sending motor data to CAN slave at address %i", slave_addr);
        }
        else
        {
            // if (pinConfig.DEBUG_CAN_ISO_TP) log_i("MotorData to axis: %i, at address %i, isStop: %i, speed: %i, targetPosition:%i, reduced %i, stopped %i, isaccel: %i, accel: %i, isEnable: %i, isForever %i, size %i", axis, slave_addr, motorData.isStop, motorData.speed, motorData.targetPosition, reduced, motorData.stopped, motorData.isaccelerated, motorData.acceleration, motorData.isEnable, motorData.isforever, dataSize);
        }
        return err;
    }

    int sendCANRestartByID(uint8_t canID)
    {
        // send a restart signal to the remote CAN device
        if (pinConfig.DEBUG_CAN_ISO_TP)
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

        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Sending MotorDataValueUpdate to axis: %i with offset: %i, value: %i", axis, offset, newVal);
        // Call your existing CAN function
        uint8_t receiverID = axis2id(axis);
        return sendCanMessage(receiverID, reinterpret_cast<uint8_t *>(&update), sizeof(update));
    }

    int sendMotorSpeedToCanDriver(uint8_t axis, int32_t newSpeed)
    {
        // send motor speed to slave via I2C
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Sending MotorData to axis: %i with speed: %i", axis, newSpeed);
        return sendMotorSingleValue(axis, offsetof(MotorData, speed), newSpeed);
    }

    int sendEncoderBasedMotionToCanDriver(uint8_t axis, bool encoderBasedMotion)
    {
        // send encoder-based motion flag to slave via CAN
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Sending encoderBasedMotion to axis: %i with value: %i", axis, encoderBasedMotion);
        return sendMotorSingleValue(axis, offsetof(MotorData, encoderBasedMotion), encoderBasedMotion ? 1 : 0);
    }

    void sendHomeDataToCANDriver(HomeData homeData, uint8_t axis)
    {
        // send home data to slave via
        uint8_t slave_addr = axis2id(axis);
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Sending HomeData to axis: %i with parameters: speed %i, maxspeed %i, direction %i, endstop polarity %i", axis, homeData.homeSpeed, homeData.homeMaxspeed, homeData.homeDirection, homeData.homeEndStopPolarity);
        // TODO: if we do homing on that axis the first time it mysteriously fails so we send it twice..
        // check if axis was homed already in axisHomed - array
        int err = sendCanMessage(slave_addr, (uint8_t *)&homeData, sizeof(HomeData));
        if (axisHomed[axis] == false)
        {
            int err = sendCanMessage(slave_addr, (uint8_t *)&homeData, sizeof(HomeData));
            if (err != 0)
            {
                if (pinConfig.DEBUG_CAN_ISO_TP)
                    log_e("Error sending home data to CAN slave at address %i", slave_addr);
            }
            else
            {
                if (pinConfig.DEBUG_CAN_ISO_TP)
                    log_i("Home data sent to CAN slave at address %i", slave_addr);
            }
        }
        else
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Axis %i was already homed, not sending home data again", axis);
        }
        axisHomed[axis] = true;
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
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_e("Error sending home state to CAN master at address %i", receiverID);
        }
        else
        {
            /* if (pinConfig.DEBUG_CAN_ISO_TP) log_i("HomeState to master at address %i, isHoming: %i, homeInEndposReleaseMode: %i, size %i, axis: %i",
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
        if (pinConfig.DEBUG_CAN_ISO_TP)
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
        if (pinConfig.DEBUG_CAN_ISO_TP)
            log_i("Sending TMCData to axis: %i", axis);
        int err = sendCanMessage(slave_addr, (uint8_t *)&tmcData, sizeof(TMCData));
        if (err != 0)
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_e("Error sending TMC data to CAN slave at address %i", slave_addr);
        }
        else
        {
            // if (pinConfig.DEBUG_CAN_ISO_TP) log_i("TMCData to axis: %i, at address %i, msteps: %i, rms_current: %i, stall_value: %i, sgthrs: %i, semin: %i, semax: %i, size %i", axis, slave_addr, tmcData.msteps, tmcData.rms_current, tmcData.stall_value, tmcData.sgthrs, tmcData.semin, tmcData.semax, dataSize);
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
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_e("Error sending laser data to CAN master at address %i", receiverID);
        }
        else
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
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
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_e("Error sending galvo data to CAN address %u: %d", receiverID, err);
        }
        else
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
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
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_e("Error sending galvo state to master CAN address %u: %d", receiverID, err);
        }
        else
        {
            if (pinConfig.DEBUG_CAN_ISO_TP)
                log_i("Galvo state sent to master CAN address %u successfully", receiverID);
        }
    }
#endif

}
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
using namespace FocusMotor;

namespace can_controller
{

    CanIsoTp isoTpSender;
    MessageData txData, rxData;
    pdu_t txPdu, rxPdu;
    static SemaphoreHandle_t canMutex;

    // for A,X,Y,Z intialize the I2C addresses
    uint32_t CAN_MOTOR_IDs[] = {
        pinConfig.CAN_ID_MOT_A,
        pinConfig.CAN_ID_MOT_X,
        pinConfig.CAN_ID_MOT_Y,
        pinConfig.CAN_ID_MOT_Z};

    // for 0,1, 2, 3 intialize the CAN LAser addresses
    uint32_t CAN_LASER_IDs[] = {
        pinConfig.CAN_ID_LASER_0,
        pinConfig.CAN_ID_LASER_1,
        pinConfig.CAN_ID_LASER_2,
        pinConfig.CAN_ID_LASER_3};

    // create an array of available CAN IDs that will be scanned later
    const int MAX_CAN_DEVICES = 20;                     // Maximum number of expected devices
    uint32_t nonAvailableCANids[MAX_CAN_DEVICES] = {0}; // Array to store found I2C addresses
    int currentCANidListEntry = 0;                      // Variable to keep track of number of devices found

    // Global queue for received messages
    static QueueHandle_t canQueue;

    void parseMotorAndHomeData(uint8_t *data, size_t size, uint32_t txID, uint32_t rxID)
    {
#ifdef MOTOR_CONTROLLER

        // Parse as MotorData
        log_i("Received MotorData from CAN, size: %i, txID: %i, rxID: %i", size, txID, rxID);
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
            Serial.print("Received MotorData from CAN, isEnable: ");
            Serial.print(receivedMotorData.isEnable);
            Serial.print(", targetPosition: ");
            Serial.println(receivedMotorData.targetPosition);
            log_i("Received MotorData from can, isEnable: %i, targetPosition: %i, absolutePosition: %i, speed: %i, acceleration: %i, isforever: %i, isStop: %i", receivedMotorData.isEnable, receivedMotorData.targetPosition, receivedMotorData.absolutePosition, receivedMotorData.speed, receivedMotorData.acceleration, receivedMotorData.isforever, receivedMotorData.isStop);
            FocusMotor::toggleStepper(mStepper, FocusMotor::getData()[mStepper]->isStop, false);
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
            FocusMotor::toggleStepper(mStepper, FocusMotor::getData()[mStepper]->isStop, false);
            log_i("Received MotorData reduced from CAN, targetPosition: %i, isforever: %i, absolutePosition: %i, speed: %i, isStop: %i", receivedMotorData.targetPosition, receivedMotorData.isforever, receivedMotorData.absolutePosition, receivedMotorData.speed, receivedMotorData.isStop);
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
            log_i("Received HomeData from CAN, homeTimeout: %i, homeSpeed: %i, homeMaxspeed: %i, homeDirection: %i, homeEndStopPolarity: %i", homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity);
            HomeMotor::startHome(mStepper, homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity, 0, false);
        }
        else if (size == sizeof(TMCData))
        {
            // parse incoming TMC Data and apply that to the TMC driver
            log_i("Received TMCData from CAN, size: %i, txID: %i", size, txID);
            TMCData receivedTMCData;
            memcpy(&receivedTMCData, data, sizeof(TMCData));
            log_i("Received TMCData from CAN, msteps: %i, rms_current: %i, stall_value: %i, sgthrs: %i, semin: %i, semax: %i, sedn: %i, tcoolthrs: %i, blank_time: %i, toff: %i", receivedTMCData.msteps, receivedTMCData.rms_current, receivedTMCData.stall_value, receivedTMCData.sgthrs, receivedTMCData.semin, receivedTMCData.semax, receivedTMCData.sedn, receivedTMCData.tcoolthrs, receivedTMCData.blank_time, receivedTMCData.toff);
            TMCController::setTMCData(receivedTMCData);
        }
        else
        {
            log_e("Error: Incorrect data size received in CAN from address %u. Data size is %u", txID, size);
        }
#endif
    }

    void parseMotorAndHomeState(uint8_t *data, size_t size, uint32_t txID, Stepper mStepper)
    {
#ifdef MOTOR_CONTROLLER
        // Parse as MotorState
        log_i("Received MotorState from CAN, size: %i, txID: %i, axis: %i", size, txID, mStepper);
        if (size == sizeof(MotorState))
        {
            // this is: The remote motor driver sends a MotorState to the central node which has to update the internal state
            // update position and is running-state
            MotorState receivedMotorState;
            memcpy(&receivedMotorState, data, sizeof(MotorState));
            FocusMotor::getData()[mStepper]->currentPosition = receivedMotorState.currentPosition;
            FocusMotor::getData()[mStepper]->stopped = !receivedMotorState.isRunning;
            log_i("Received MotorState from CAN, currentPosition: %i, isRunning: %i from axis: %i", receivedMotorState.currentPosition, receivedMotorState.isRunning, mStepper);
            FocusMotor::sendMotorPos(mStepper, 0);
        }
        else if (size == sizeof(HomeState))
        {
            // Parse as HomeState
            HomeState receivedHomeState;
            memcpy(&receivedHomeState, data, sizeof(HomeState));
            HomeMotor::getHomeData()[mStepper]->homeIsActive = receivedHomeState.isHoming;
            HomeMotor::getHomeData()[mStepper]->homeInEndposReleaseMode = receivedHomeState.homeInEndposReleaseMode;
            FocusMotor::getData()[mStepper]->currentPosition = receivedHomeState.currentPosition;
            log_i("Received HomeState from CAN, isHoming: %i, homeInEndposReleaseMode: %i from axis: %i", receivedHomeState.isHoming, receivedHomeState.homeInEndposReleaseMode, mStepper);
            HomeMotor::sendHomeDone(mStepper);
        }
        else
        {
            log_e("Error: Incorrect data size received in CAN from address %u. Data size is %u", txID, size);
        }
#endif
    }

    void parseLaserData(uint8_t *data, size_t size, uint32_t txID)
    {
        #ifdef LASER_CONTROLLER
        // Parse as LaserData
        LaserData laser;
        if (size >= sizeof(laser))
        {
            memcpy(&laser, data, sizeof(laser));
            // Do something with laser data
            log_i("Laser intensity: %d", laser.LASERval);
            // assign PWM channesl to the laserid
            if (laser.LASERid == 0)
            {
                LaserController::setLaserVal(LaserController::PWM_CHANNEL_LASER_0, laser.LASERval);
            }
            else if (laser.LASERid == 1)
            {
                LaserController::setLaserVal(LaserController::PWM_CHANNEL_LASER_1, laser.LASERval);
            }
            else if (laser.LASERid == 2)
            {
                LaserController::setLaserVal(LaserController::PWM_CHANNEL_LASER_2, laser.LASERval);
            }
            else if (laser.LASERid == 3)
            {
                LaserController::setLaserVal(LaserController::PWM_CHANNEL_LASER_3, laser.LASERval);
            }
        }
        else
        {
            log_e("Error: Incorrect data size received in CAN from address %u. Data size is %u", txID, size);
        }
        #endif
    }

    void dispatchIsoTpData(pdu_t &pdu)
    {
        // Parse the received data
        uint32_t rxID = pdu.rxId; // ID from which the message was sent
        uint32_t txID = pdu.txId; // ID to which the message was sent
        uint8_t *data = pdu.data; // Data buffer
        size_t size = pdu.len;    // Data size
        log_i("CAN RXID: %u, TXID: %u, size: %u, own id: %u", rxID, txID, size, getCANAddress());

        // this is coming from the central node, so slaves should react
        /*
        FROM MASTER SENDER => perform an action remotly
        */
        // if the message was sent from the central node, we need to parse the data and perform an action (e.g. _act , _get) on the slave side
        // assuming the slave is a motor, we can parse the data and send it to the motor
        if (rxID == getCANAddress() && (rxID == pinConfig.CAN_ID_MOT_A || rxID == pinConfig.CAN_ID_MOT_X || rxID == pinConfig.CAN_ID_MOT_Y || rxID == pinConfig.CAN_ID_MOT_Z))
        {
            parseMotorAndHomeData(data, size, rxID, rxID);
        }
        else if (rxID == getCANAddress() && (rxID == pinConfig.CAN_ID_LASER_0 || rxID == pinConfig.CAN_ID_LASER_1 || rxID == pinConfig.CAN_ID_LASER_2) || rxID == pinConfig.CAN_ID_LASER_3)
        {
            parseLaserData(data, size, rxID);
        }
        else
        {
            log_e("Error: Unknown CAN address %u", rxID);
        }

        // CAN RXID: 273, TXID: 256, size: 8
        if (size == sizeof(MotorState) or size == sizeof(HomeState)) // this is coming from the X motor
        {
            /*
            FROM THE DEVICES => update the state
            */
            Stepper mStepper = static_cast<Stepper>(pinConfig.REMOTE_MOTOR_AXIS_ID);
            if (rxID == pinConfig.CAN_ID_MOT_A)
            {
                mStepper = Stepper::A;
            }
            else if (rxID == pinConfig.CAN_ID_MOT_X)
            {
                mStepper = Stepper::X;
            }
            else if (rxID == pinConfig.CAN_ID_MOT_Y)
            {
                mStepper = Stepper::Y;
            }
            else if (rxID == pinConfig.CAN_ID_MOT_Z)
            {
                mStepper = Stepper::Z;
            }
            log_i("Received MotorState from CAN, currentPosition: %i, isRunning: %i from axis: %i", data[0], data[1], mStepper);
            parseMotorAndHomeState(data, size, rxID, mStepper);
        }
    }

    uint32_t getCANAddressPreferences()
    {
        Preferences preferences;
        preferences.begin("CAN", false);
        // if value not present yet, initialize:
        if (!preferences.isKey("address"))
        {
            preferences.putUInt("address", pinConfig.CAN_ID_CURRENT);
        }
        uint32_t address = preferences.getUInt("address", pinConfig.CAN_ID_CURRENT);
        preferences.end();
        return address;
    }

    uint32_t getCANAddress()
    {
        return getCANAddressPreferences();
    }

    void setCANAddress(uint32_t address)
    {
        // set the CAN address to the preferences (e.g. map axis)
        // TODO: Decide if we want to disable changing the can address if it'S a master
        if (address == pinConfig.CAN_ID_CENTRAL_NODE)
        {
            log_e("Error: Cannot set CAN address to the central node");
            address = pinConfig.CAN_ID_CENTRAL_NODE;
        }
        log_i("Setting CAN address to %u", address);
        Preferences preferences;
        preferences.begin("CAN", false);
        preferences.putUInt("address", address);
        preferences.end();
    }

    bool isIDInAvailableCANDevices(uint32_t idToCheck)
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

    // generic sender function
    int sendCanMessage(uint32_t receiverID, const uint8_t *data, uint8_t size)
    {
        /*
        This sends a message to the CAN bus via ISO-TP
        - If the receiverID is in the list of non-working motors, the message will not be sent
        - The Broadcast receiverID is 0 -> TODO: Not implemented yet
        - The Master receiverID is CAN_ID_CENTRAL_NODE 0x100 => 256
        - The Motor receiverIDs are CAN_ID_MOT_A, CAN_ID_MOT_X, CAN_ID_MOT_Y, CAN_ID_MOT_Z
        - The Laser receiverIDs are CAN_ID_LASER_0, CAN_ID_LASER_1, CAN_ID_LASER_2, CAN_ID_LASER_3
        */
        lastSend = millis();
        // check if the receiverID is in the list of non-working motors
        if (isIDInAvailableCANDevices(receiverID))
        {
            log_e("Error: ReceiverID %u is in the list of non-working motors", receiverID);
            // return -1;
        }

        if (xSemaphoreTake(canMutex, portMAX_DELAY) == pdTRUE)
        {
            // Send the data
            log_i("Sending data with txID %u, rxID %u, with size %u", receiverID, getCANAddress(), size);
            txPdu.data = (uint8_t *)data;
            txPdu.len = size;
            txPdu.txId = receiverID;      // the target ID
            txPdu.rxId = getCANAddress(); // the current ID
            int ret = isoTpSender.send(&txPdu);
            xSemaphoreGive(canMutex);
            // if ret != 0, we have an error and should add the receiverID to the list of non-working motors
            if (ret != 0)
            {
                // TODO: Is this a good idea? Maybe we should try again later?
                log_e("Error sending data to CAN address; adding %u to the list of non-available CAN Modules", receiverID);
                // add the receiverID to the list of non-working IDs
                nonAvailableCANids[currentCANidListEntry] = receiverID;
                currentCANidListEntry++;
            }
            return ret;
        }
        // Couldn't get the mutex for some reason
        return -1;
    }

    // generic receiver function
    int receiveCanMessage(uint32_t rxID, uint8_t *data)
    {
        /*
        This receives a message from the CAN bus via ISO-TP
        - If the senderID is in the list of non-working motors, the message will not be received
        - The Broadcast senderID is 0 -> TODO: Not implemented yet
        - The Master senderID is CAN_ID_CENTRAL_NODE 0x100 => 256
        - The Motor senderIDs are CAN_ID_MOT_A, CAN_ID_MOT_X, CAN_ID_MOT_Y, CAN_ID_MOT_Z
        - The Laser senderIDs are CAN_ID_LASER_0, CAN_ID_LASER_1, CAN_ID_LASER_2, CAN_ID_LASER_3

        We are checcking against the rxID

        senderID: the ID of the sender
        data: the data buffer to store the received data
        rxPdu: the pdu_t struct to store the received data
        txPdu: the pdu_t struct to store the data to be sent
        rxId: for whom the data is intended (i.e. the receiver ID)
        txId: who is sending the data (i.e. the transmitter ID)
        */
        if (xSemaphoreTake(canMutex, portMAX_DELAY) == pdTRUE)
        {
            // receive data from any node
            rxPdu.data = genericDataPtr;
            rxPdu.len = sizeof(genericDataPtr);
            rxPdu.rxId = rxID; // broadcast => 0 - listen to all ids
            rxPdu.txId = 0;    // it really (!!!) doesn't matter when receiving frames, could use anything - but we use the current id
            int ret = isoTpSender.receive(&rxPdu, 150);
            xSemaphoreGive(canMutex);
            // check if remote ID is in the list of non-working motors - if yes and we received data, we should remove it from the list
            if (ret == 0 && isIDInAvailableCANDevices(rxID))
            {
                // remove the senderID from the list of non-working motors
                for (int i = 0; i < MAX_CAN_DEVICES; i++) // Iterate through the array
                {
                    // check if ID is in nonAvailableCANids
                    if (nonAvailableCANids[i] == rxID)
                    {
                        log_i("Removing %u from the list of non-working motors", rxID);
                        nonAvailableCANids[i] = 0; // Address found
                    }
                }
            }

            return ret;
        }
        xSemaphoreGive(canMutex);
        return -1;
    }

    void setup()
    {
        // Create a mutex for the CAN bus
        canMutex = xSemaphoreCreateMutex();

        // Initialize CAN bus
        if (!isoTpSender.begin(500, pinConfig.CAN_TX, pinConfig.CAN_RX))
        {
            log_e("Failed to initialize CAN bus");
            return;
        }

        log_i("CAN bus initialized with address %u on pins RX: %u, TX: %u", getCANAddress(), pinConfig.CAN_RX, pinConfig.CAN_TX);

        // Create a queue to store incoming pdu_t
        // canQueue = xQueueCreate(5, sizeof(pdu_t));
        // Create a dedicated task for receiving CAN messages
        // xTaskCreate(canReceiveTask, "canReceiveTask", 2048, NULL, 1, NULL);

        // now we should announce that we are ready to receive data to the master (e.g. send the current address)
        uint8_t mCurrentAddress = getCANAddress();
        sendCanMessage(pinConfig.CAN_ID_CENTRAL_NODE, &mCurrentAddress, sizeof(mCurrentAddress));
    }

    int act(cJSON *doc)
    {
        /*
         extract the address and set it to the preferences
        pinConfig.CAN_ID_CURRENT
        should be decimal
        {"task":"/can_act", "address": 273}
        {"task":"/can_get", "address": 1}
        List of CAN Addresses is in the PinConfig.h
            uint32_t CAN_ID_MOT_A = 0x110 => 272
            uint32_t CAN_ID_MOT_X = 0x111 => 273
            uint32_t CAN_ID_MOT_Y = 0x112 => 274
            uint32_t CAN_ID_MOT_Z = 0x113 => 275

            uint32_t CAN_ID_LASER_0 = 0x220 => 544
            uint32_t CAN_ID_LASER_1 = 0x221 => 545
            uint32_t CAN_ID_LASER_2 = 0x222 => 546
            uint32_t CAN_ID_LASER_3 = 0x223 => 547
        */
        cJSON *address = cJSON_GetObjectItem(doc, "address");
        if (address != NULL)
        {
            setCANAddress(address->valueint);
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

        else
            log_i("Motor json is null");
        return 0;
    }

    uint32_t axis2id(int axis)
    {
        if (axis >= 0 && axis < 4)
        {
            return CAN_MOTOR_IDs[axis];
        }
        return 0;
    }

    void startStepper(MotorData *data, int axis, bool reduced)
    {
#ifdef MOTOR_CONTROLLER
        if (getData()[axis] != nullptr)
        {
            // positionsPushedToDial = false;
            log_i("Starting motor on axis %i with speed %i, targetPosition %i, reduced: %i", axis, getData()[axis]->speed, getData()[axis]->targetPosition, reduced);
            getData()[axis]->isStop = false; // ensure isStop is false
            getData()[axis]->stopped = false;
            int err = sendMotorDataToCANDriver(*getData()[axis], axis, reduced);
            if (err != 0)
            {
                log_e("Error starting motor on axis %i, we have to add this to the list of non-working motors", axis);
            }
        }
#endif
    }

    void stopStepper(Stepper axis)
    {
// stop the motor
#ifdef MOTOR_CONTROLLER
        log_i("Stopping motor on axis %i", axis);
        getData()[axis]->isStop = true;
        getData()[axis]->stopped = true;
        int err = sendMotorDataToCANDriver(*getData()[axis], axis, true);
        if (err != 0)
        {
            log_e("Error starting motor on axis %i, we have to add this to the list of non-working motors", axis);
        }
#endif
    }

    void sendMotorStateToCANMaster(MotorData motorData)
    {
        // send motor state to master via I2C
        uint32_t slave_addr = getCANAddress();
        uint32_t receiverID = pinConfig.CAN_ID_CENTRAL_NODE;
        MotorState motorState;
        motorState.currentPosition = motorData.currentPosition;
        motorState.isRunning = !motorData.stopped;
        uint8_t *dataPtr = (uint8_t *)&motorState;
        int dataSize = sizeof(MotorState);
        int err = sendCanMessage(receiverID, dataPtr, dataSize);
        if (err != 0)
        {
            log_e("Error sending motor state to CAN master at address %i", slave_addr);
        }
        else
        {
            log_i("MotorState to master at address %i, currentPosition: %i, isRunning: %i, size %i", slave_addr, motorState.currentPosition, motorState.isRunning, dataSize);
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
        return !getData()[axis]->stopped;
#else
        return false;
#endif
    }

    int sendMotorDataToCANDriver(MotorData motorData, uint8_t axis, bool reduced)
    {
        // send motor data to slave via I2C
        uint32_t slave_addr = axis2id(axis);

        int err = 0;
        int dataSize = 0;
        if (reduced)
        {
            log_i("Reducing MotorData to axis: %i at address: %u", axis, slave_addr);
            MotorDataReduced reducedData;
            reducedData.targetPosition = motorData.targetPosition;
            reducedData.isforever = motorData.isforever;
            reducedData.absolutePosition = motorData.absolutePosition;
            reducedData.speed = motorData.speed;
            reducedData.isStop = motorData.isStop;

            uint8_t *dataPtr = (uint8_t *)&reducedData;
            dataSize = sizeof(MotorDataReduced);
            err = sendCanMessage(slave_addr, dataPtr, dataSize);
        }
        else
        {
            log_i("Sending MotorData to axis: %i", axis);
            // Cast the structure to a byte array
            uint8_t *dataPtr = (uint8_t *)&motorData;
            dataSize = sizeof(MotorData);
            err = sendCanMessage(slave_addr, dataPtr, dataSize);
        }
        if (err != 0)
        {
            log_e("Error sending motor data to CAN slave at address %i", slave_addr);
        }
        else
        {
            log_i("MotorData to axis: %i, at address %i, isStop: %i, speed: %i, targetPosition:%i, reduced %i, stopped %i, isaccel: %i, accel: %i, isEnable: %i, isForever %i, size %i", axis, slave_addr, motorData.isStop, motorData.speed, motorData.targetPosition, reduced, motorData.stopped, motorData.isaccelerated, motorData.acceleration, motorData.isEnable, motorData.isforever, dataSize);
        }
        return err;
    }

    void sendHomeDataToCANDriver(HomeData homeData, uint8_t axis)
    {
        // send home data to slave via
        uint32_t slave_addr = axis2id(axis);
        log_i("Sending HomeData to axis: %i", axis);
        uint8_t *dataPtr = (uint8_t *)&homeData;
        int dataSize = sizeof(HomeData);
        int err = sendCanMessage(slave_addr, dataPtr, dataSize);
        if (err != 0)
        {
            log_e("Error sending home data to CAN slave at address %i", slave_addr);
        }
        else
        {
            log_i("Home data sent to CAN slave at address %i", slave_addr);
        }
    }

    void sendHomeStateToMaster(HomeState homeState)
    {
        // send home state to master via I2C
        uint32_t receiverID = pinConfig.CAN_ID_CENTRAL_NODE;
        uint8_t *dataPtr = (uint8_t *)&homeState;
        int dataSize = sizeof(HomeState);
        int err = sendCanMessage(receiverID, dataPtr, dataSize);
        if (err != 0)
        {
            log_e("Error sending home state to CAN master at address %i", receiverID);
        }
        else
        {
            log_i("HomeState to master at address %i, isHoming: %i, homeInEndposReleaseMode: %i, size %i", receiverID, homeState.isHoming, homeState.homeInEndposReleaseMode, dataSize);
        }
    }

    void sendTMCDataToCANDriver(TMCData tmcData, int axis)
    {
        // send TMC Data to remote Motor
        uint32_t slave_addr = axis2id(axis);
        log_i("Sending TMCData to axis: %i", axis);
        uint8_t *dataPtr = (uint8_t *)&tmcData;
        int dataSize = sizeof(TMCData);
        int err = sendCanMessage(slave_addr, dataPtr, dataSize);
        if (err != 0)
        {
            log_e("Error sending TMC data to CAN slave at address %i", slave_addr);
        }
        else
        {
            log_i("TMCData to axis: %i, at address %i, msteps: %i, rms_current: %i, stall_value: %i, sgthrs: %i, semin: %i, semax: %i, size %i", axis, slave_addr, tmcData.msteps, tmcData.rms_current, tmcData.stall_value, tmcData.sgthrs, tmcData.semin, tmcData.semax, dataSize);
        }
    }

    void sendLaserDataToCANDriver(LaserData laserData)
    {
        // send laser data to master via I2C
        // convert the laserID to the CAN address
        int laserID = laserData.LASERid;
        uint32_t receiverID = CAN_LASER_IDs[pinConfig.REMOTE_LASER_ID];
        if (0)
        {
            // this is only if we wanted to spread the lasers over different CAN addresses // TODO: check if this is necessary at one point
            receiverID = CAN_LASER_IDs[laserID];
        }

        uint8_t *dataPtr = (uint8_t *)&laserData;
        int dataSize = sizeof(LaserData);
        int err = sendCanMessage(receiverID, dataPtr, dataSize);
        if (err != 0)
        {
            log_e("Error sending laser data to CAN master at address %i", receiverID);
        }
        else
        {
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
        int addr = getCANAddress();
        cJSON_AddNumberToObject(doc, "address", addr);

        // add the list of non-working CAN IDs
        cJSON *nonworkingArray = cJSON_CreateIntArray((const int *)nonAvailableCANids, MAX_CAN_DEVICES);
        cJSON_AddItemToObject(doc, "nonworking", nonworkingArray);

        // add the pins for RX/TX to the CAN bus
        cJSON_AddNumberToObject(doc, "rx", pinConfig.CAN_RX);
        cJSON_AddNumberToObject(doc, "tx", pinConfig.CAN_TX);

        return doc;
    }

    void loop()
    {
        // Check if there's a new message
        if (millis() - lastSend >= 10)
        {
            // pdu->txId = <other node's ID>
            // pdu->rxId = <my ID>
            uint32_t mCurrentAddress = getCANAddress();
            // receive only messages that are sent to the current address
            int mError = receiveCanMessage(mCurrentAddress, (uint8_t *)&genericDataPtr);

            // int mError = receiveCanMessage(0, (uint8_t *)&genericDataPtr); // listen to all messages  - does it make sense?

            // parse the data depending on the ID's strucutre and size
            if (mError == 0)
            {
                dispatchIsoTpData(rxPdu);
            }
        }
        /*
        else if (false) // this does not work, consecutive frames are not received in time
        {
            static pdu_t rxPdu;
            // Non-blocking check if there's a new message
            if (xQueueReceive(canQueue, &rxPdu, 0) == pdTRUE)
            {
                // Process received data
                log_i("Sender: Received data from ID %u", rxPdu.rxId);
                dispatchIsoTpData(rxPdu);
            }
        }
        */
    }
}
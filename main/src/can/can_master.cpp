#include "can_master.h"
#include <PinConfig.h>
#include "Wire.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_debug_helpers.h"

#include "JsonKeys.h"
#include <Preferences.h>
#ifdef DIAL_CONTROLLER
#include "../dial/DialController.h"
#endif
#ifdef LASER_CONTROLLER
#include "../laser/LaserController.h"
#endif

using namespace FocusMotor;
namespace can_master
{

    // for A,X,Y,Z intialize the CAN addresses
    uint8_t can_addresses[] = {
        pinConfig.CAN_ADD_MOT_A,
        pinConfig.CAN_ADD_MOT_X,
        pinConfig.CAN_ADD_MOT_Y,
        pinConfig.CAN_ADD_MOT_Z};

    uint8_t CAN_ADDRESS_MASTER = 0;

    uint8_t can_laseraddresses[] = {
        pinConfig.CAN_ADD_LEX_PWM0,
        pinConfig.CAN_ADD_LEX_PWM1,
        pinConfig.CAN_ADD_LEX_PWM2,
        pinConfig.CAN_ADD_LEX_PWM3};

    // keep track of the motor states
    const int MAX_CAN_DEVICES = 20;     // Maximum number of expected devices
    byte canAddresses[MAX_CAN_DEVICES]; // Array to store found CAN addresses
    int numDevices = 0;                 // Variable to keep track of number of devices found
    int canRescanTick = 0;              // Variable to keep track of number of devices found
    int canRescanAfterNTicks = -1;      // Variable to keep track of number of devices found
    int pullMotorDataCANDriverTick[4] = {0, 0, 0, 0};

    void CANListenerTask(void *param)
    {
        CanFrame frame;

        while (true)
        {
            if (ESP32Can.readFrame(&frame))
            {
                uint8_t nodeID = GET_NODE_ID(frame.identifier);
                uint8_t messageType = GET_MESSAGE_TYPE(frame.identifier);

                MultiFrameBuffer *buffer = &multiFrameBuffers[nodeID];

                uint8_t chunkIndex = frame.data[0]; // Frame number
                bool isStart = frame.data[1] == 1;  // Start of message
                bool isEnd = frame.data[2] == 1;    // End of message

                if (isStart)
                {
                    buffer->receivedSize = 0;
                    buffer->currentFrame = 0;
                    buffer->complete = false;
                }

                if (chunkIndex == buffer->currentFrame)
                {
                    size_t chunkSize = frame.data_length_code - 3;
                    memcpy(buffer->buffer + buffer->receivedSize, &frame.data[3], chunkSize);
                    buffer->receivedSize += chunkSize;
                    buffer->currentFrame++;

                    if (isEnd)
                    {
                        buffer->complete = true;

                        CANMessage message;
                        message.nodeID = nodeID;
                        message.messageType = messageType;
                        message.dataSize = buffer->receivedSize;
                        memcpy(message.data, buffer->buffer, buffer->receivedSize);

                        // Push complete message to queue
                        if (xQueueSend(messageQueue, &message, pdMS_TO_TICKS(10)) != pdTRUE)
                        {
                            log_e("Queue full. Message from Node %i dropped.", nodeID);
                        }

                        log_i("Multi-frame message complete: Node %i, Type %i", nodeID, messageType);
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to prevent task starvation
        }
    }

    void sendMotorStateToMaster(){
        MotorState motorState; 
            long currentPosition = getData()[I2C_MOTOR_AXIS]->currentPosition; // Get the current position of the motor
	        bool isRunning = getData()[I2C_BUFFER_LENGTH]->isRunning; // Get the current running state of the motor
            // now send it back to the master
            uint16_t msgID = CAN_ADDRESS_MASTER;
            sendSegmentedDataCAN(msgID, CAN_MESSAGE_TYPE::MOTOR_STATE, &motorState, sizeof(MotorState)); 
    }

    void processCANMessage(const CANMessage &message)
    {
        /*
        This function processes all the possible message types that can be received from the CAN bus.
        Therefore, there is no clear distinction between master and slave anymore. Everyone can send, everyone can receive.

        */
        switch (message.messageType)
        {
        case CAN_MESSAGE_TYPE::MOTOR_ACT:
        {
            // A motor command from a master arrives at the slave and will be executed
            // THIS CODE is executed on the SLAVE
            uint8_t motorAxis = address2axis(message.nodeID);
            MotorData motorData;
            memcpy(&motorData, message.data, sizeof(MotorData));
            log_i("MotorState at motorAxis %i, Position=%ld, Running=%d", motorAxis, motorData.currentPosition, motorData.isRunning);
            // TODO: Implement the start_stepper logic, check which MotorID is being addressed
            getData()[motorAxis]->currentPosition = motorData.currentPosition;
            getData()[motorAxis]->isRunning = motorData.isRunning;
            getData()[motorAxis]->isforever = motorData.isforever;
            getData()[motorAxis]->isStop = motorData.isStop;
            getData()[motorAxis]->speed = motorData.speed;
            getData()[motorAxis]->acceleration = motorData.acceleration;
            bool isStop = getData()[motorAxis]->isStop;
            FocusMotor::toggleStepper(motorAxis, isStop, false);
            break;
        }
        case CAN_MESSAGE_TYPE::MOTOR_GET:
        {
            // The CAN master requests the position of the slave under a certain node ID, we have to 
            // respond with the current motor state and send it via CAN 
            // THIS IS SLAVE CODE
            sendMotorStateToMaster();
            break;
        }
        case CAN_MESSAGE_TYPE::MOTOR_STATE:
        {
            // The slave sends the motor state to the master
            // THIS IS MASTER CODE
            MotorState motorState;
            int mMotorAxis = address2axis(message.nodeID);
            memcpy(&motorState, message.data, sizeof(MotorState));
            log_i("MotorState at axis %d: Position=%ld, Running=%d", mMotorAxis, motorState.currentPosition, motorState.isRunning);
            // now we have to assign this to the respective axis 
            // convert nodeID to motoraxis 
            getData()[motorAxis]->currentPosition = motorState.currentPosition;
            getData()[motorAxis]->isRunning = motorState.isRunning;
            break;
        }

        case CAN_MESSAGE_TYPE::HOME_SET:
        {
            HomeState homeState;
            memcpy(&homeState, message.data, sizeof(HomeState));
            log_i("HomeState: Active=%d, EndReleaseMode=%d", homeState.homeIsActive, homeState.homeInEndposReleaseMode);
            break;
        }
        default:
            log_w("Unknown message type %i from Node %i", message.messageType, message.nodeID);
            break;
        }
    }

    bool sendSegmentedDataCAN(uint16_t msgID, uint8_t messageType, void *data, size_t dataSize)
    {
        const uint8_t chunkSize = 8; // CAN frame payload size (max 8 bytes)
        uint8_t *dataPtr = (uint8_t *)data;

        for (uint8_t i = 0; i < (dataSize + chunkSize - 3 - 1) / (chunkSize - 3); i++) // Segmented
        {
            CanFrame frame;
            frame.identifier = msgID; // Unique ID per axis or motor
            frame.extd = 0;           // Standard CAN ID
            frame.data_length_code = chunkSize;

            frame.data[0] = i;                                               // Chunk index
            frame.data[1] = (i == 0) ? 1 : 0;                                // Start flag
            frame.data[2] = ((i + 1) * (chunkSize - 3) >= dataSize) ? 1 : 0; // End flag
            frame.data[3] = messageType;                                     // Message type (e.g., MOTOR_STATE)

            // Copy a portion of the data to the CAN frame
            size_t remainingBytes = dataSize - (i * (chunkSize - 4));
            size_t copySize = min(chunkSize - 4, remainingBytes);
            memcpy(&frame.data[4], dataPtr + (i * (chunkSize - 4)), copySize);

            // Send the frame and check for success
            if (!ESP32Can.writeFrame(&frame))
            {
                log_e("Failed to send CAN frame for chunk %d", i);
                return false;
            }
        }

        return true; // All chunks sent successfully
    }

    void can_scan()
    {
    }

    void setup()
    {

        // Initialize CAN
        ESP32Can.begin(ESP32Can.convertSpeed(500));
        ESP32Can.setPins(CAN_TX, CAN_RX);
        can_scan();

        // Create message queue
        messageQueue = xQueueCreate(CAN_QUEUE_LENGTH, sizeof(CANMessage));
        if (!messageQueue)
        {
            log_e("Failed to create message queue!");
            return;
        }

        // Create CAN listener task
        xTaskCreatePinnedToCore(
            CANListenerTask,      // Task function
            "CANListenerTask",    // Task name
            CAN_RX_TASK_STACK,    // Stack size
            NULL,                 // Task parameters
            CAN_RX_TASK_PRIORITY, // Task priority
            NULL,                 // Task handle
            1                     // Core to pin the task
        );

        log_i("CAN Listener Task started.");
    }

    /**************************************
    MOTOR
    *************************************/

    MotorState getMotorState(int i)
    {
        MotorState mMotorState = pullMotorDataCANDriver(i);
        return mMotorState;
    }

    int axis2address(int axis)
    {
        return axis;
    }

    int address2axis(int address)
    {
        return address;
    }

    void sendMotorDataToCANDriver(MotorData motorData, uint8_t axis, bool reduced = false)
    {
        // Function to send motor data via CAN
        uint8_t msgID = axis2address(axis); // CAN identifier derived from the axis
        bool mResult = false;

        if (false) //reduced)
        {
            // Reduced version of MotorData
            MotorDataReduced reducedData;
            reducedData.targetPosition = motorData.targetPosition;
            reducedData.isforever = motorData.isforever;
            reducedData.absolutePosition = motorData.absolutePosition;
            reducedData.speed = motorData.speed;
            reducedData.isStop = motorData.isStop;

            // Send reduced data segmented with MOTOR_STATE message type
            mResult = sendSegmentedDataCAN(msgID, CAN_MESSAGE_TYPE::MOTOR_ACT:, &reducedData, sizeof(MotorDataReduced));
        }
        else
        {
            // Send full MotorData struct segmented with MOTOR_STATE message type
            mResult = sendSegmentedDataCAN(msgID, CAN_MESSAGE_TYPE::MOTOR_ACT:, &motorData, sizeof(MotorData));
        }

        if (!mResult)
            log_e("MotorData not sent to CAN Slave");
        else
            log_i("MotorData successfully sent to CAN Slave on axis %d", axis);
    }

    void startStepper(MotorData *data, int axis, bool reduced)
    {
        if (data != nullptr)
        {
            // positionsPushedToDial = false;
            data->isStop = false; // ensure isStop is false
            data->stopped = false;
            sendMotorDataToCANDriver(*data, axis, reduced);
        }
        else
        {
            ESP_LOGE("CAN_MASTER", "Invalid motor data for axis %d", axis);
        }
    }

    void stopStepper(MotorData *data, int axis)
    {
        // esp_backtrace_print(10);
        //   only send motor data if it was running before
        log_i("Stop Motor %i in CAN Master", axis);
        sendMotorDataToCANDriver(*data, axis, false);
    }

    void setPosition(Stepper s, int pos)
    {
        // TODO: Change!
        setPositionCANDriver(s, 0);
    }

    void setPositionCANDriver(Stepper s, long pos)
    {
        // Create a CAN frame to send the position
        CanFrame frame;
        uint8_t canID = axis2address(axis);
        frame.identifier = canID; // Unique CAN ID for each stepper axis
        frame.extd = 0;           // Standard CAN frame
        frame.data_length_code = sizeof(long);

        // Copy the position into the CAN data payload
        memcpy(frame.data, &pos, sizeof(long));

        // Send the CAN frame
        if (!ESP32Can.writeFrame(&frame))
        {
            log_e("Error sending position to CAN slave on axis %i", s);
        }
        else
        {
            log_i("Position set to %i on axis %i", pos, s);
        }
    }

    bool requestMotorDataCANDriver(int axis)
    {
        /*
        We pull the motor position from one of the external motor boards
        */
        uint8_t canID = axis2address(axis); // CAN identifier for this axis
        const uint8_t maxRetries = 2;
        uint8_t retryCount = 0;
        bool success = false;CAN_ADDRESS_MASTER

        // Prepare the request type for MotorState
        uint8_t requestType = CAN_MESSAGE_TYPE::MOTOR_GET;

        // Send the request for MOTOR_STATE
        if (!sendSegmentedDataCAN(canID, CAN_MESSAGE_TYPE::MOTOR_GET, &requestType, sizeof(requestType)))
        {
            log_e("Error sending MotorState request to axis %i", axis);
            return false;
        }
        else
        {
            return true;
        }
    }

    /***************************************
    HOME
    ***************************************/

    void sendHomeDataCAN(HomeData homeData, uint8_t axis)
    {
        // send home data to slave via CAN and initiate homing
        uint8_t msgID = axis2address(axis);
        log_i("HomeData to axis: %i ", axis);
        if (sendSegmentedDataCAN(msgID, &homeData, sizeof(HomeData)))
            log_i("HomeData sent to CAN Slave");
        else
            log_i("HomeData not sent to CAN Slave");
    }

    HomeState pullHomeStateFromCANDriver(int axis)
    {

        // we pull the data from the slave's register
        uint8_t slave_addr = axis2address(axis);

        // Request data from the slave but only if inside canAddresses
        if (!isAddressInCANDevices(slave_addr))
        {
            // log_e("Error: CAN slave address %i not found in canAddresses", slave_addr);
            return HomeState();
        }

        // read the data from the slave
        const int maxRetries = 2;
        int retryCount = 0;
        bool success = false;
        HomeState homeState; // Initialize with default values
        while (retryCount < maxRetries && !success)
        {
            // First send the request-code to the slave
            Wire.beginTransmission(slave_addr);
            Wire.write(REQUEST_HOMESTATE);
            Wire.endTransmission();

            // temporarily disable watchdog in case of slow CAN communication
            Wire.requestFrom(slave_addr, sizeof(HomeState));
            if (Wire.available() == sizeof(HomeState))
            {
                Wire.readBytes((uint8_t *)&homeState, sizeof(homeState));
                success = true;
            }
            else
            {
                retryCount++;
                log_w("Warning: Incorrect data size received from address %i. Data size is %i. Retry %i/%i", slave_addr, Wire.available(), retryCount, maxRetries);
                delay(20); // Wait a bit before retrying
            }
        }
        return homeState;
    }

    /***************************************
     * CAN
     ***************************************/

    bool isAddressInCANDevices(byte addressToCheck)
    {
        return true; // Address not found
    }

    /***************************************
     * Laser
     ***************************************/

    int laserid2address(int id)
    {
        // we need to check if the id is in the range of the laser addresses
        if (id >= 0 && id < numDevices)
        {
            return can_laseraddresses[id];
        }
        return 0;
    }

    void sendLaserDataCAN(LaserData laserdata, uint8_t id)
    {
    }

    /***************************************
     * TMC
     ***************************************/

#ifdef TMC_CONTROLLER
    void sendTMCDataCAN(TMCData tmcData, uint8_t axis)
    {
    }
#endif

    int act(cJSON *doc)
    {
        // do nothing
        return 0;
    }

    cJSON *get(cJSON *ob)
    {
        // return a list of all CAN devices {"task":"/can_get"}
        cJSON *canDevices = cJSON_CreateArray();
        for (int i = 0; i < numDevices; i++)
        {
            cJSON *canDevice = cJSON_CreateObject();
            cJSON_AddNumberToObject(canDevice, "address", canAddresses[i]);
            cJSON_AddItemToArray(canDevices, canDevice);
        }
        cJSON_AddItemToObject(ob, "canDevices", canDevices);
        return ob;
    }

    void loop()
    {
        CANMessage receivedMessage;

        if (xQueueReceive(messageQueue, &receivedMessage, pdMS_TO_TICKS(100)))
        {
            processCANMessage(receivedMessage);
        }

        // add anything that would eventually require a pull from the CAN bus
#ifdef DIAL_CONTROLLER
        if (ticksLastPosPulled >= ticksPosPullInterval)
        {
            ticksLastPosPulled = 0;
            // Here we want to pull the dial data from the CAN bus and assign it to the motors
            pullParamsFromDial();
        }
        else
        {
            ticksLastPosPulled++;
        }
#endif
    }
}
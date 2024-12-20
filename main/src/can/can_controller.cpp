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
#ifdef LASER_CONTROLLER
#include "../laser/LaserController.h"
#endif

using namespace FocusMotor;
namespace can_controller
{

    // for A,X,Y,Z intialize the CAN addresses
    uint8_t CAN_ADDRESS_MASTER = 0;

    // keep track of the motor states
    const int MAX_CAN_DEVICES = 20;     // Maximum number of expected devices
    byte canAddresses[MAX_CAN_DEVICES]; // Array to store found CAN addresses
    int numDevices = 0;                 // Variable to keep track of number of devices found
    int canRescanAfterNTicks = -1;      // Variable to keep track of number of devices found
    int pullMotorDataCANDriverTick[4] = {0, 0, 0, 0};
    // std::map<uint8_t, uint32_t> lastHeartbeatReceived; // Tracks last heartbeat from nodes

    // 29-Bit Identifier Scheme
    // We structure the 29-bit identifier as follows:
    //
    // Bits   Field            Purpose
    // 8      Destination ID   Target device or node (0-255)
    // 8      Source ID        Sender device or node (0-255)
    // 8      Message Type     Command/Response type (0-255)
    // 5      Additional Flags R/W, priority, heartbeat flag, etc.
    //
    // This allows:
    // - Up to 256 devices (0-255 for source and destination IDs).
    // - 256 message types per communication.
    // - Additional flags to indicate message priority, error states, or read/write actions.

    uint8_t getSelfCANID()
    {
        // Get the I2C address of the slave
        // Load the I2C address form the preference
        Preferences preferences;
        preferences.begin("CAN", false);
        int can_id = preferences.getInt("can_id", pinConfig.CAN_DEFAULT_ID);
        preferences.end();
        return can_id;
    }

    uint32_t constructExtendedCANID(uint8_t destID, uint8_t messageType, uint8_t flags)
    {
        uint32_t id = 0;
        uint8_t sourceID = getSelfCANID();
        id |= ((uint32_t)destID & 0xFF) << 21;     // Destination ID (8 bits)
        id |= ((uint32_t)sourceID & 0xFF) << 13;   // Source ID (8 bits)
        id |= ((uint32_t)messageType & 0xFF) << 5; // Message Type (8 bits)
        id |= (flags & 0x1F);                      // Flags (5 bits)
        return id;
    }

    void decodeExtendedCANID(uint32_t id, uint8_t &sourceID, uint8_t &destID, uint8_t &messageType, uint8_t &flags)
    {
        // id corresponds to the 29-bit CAN ID
        destID = (id >> 21) & 0xFF;     // Extract Destination ID
        sourceID = (id >> 13) & 0xFF;   // Extract Source ID
        messageType = (id >> 5) & 0xFF; // Extract Message Type
        flags = id & 0x1F;              // Extract Flags
    }

    void CANListenerTask(void *param)
    {
        CanFrame frame;
        log_i("CAN Listener Task started.");
        int iterator = 0;
        while (true)
        {
            iterator++;
            uint16_t timeout = 100;
            if (ESP32Can.readFrame(&frame, timeout))
            {
                // Decode Extended CAN ID
                uint8_t sourceID, destID, messageType, flags;
                decodeExtendedCANID(frame.identifier, sourceID, destID, messageType, flags);
                log_i("Message received from Node %i to Node %i, Type %i", sourceID, destID, messageType);

                // Select the buffer for the source node
                MultiFrameBuffer *buffer = &multiFrameBuffers[sourceID];

                uint8_t chunkIndex = frame.data[0]; // Frame number
                bool isStart = frame.data[1] == 1;  // Start of message
                bool isEnd = frame.data[2] == 1;    // End of message

                if (isStart)
                {
                    // Reset buffer for a new multi-frame message
                    log_i("New multi-frame message from Node %i", sourceID);
                    buffer->receivedSize = 0;
                    buffer->currentFrame = 0;
                    buffer->complete = false;
                }

                if (chunkIndex == buffer->currentFrame)
                {
                    size_t chunkSize = frame.data_length_code - 3;
                    if (buffer->receivedSize + chunkSize <= sizeof(buffer->buffer))
                    {
                        // Copy the current chunk of data into the buffer
                        memcpy(buffer->buffer + buffer->receivedSize, &frame.data[3], chunkSize);
                        buffer->receivedSize += chunkSize;
                        buffer->currentFrame++;
                    }
                    else
                    {
                        log_e("Buffer overflow for Node %i", sourceID);
                        continue;
                    }

                    if (isEnd)
                    {
                        // Mark message as complete
                        buffer->complete = true;

                        CANMessage message;
                        message.sourceID = sourceID;
                        message.destID = destID;
                        message.messageType = messageType;
                        message.dataSize = buffer->receivedSize;
                        memcpy(message.data, buffer->buffer, buffer->receivedSize);

                        // Push complete message to queue
                        if (xQueueSend(messageQueue, &message, pdMS_TO_TICKS(10)) != pdTRUE)
                        {
                            log_e("Queue full. Message from Node %i dropped.", sourceID);
                        }
                        else
                        {
                            log_i("Multi-frame message complete: Node %i, Type %i, Size: %i bytes",
                                  sourceID, messageType, message.dataSize);
                        }
                    }
                }
                else
                {
                    log_w("Unexpected frame index from Node %i: expected %i, got %i",
                          sourceID, buffer->currentFrame, chunkIndex);
                }
            }

            vTaskDelay(pdMS_TO_TICKS(1)); // Small delay to prevent task starvation
        }
    }

    void processCANMessage(const CANMessage &message)
    {
        /*
        This function processes all the possible message types that can be received from the CAN bus.
        Therefore, there is no clear distinction between master and slave anymore. Everyone can send, everyone can receive.
        */

        uint8_t sourceID = message.sourceID;
        uint8_t destID = message.destID;
        uint8_t messageType = message.messageType;

        log_i("Message received from Node %i to Node %i, Type %i", sourceID, destID, messageType);

        if (destID != 0x00 && destID != 0xFF)
        { // Ignore if not for the gateway or broadcast
            log_i("Ignoring message for Node %i", destID);
            return;
        }

        switch (messageType)
        {
        case CANMessageTypeID::HEARTBEAT:
        {
            log_i("Heartbeat received from Node %i", sourceID);
            // lastHeartbeatReceived[sourceID] = millis(); // Update last received heartbeat time

            // Respond to heartbeat request
            if (message.data[0] == 0x02)
            {
                sendHeartbeatCAN();
            }
            break;
        }
        case CANMessageTypeID::MOTOR_ACT_REDUCED:
        {
            /*
             * A reduced motor command (MOTOR_ACT_REDUCED) has arrived at the slave.
             * This is a more compact command for reduced bandwidth situations.
             * The message payload will be smaller and only contain essential parameters.
             */
            uint8_t motorAxis = address2axis(message.destID); // Convert destination ID to motor axis
            MotorDataReduced motorData;
            if (message.dataSize >= sizeof(MotorDataReduced))
            {
                memcpy(&motorData, message.data, sizeof(MotorDataReduced));
                log_i("Reduced Motor Command received for axis %i: TargetPos=%ld, Speed=%i, isStop=%i",
                      motorAxis, motorData.targetPosition, motorData.speed, motorData.isStop);

                // Apply reduced motor data to the motor state
                getData()[motorAxis]->targetPosition = motorData.targetPosition;
                getData()[motorAxis]->isforever = motorData.isforever;
                getData()[motorAxis]->speed = motorData.speed;
                getData()[motorAxis]->isStop = motorData.isStop;

                // Execute the motor action
                FocusMotor::toggleStepper(static_cast<Stepper>(motorAxis), motorData.isStop, false);
            }
            else
            {
                log_e("Invalid data size for MOTOR_ACT_REDUCED: expected %u, received %u",
                      sizeof(MotorDataReduced), message.dataSize);
            }
            break;
        }
        case CANMessageTypeID::MOTOR_ACT:
        {
            /*
             * A full motor command (MOTOR_ACT) has arrived at the slave.
             * The full MotorData structure will be used for this action.
             */
            uint8_t motorAxis = address2axis(message.destID); // Convert destination ID to motor axis
            MotorData motorData;

            if (message.dataSize >= sizeof(MotorData))
            {
                memcpy(&motorData, message.data, sizeof(MotorData));
                log_i("Full Motor Command received for axis %i: TargetPos=%ld, Speed=%i, Accel=%i, isStop=%i",
                      motorAxis, motorData.targetPosition, motorData.speed, motorData.acceleration, motorData.isStop);

                // Update motor state
                getData()[motorAxis]->currentPosition = motorData.currentPosition;
                getData()[motorAxis]->targetPosition = motorData.targetPosition;
                getData()[motorAxis]->isforever = motorData.isforever;
                getData()[motorAxis]->isStop = motorData.isStop;
                getData()[motorAxis]->speed = motorData.speed;
                getData()[motorAxis]->acceleration = motorData.acceleration;

                // Execute the motor action
                FocusMotor::toggleStepper(static_cast<Stepper>(motorAxis), motorData.isStop, false);
            }
            else
            {
                log_e("Invalid data size for MOTOR_ACT: expected %u, received %u",
                      sizeof(MotorData), message.dataSize);
            }
            break;
        }
        case CANMessageTypeID::MOTOR_GET:
        {
            // The CAN master requests the position of the slave under a certain node ID, we have to
            // respond with the current motor state and send it via CAN
            // THIS IS SLAVE CODE
            sendMotorStateToMaster();
            break;
        }
        case CANMessageTypeID::MOTOR_STATE:
        {
            // The slave sends the motor state to the master
            // THIS IS MASTER CODE
            MotorState motorState;
            int mMotorAxis = address2axis(message.destID);
            memcpy(&motorState, message.data, sizeof(MotorState));
            log_i("MotorState at axis %d: Position=%ld, Running=%d", mMotorAxis, motorState.currentPosition, motorState.isRunning);
            // now we have to assign this to the respective axis
            // convert nodeID to motoraxis
            getData()[mMotorAxis]->currentPosition = motorState.currentPosition;
            // getData()[mMotorAxis]->isRunning = motorState.isRunning; // FIXME: Not working like this!
            break;
        }

        case CANMessageTypeID::HOME_ACT:
        {
            HomeState homeState;
            memcpy(&homeState, message.data, sizeof(HomeState));
            // log_i("HomeState: Active=%d, EndReleaseMode=%d", homeState.homeIsActive, homeState.homeInEndposReleaseMode);
            break;
        }
        default:
            log_w("Unknown message type %i from Node %i to Node %i", message.messageType, message.sourceID, message.destID);
            break;
        }
    }

    bool sendHeartbeatCAN()
    {
        // Send a heartbeat message to all devices
        CanFrame frame;
        // log_i("Sending heartbeat to all nodes on tx%i, rx%i", pinConfig.CAN_TX, pinConfig.CAN_RX);
        frame.identifier = constructExtendedCANID(CANMessageTypeID::BROADCAST, CANMessageTypeID::HEARTBEAT, 0);
        frame.extd = 1; // Extended CAN Frame Format (29-bit ID)
        frame.data_length_code = 1;
        frame.data[0] = 0x01; // Alive signal from master

        return ESP32Can.writeFrame(&frame);
    }

    bool sendSegmentedDataCAN(uint32_t canID, void *data, size_t dataSize)
    {
        const uint8_t chunkSize = 8; // Maximum CAN frame payload size
        uint8_t *dataPtr = (uint8_t *)data;

        for (uint8_t i = 0; i < (dataSize + (chunkSize - 4) - 1) / (chunkSize - 4); i++) // Chunk iteration
        {
            CanFrame frame;
            frame.identifier = canID;           // Extended CAN ID including source and destination and message type
            frame.extd = 1;                     // Use Extended Frame Format (29-bit ID)
            frame.data_length_code = chunkSize; // Set the payload size to the maximum chunk size (e.g., 8 bytes)

            frame.data[0] = i;                                               // Chunk index
            frame.data[1] = (i == 0) ? 1 : 0;                                // Start flag
            frame.data[2] = ((i + 1) * (chunkSize - 4) >= dataSize) ? 1 : 0; // End flag
            frame.data[3] = 0x00;                                            // Reserved byte for future use

            // Copy the current chunk into the frame payload
            size_t remainingBytes = dataSize - (i * (chunkSize - 4));
            size_t copySize = std::min(static_cast<size_t>(chunkSize - 4), remainingBytes);
            memcpy(&frame.data[4], dataPtr + (i * (chunkSize - 4)), copySize);

            // Send the frame
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
        log_i("Starting CAN on DEST ID: %i on Pins TX %i, RX %i", getSelfCANID(), pinConfig.CAN_TX, pinConfig.CAN_RX);

        ESP32Can.setPins(pinConfig.CAN_TX, pinConfig.CAN_RX);
        ESP32Can.begin(ESP32Can.convertSpeed(500));
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
    }

    /**************************************
    MOTOR
    *************************************/

    void sendMotorStateToMaster()
    {
        MotorState motorState;
        long currentPosition = getData()[pinConfig.I2C_MOTOR_AXIS]->currentPosition; // Get the current position of the motor
                                                                                     // bool isRunning = getData()[I2C_BUFFER_LENGTH]->isRunning; // Get the current running state of the motor
        //  now send it back to the master
        uint16_t msgID = CAN_ADDRESS_MASTER;
        sendSegmentedDataCAN(msgID, &motorState, sizeof(MotorState));
    }

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
        /*
         * Sends motor data via CAN.
         * Constructs the CAN ID using the updated schema and sends the data segmented.
         */
        uint8_t destID = axis2address(axis); // Device ID for the target motor
        bool result = false;

        if (reduced)
        {
            // Reduced version of MotorData
            MotorDataReduced reducedData;
            reducedData.targetPosition = motorData.targetPosition;
            reducedData.isforever = motorData.isforever;
            reducedData.absolutePosition = motorData.absolutePosition;
            reducedData.speed = motorData.speed;
            reducedData.isStop = motorData.isStop;

            // Construct the CAN ID and send the reduced data
            uint32_t canID = constructExtendedCANID(destID, CANMessageTypeID::MOTOR_ACT_REDUCED, 0);
            result = sendSegmentedDataCAN(canID, &reducedData, sizeof(MotorDataReduced));
        }
        else
        {
            // Send full MotorData struct
            uint32_t canID = constructExtendedCANID(destID, CANMessageTypeID::MOTOR_ACT, 0);
            result = sendSegmentedDataCAN(canID, &motorData, sizeof(MotorData));
        }

        if (!result)
            log_e("Failed to send MotorData to CAN device (axis %d)", axis);
        else
            log_i("MotorData successfully sent to CAN device on axis %d", axis);
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
            ESP_LOGE("CAN_CONTROLLER", "Invalid motor data for axis %d", axis);
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
        uint8_t canID = axis2address(s);
        frame.identifier = canID; // Unique CAN ID for each stepper axis
        frame.extd = 1;           // Standard CAN frame
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
        bool success = false;

        // Prepare the request type for MotorState
        uint8_t requestType = CANMessageTypeID::MOTOR_GET;

        // Send the request for MOTOR_STATE
        if (!sendSegmentedDataCAN(canID, &requestType, sizeof(requestType)))
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
    }

    HomeState pullHomeStateFromCANDriver(int axis)
    {
        HomeState homeState;
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
            return id;
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
        // send heartbeat periodically to all devices
        if (millis() - canLastHeartbeat > canHeartbeatPeriod)
        {
            canLastHeartbeat = millis();
            sendHeartbeatCAN();
        }

        CANMessage receivedMessage;

        if (xQueueReceive(messageQueue, &receivedMessage, pdMS_TO_TICKS(100)))
        {
            processCANMessage(receivedMessage);
        }
    }
}
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

    // Function to send data over CAN in segments
    bool sendSegmentedData(uint16_t msgID, void *data, size_t dataSize)
    {
        const uint8_t chunkSize = 8; // CAN frame payload size
        uint8_t *dataPtr = (uint8_t *)data;

        for (uint8_t i = 0; i < (dataSize + chunkSize - 1) / chunkSize; i++)
        {
            CanFrame frame;
            frame.identifier = msgID; // Unique ID per axis or motor
            frame.extd = 0;           // Standard CAN ID
            frame.data_length_code = chunkSize;

            frame.data[0] = i;                                         // Chunk index
            frame.data[1] = (i == 0) ? 1 : 0;                          // Start flag
            frame.data[2] = ((i + 1) * chunkSize >= dataSize) ? 1 : 0; // End flag

            // Copy a portion of the data to the CAN frame
            size_t remainingBytes = dataSize - (i * (chunkSize - 3));
            size_t copySize = min(chunkSize - 3, remainingBytes);
            memcpy(&frame.data[3], dataPtr + (i * (chunkSize - 3)), copySize);

            // Send the frame and check for success
            if (!ESP32Can.writeFrame(&frame))
            {
                return false;
            }
            else
            {
                return true;
            }
        }
    }

    void can_scan()
    {
    }

    void setup()
    {
        ESP32Can.setPins(CAN_TX, CAN_RX);
        ESP32Can.begin(ESP32Can.convertSpeed(500));
        can_scan();
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
        if (axis >= 0 && axis < 4)
        {
            return can_addresses[axis];
        }
        return 0;
    }

    // Function to send motor data via CAN
    void sendMotorDataToCANDriver(MotorData motorData, uint8_t axis, bool reduced = false)
    {
        uint8_t msgID = axis2address(axis);
        bool mResult = false;
        if (reduced)
        {
            // Reduced version of MotorData
            MotorDataReduced reducedData;
            reducedData.targetPosition = motorData.targetPosition;
            reducedData.isforever = motorData.isforever;
            reducedData.absolutePosition = motorData.absolutePosition;
            reducedData.speed = motorData.speed;
            reducedData.isStop = motorData.isStop;

            // Send reduced data segmented
            mResult = sendSegmentedData(msgID, &reducedData, sizeof(MotorDataReduced));
        }
        else
        {
            // Send full MotorData struct segmented
            mResult = sendSegmentedData(msgID, &motorData, sizeof(MotorData));
        }
        if(!mResult)
            log_i("MotorData not sent to CAN Slave");
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

    MotorState pullMotorDataCANDriver(int axis)
    {
        /*
        We pull the motor position from one of the external motor boards 
        */
        uint8_t canID = axis2address(axis);
        const uint8_t maxRetries = 2;
        uint8_t retryCount = 0;
        bool success = false;

        // Prepare request frame to request MotorState
        CanFrame requestFrame;
        requestFrame.identifier = canID; // CAN ID for MotorState request
        requestFrame.extd = 0;                  // Standard CAN ID
        requestFrame.data_length_code = 1;      // Single byte for request type
        requestFrame.data[0] = CAN_REQUESTS::REQUEST_MOTORSTATE;

        // Send the request frame
        if (!ESP32Can.writeFrame(&requestFrame))
        {
            log_e("Error sending MotorState request to axis %i", axis);
            return MotorState();
        }

        // Prepare for receiving response
        CanFrame responseFrame;
        MotorState motorState = {};

        while (retryCount < maxRetries && !success)
        {
            if (ESP32Can.readFrame(&responseFrame, 100))
            { // 100 ms timeout
                if (responseFrame.identifier == (0x400 + axis))
                { // Response ID for MotorState
                    if (responseFrame.data_length_code == sizeof(MotorState))
                    {
                        memcpy(&motorState, responseFrame.data, sizeof(MotorState));
                        success = true;
                    }
                    else
                    {
                        log_w("Warning: Incorrect data size received from axis %i. Expected: %i, Received: %i",
                              axis, sizeof(MotorState), responseFrame.data_length_code);
                    }
                }
            }
            retryCount++;
        }

        if (!success)
        {
            log_e("Error: Failed to receive MotorState from axis %i after %i attempts", axis, maxRetries);
            motorState.currentPosition = FocusMotor::getData()[axis]->currentPosition; // Fallback
        }
        else
        {
            log_i("Motor State: position: %i, isRunning: %i, axis %i", motorState.currentPosition, motorState.isRunning, axis);
        }

        return motorState;
    }

    /***************************************
    HOME
    ***************************************/

    void sendHomeDataCAN(HomeData homeData, uint8_t axis)
    {
        // send home data to slave via CAN and initiate homing
        uint8_t msgID = axis2address(axis);
        log_i("HomeData to axis: %i ", axis);
        if(sendSegmentedData(msgID, &homeData, sizeof(HomeData)))
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
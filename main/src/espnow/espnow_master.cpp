#include "espnow_master.h"

using namespace FocusMotor;
namespace espnow_master
{
    // keep track of the motor states
    MotorData a_dat;
    MotorData x_dat;
    MotorData y_dat;
    MotorData z_dat;
    MotorData *data[4];

    Preferences preferences;

    void setup()
    {

        // Start ESP-NOW
        WiFi.mode(WIFI_STA); // Ensure the ESP32 is in station mode
        if (esp_now_init() != ESP_OK)
        {
            log_i("Error initializing ESP-NOW");
            return;
        }
        else
        {
            log_i("ESP-NOW initialized");
        }

        // Register send and receive callbacks
        esp_now_register_send_cb(onDataSent);
        esp_now_register_recv_cb(onDataRecv);

        // Add a broadcast peer for sending commands to all slaves
        esp_now_peer_info_t broadcastPeer = {};
        memset(broadcastPeer.peer_addr, 0xFF, 6); // Broadcast address
        broadcastPeer.channel = 0;
        broadcastPeer.encrypt = false;
        esp_now_add_peer(&broadcastPeer);

        /*
        if (pinConfig.MOTOR_A_STEP >= 0)
        {
            updateMotorData(Stepper::A);
            log_i("Motor A position: %i", data[Stepper::A]->currentPosition);
        }
        if (pinConfig.MOTOR_X_STEP >= 0)
        {
            updateMotorData(Stepper::X);
            log_i("Motor X position: %i", data[Stepper::X]->currentPosition);
        }
        if (pinConfig.MOTOR_Y_STEP >= 0)
        {
            updateMotorData(Stepper::Y);
            log_i("Motor Y position: %i", data[Stepper::Y]->currentPosition);
        }
        if (pinConfig.MOTOR_Z_STEP >= 0)
        {
            updateMotorData(Stepper::Z);
            log_i("Motor Z position: %i", data[Stepper::Z]->currentPosition);
        }
        */
    }

    void updateMotorData(int i)
    {
        // Request the current position from the slave
        /*
        // TODO: Impelemtn this
        MotorState mMotorState = pullMotorDataI2CDriver(i);
        data[i]->currentPosition = mMotorState.currentPosition;
        */
        data[i]->currentPosition = 0;
    }

    long getMotorPosition(int i)
    {
        return data[i]->currentPosition;
    }

    void sendMotorDataToESPNowDriver(MotorData motorData, uint8_t axis, bool reduced = false)
    {
        // send motor data to slave via ESPNOW
        log_i("MotorData to axis: %i, isStop: %i, speed: %i, targetPosition:%i, reduced %i, stopped %i", axis, motorData.isStop, motorData.speed, motorData.targetPosition, reduced, motorData.stopped);
        if (reduced)
        {
            // if we only want to send position, etc. we can reduce the size of the data
            MotorDataI2C reducedData;
            reducedData.targetPosition = motorData.targetPosition;
            reducedData.isforever = motorData.isforever;
            reducedData.absolutePosition = motorData.absolutePosition;
            reducedData.speed = motorData.speed;
            reducedData.isStop = motorData.isStop;

            // Prepare motor command for ESP-NOW
            struct MotorCommand
            {
                uint8_t axis;
                bool reduced;
                int targetPosition;
                int speed;
            };

            MotorCommand command = {axis, reduced, data[axis]->targetPosition, data[axis]->speed};

            // Broadcast the command to all slaves
            uint8_t *dataPtr = (uint8_t *)&reducedData;
            esp_now_send(NULL, dataPtr, sizeof(MotorDataI2C));
        }
        else
        {
            // Cast the structure to a byte array
            uint8_t *dataPtr = (uint8_t *)&motorData;
            esp_now_send(NULL, dataPtr, sizeof(MotorData));
        }
    }

    void startStepper(int axis, bool reduced, bool external)
    {
        if (data != nullptr && data[axis] != nullptr)
        {
            data[axis]->stopped = false;
            log_i("data[axis]->stopped %i, getData()[axis]->stopped %i, axis %i", data[axis]->stopped, FocusMotor::getData()[axis]->stopped, axis);
            sendMotorDataToESPNowDriver(*data[axis], axis, reduced);
        }
        else
        {
            // Fehlerbehandlung, falls data oder data[axis] null ist
            ESP_LOGE("I2C_MASTER", "Invalid motor data for axis %d", axis);
        }
    }

    void stopStepper(int i)
    {
        // only send motor data if it was running before
        log_i("Stop Motor in I2C Master %i", i);
        // if (!data[i]->stopped)
        data[i]->isStop = true;      // FIXME: We should send only those bits that are relevant (e.g. start/stop + payload bytes)
        data[i]->targetPosition = 0; // weird bug, probably not interpreted correclty on slave: If we stop the motor it's taking the last position and moves twice
        data[i]->absolutePosition = false;
        data[i]->stopped = true;                        // FIXME: difference between stopped and isStop?
        sendMotorDataToESPNowDriver(*data[i], i, true); // TODO: This cannot send two motor information simultaenosly
        sendMotorPos(i, 0);
    }

    void toggleStepper(Stepper s, bool isStop, bool reduced = false)
    {
        if (isStop)
        {
            log_i("stop stepper from parseMotorDriveJson");
            stopStepper(s);
        }
        else
        {
            log_i("start stepper from parseMotorDriveJson");
            startStepper(s, reduced, false);
        }
    }

    void setPosition(Stepper s, int pos)
    {
        // TODO: We need to send the position to the slave
    }

    void setMotorStopped(int axis, bool stopped)
    {
        // manipulate the motor data
        data[axis]->stopped = stopped;
    }

    void setPositionI2CDriver(Stepper s, long pos)
    {
        // TODO: send the position to the slave via I2C
    }

    int act(cJSON *doc)
    {
        // do nothing
        return 0;
    }

    cJSON *get(cJSON *ob)
    {
        // do nothing
        return NULL;
    }

    MotorState pullMotorDataESPNowDriver(int axis)
    {

        MotorState motorState; // Initialize with default values
        return motorState;
    }

    void loop()
    {

        if (millis() - lastRequestTime > 5000)
        {
            lastRequestTime = millis();
            requestPosition(0); // Request position for axis 0
        }
    }

    void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
    {
        Serial.printf("ESP-NOW Send Status: %s\n", status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failure");
    }

    // Callback for receiving messages
    void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len)
    {
        uint8_t msgType = data[0];
        log_i("Received message type %i", msgType);
        if (msgType == PAIRING)
        {
            // Handle pairing message
            PairingMessage pairing;
            memcpy(&pairing, data, sizeof(PairingMessage));

            Serial.printf("Pairing request from MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                          pairing.macAddr[0], pairing.macAddr[1], pairing.macAddr[2],
                          pairing.macAddr[3], pairing.macAddr[4], pairing.macAddr[5]);

            // Assign an axis and store MAC address
            uint8_t assignedAxis = axisToMac.size(); // Assign next available axis
            memcpy(axisToMac[assignedAxis], pairing.macAddr, 6);

            // Send acknowledgment
            pairing.msgType = PAIRING;
            pairing.axis = assignedAxis;
            esp_now_send(mac_addr, (uint8_t *)&pairing, sizeof(pairing));
        }
        else if (msgType == POSITION_RESPONSE)
        {
            // Handle position response
            PositionResponse response;
            memcpy(&response, data, sizeof(PositionResponse));

            Serial.printf("Axis %d: Position = %ld, Status = %s\n",
                          response.axis, response.position, response.status);
        }
    }

    /***********************************************
     * EVERYTHIN MOTOR RELATED
     **********************************************/

    // Request position for a specific axis
    void requestPosition(uint8_t axis)
    {
        if (axisToMac.find(axis) != axisToMac.end())
        {
            PositionRequest request = {POSITION_REQUEST, axis};
            esp_now_send(axisToMac[axis], (uint8_t *)&request, sizeof(request));
            Serial.printf("Position requested for axis %d\n", axis);
        }
        else
        {
            Serial.printf("Axis %d not found\n", axis);
        }
    }
}
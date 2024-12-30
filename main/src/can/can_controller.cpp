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

    CanIsoTp isoTpSender;
    MessageData txData, rxData;
    pdu_t txPdu, rxPdu;

    // for A,X,Y,Z intialize the I2C addresses
    uint32_t CAN_IDs[] = {
        pinConfig.CAN_ID_MOT_A,
        pinConfig.CAN_ID_MOT_X,
        pinConfig.CAN_ID_MOT_Y,
        pinConfig.CAN_ID_MOT_Z};

    // Global queue for received messages
    static QueueHandle_t canQueue;

    void canReceiveTask(void *pvParameters)
    {
        pdu_t rxPdu;
        for (;;)
        {
            // Try receiving (blocks up to 100ms, depending on your driver’s API)
            if (isoTpSender.receive(&rxPdu, 50) == 0)
            {
                xQueueSend(canQueue, &rxPdu, portMAX_DELAY);
            }
            // Delay to avoid hogging the CPU, adjust as needed
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    void dispatchIsoTpData(uint32_t id, const uint8_t *data, size_t size)
    {
        switch (id)
        {

        case 0x123:
        {
            // Parse as SomeEngineData
            MessageData engine;
            if (size >= sizeof(engine))
            {
                memcpy(&engine, data, sizeof(engine));
                // Do something with engine data
                Serial.printf("Engine RPM: %u\n", engine.counter);
            }
            break;
        }
        case 0x456:
        {
            // Parse as SomeOtherData
            break;
        }
        default:
        {
            // Unknown ID
            log_i("Unknown CAN ID: %u", id);
            if (id == pinConfig.CAN_ID_CENTRAL_NODE)
            {
                // Parse as CentralNodeData
                Stepper motorAxis = static_cast<Stepper>(0);
                // based on current ID we have to parse the data
                if (getCANAddress() == pinConfig.CAN_ID_MOT_X)
                {
                    motorAxis = Stepper::X;
                }
                else if (getCANAddress() == pinConfig.CAN_ID_MOT_Y)
                {
                    motorAxis = Stepper::Y;
                }
                else if (getCANAddress() == pinConfig.CAN_ID_MOT_Z)
                {
                    motorAxis = Stepper::Z;
                }
                else if (getCANAddress() == pinConfig.CAN_ID_MOT_A)
                {
                    motorAxis = Stepper::A;
                }
                else
                {
                    log_e("Error: Unknown CAN address %u", getCANAddress());
                    return;
                }
                // Parse as MotorData
                MotorData motor;
                if (size >= sizeof(motor))
                {
                    memcpy(&motor, data, sizeof(motor));
                    // Do something with motor data
                    log_i("Motor position: %d", motor.targetPosition);
                    getData()[motorAxis]->targetPosition = motor.targetPosition;
                    FocusMotor::startStepper(motorAxis, false);
                }
                else
                {
                    log_e("Error: Incorrect data size received in CAN from address %u. Data size is %u", id, size);
                }
                break;
            }
            break;
        }
        }
    }

    uint32_t getCANAddress()
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

    void setCANAddress(uint32_t address)
    {
        log_i("Setting CAN address to %u", address);
        Preferences preferences;
        preferences.begin("CAN", false);
        preferences.putUInt("address", address);
        preferences.end();
    }

    // generic sender function
    int sendCanMessage(uint32_t receiverID, const uint8_t *data, uint8_t size)
    {
        // Send the data
        log_i("Sending data with rxID %u, txID %u, with size %u", receiverID, getCANAddress(), size);
        txPdu.data = (uint8_t *)data;
        txPdu.len = size;
        txPdu.rxId = receiverID; // maybe reverse with txId?
        txPdu.txId = getCANAddress();
        return isoTpSender.send(&txPdu);
    }

    // generic receiver function
    int receiveCanMessage(uint32_t senderID, uint8_t *data)
    {
        // Receive the data
        rxPdu.data = data;
        rxPdu.rxId = getCANAddress();
        rxPdu.txId = senderID;
        return isoTpSender.receive(&rxPdu, 100);
    }

    void setup()
    {
        // Initialize CAN bus
        if (!isoTpSender.begin(500, pinConfig.CAN_TX, pinConfig.CAN_RX))
        {
            log_e("Failed to initialize CAN bus");
            return;
        }

        log_i("CAN bus initialized with address %u", getCANAddress());

        // Create a queue to store incoming pdu_t
        canQueue = xQueueCreate(5, sizeof(pdu_t));
        // Create a dedicated task for receiving CAN messages
        xTaskCreate(canReceiveTask, "canReceiveTask", 2048, NULL, 1, NULL);
    }

    int act(cJSON *doc)
    {
        // extract the address and set it to the preferences
        // {"task":"/can_act", "address": 0x123}
        cJSON *address = cJSON_GetObjectItem(doc, "address");
        if (address != NULL)
        {
            setCANAddress(address->valueint);
            return 1;
        }

        // if we want to send a message to the motor, we can do it here
        // {"task":"/can_act", "motor": {"steppers": [{"stepperid": 1, "position": -10000, "speed": 20000, "isabs": 0.0, "isaccel": 1, "accel":20000, "isen": true}]}, "qid": 5}
        cJSON *motor = cJSON_GetObjectItem(doc, "motor");
        if (motor != NULL)
        {
            cJSON *stprs = cJSON_GetObjectItemCaseSensitive(motor, "steppers");
            cJSON *stp = NULL;
            if (stprs != NULL)
            {
                cJSON_ArrayForEach(stp, stprs)
                {
                    Stepper s = static_cast<Stepper>(cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(stp, "stepperid")));
                    getData()[s]->qid = cJsonTool::getJsonInt(doc, "qid");
                    getData()[s]->speed = cJsonTool::getJsonInt(stp, "speed");
                    getData()[s]->isEnable = cJsonTool::getJsonInt(stp, "isen");
                    getData()[s]->targetPosition = cJsonTool::getJsonInt(stp, "position");
                    getData()[s]->isforever = cJsonTool::getJsonInt(stp, "isforever");
                    getData()[s]->absolutePosition = cJsonTool::getJsonInt(stp, "isabs");
                    getData()[s]->acceleration = cJsonTool::getJsonInt(stp, "acceleration");
                    getData()[s]->isaccelerated = cJsonTool::getJsonInt(stp, "isaccel");
                    cJSON *cstop = cJSON_GetObjectItemCaseSensitive(stp, "isstop");
                    bool isStop = (cstop != NULL) ? cstop->valueint : false;
                    sendMotorDataToCANDriver(*getData()[s], s, false);
                }
            }
            else
                log_i("Motor steppers json is null");
        }
        else
            log_i("Motor json is null");
        return 0;
    }

    uint32_t axis2id(int axis)
    {
        if (axis >= 0 && axis < 4)
        {
            return CAN_IDs[axis];
        }
        return 0;
    }

    void startStepper(MotorData *data, int axis, bool reduced)
    {
        if (getData()[axis] != nullptr)
        {
            // positionsPushedToDial = false;
            getData()[axis]->isStop = false; // ensure isStop is false
            getData()[axis]->stopped = false;
            sendMotorDataToCANDriver(*getData()[axis], axis, reduced);
        }
    }

    void stopStepper(Stepper s)
    {
        // stop the motor
        getData()[s]->isStop = true;
        getData()[s]->stopped = true;
        sendMotorDataToCANDriver(*getData()[s], s, false);
    }

    void sendMotorDataToCANDriver(MotorData motorData, uint8_t axis, bool reduced)
    {
        // send motor data to slave via I2C
        uint32_t slave_addr = axis2id(axis);

        // Cast the structure to a byte array
        uint8_t *dataPtr = (uint8_t *)&motorData;
        int dataSize = sizeof(MotorData);

        int err = sendCanMessage(slave_addr, dataPtr, dataSize);

        if (err != 0)
        {
            log_e("Error sending motor data to CAN slave at address %i", slave_addr);
        }
        else
        {
            log_i("MotorData to axis: %i, at address %i, isStop: %i, speed: %i, targetPosition:%i, reduced %i, stopped %i, isaccel: %i, accel: %i, isEnable: %i, isForever %i, size %i", axis, slave_addr, motorData.isStop, motorData.speed, motorData.targetPosition, reduced, motorData.stopped, motorData.isaccelerated, motorData.acceleration, motorData.isEnable, motorData.isforever, dataSize);
        }
    }

    cJSON *get(cJSON *ob)
    {
        return ob;
    }

    void loop()
    {
        // Send a message every 1 second
        if ( millis() - lastSend >= 10)
        {
            // receive data from any node
            rxPdu.data = genericDataPtr;
            rxPdu.len = sizeof(genericDataPtr);
            rxPdu.rxId = 0;               // broadcast - listen to all ids
            rxPdu.txId = getCANAddress(); // doesn't matter, but we use the current id
            int mError = isoTpSender.receive(&rxPdu, 50);
            // int mError = receiveCanMessage(0, (uint8_t *)&genericDataPtr);

            // parse the data depending on the ID's strucutre and size
            if (mError == 0)
            {
                log_i("Sender: Received data form ID %u", rxPdu.rxId);
                dispatchIsoTpData(rxPdu.rxId, rxPdu.data, rxPdu.len);
            }
            else
            {
                log_e("Sender: No response or error");
            }
            lastSend = millis();
        }
    
    else if(false) // this does not work, consecutive frames are not received in time
    {
        static pdu_t rxPdu;
        // Non-blocking check if there's a new message
        if (xQueueReceive(canQueue, &rxPdu, 0) == pdTRUE)
        {
            // Process received data
            log_i("Sender: Received data from ID %u", rxPdu.rxId);
            dispatchIsoTpData(rxPdu.rxId, rxPdu.data, rxPdu.len);
        }
    }
    }

}
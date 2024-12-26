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
    uint8_t can_addresses[] = {
        pinConfig.I2C_ADD_MOT_A,
        pinConfig.I2C_ADD_MOT_X,
        pinConfig.I2C_ADD_MOT_Y,
        pinConfig.I2C_ADD_MOT_Z};

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
            Serial.println("Unknown message ID");
            // Possibly dump raw bytes
        }
        }
    }

    uint32_t getCANAddress()
    {
        Preferences preferences;
        preferences.begin("CAN", false);
        uint32_t address = preferences.getUInt("address", pinConfig.I2C_ADD_SLAVE);
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
    int sendCanMessage(uint32_t receiverID, const uint8_t *data)
    {
        // Send the data
        txPdu.data = (uint8_t *)data;
        txPdu.len = sizeof(data);
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
        return isoTpSender.receive(&rxPdu);
    }

    void setup()
    {
        // Initialize CAN bus
        if (!isoTpSender.begin(500, pinConfig.CAN_TX, pinConfig.CAN_RX))
        {
            log_e("Failed to initialize CAN bus");
            return;
        }



        // Initialize data
        txData.counter = 0;

        // Setup Tx PDU
        txPdu.txId = 0x123;
        txPdu.rxId = 0x456;
        txPdu.data = (uint8_t *)&txData;
        txPdu.len = sizeof(txData);
        txPdu.cantpState = CANTP_IDLE;
        txPdu.blockSize = 0;
        txPdu.separationTimeMin = 5;

        // Setup Rx PDU for responses
        rxPdu.txId = 0x456;   // Receiver's ID
        rxPdu.rxId = 0;       // broadcast - listen to all ids; 0x123; // Sender's ID
        rxPdu.data = nullptr; // nullptr indicates that we will parse the data later; size will be dicated by FC frame (uint8_t *)&rxData;
        rxPdu.len = 0;        //       sizeof(rxData);
        rxPdu.cantpState = CANTP_IDLE;
        rxPdu.blockSize = 0;
        rxPdu.separationTimeMin = 0;
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
        
    int axis2id(int axis)
    {
        if (axis >= 0 && axis < 4)
        {
            return can_addresses[axis];
        }
        return 0;
    }

    void startStepper(MotorData *data, int axis, bool reduced)
    {
        if (getData()[axis] != nullptr)
        {
            //positionsPushedToDial = false;
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
        uint8_t slave_addr = axis2id(axis);
        
        // Cast the structure to a byte array
        uint8_t *dataPtr = (uint8_t *)&motorData;
        int dataSize = sizeof(MotorData);
        
        int err = sendCanMessage(slave_addr, dataPtr);

        if (err != 0)
        {
            log_e("Error sending motor data to CAN slave at address %i", slave_addr);
        }
        else{
            log_i("MotorData to axis: %i, at address %i, isStop: %i, speed: %i, targetPosition:%i, reduced %i, stopped %i, isaccel: %i, accel: %i, isEnable: %i, isForever %i", axis, slave_addr, motorData.isStop, motorData.speed, motorData.targetPosition, reduced, motorData.stopped, motorData.isaccelerated, motorData.acceleration, motorData.isEnable, motorData.isforever);
        }
    }

    cJSON *get(cJSON *ob)
    {
        return ob;
    }

    void loop()
    {
        // Send a message every 1 second
        if (millis() - lastSend >= 10)
        {
            lastSend = millis();
            txData.counter++;
            txPdu.data = (uint8_t *)&txData;
            txPdu.len = sizeof(txData);
            if (isoTpSender.send(&txPdu) == 0)
            {
                Serial.print("Sender: Sent counter = ");
                Serial.println(txData.counter);
            }
            else
            {
                Serial.println("Sender: Error sending");
            }
        }

        // receive data from receiver always
        int result = isoTpSender.receive(&rxPdu);
        if (result == 0 && rxPdu.cantpState == CANTP_END)
        {
            /*
            Serial.print("Sender: Received response counter = ");
            Serial.println(rxData.counter);
            Serial.print("Sender ID: ");
            Serial.println(rxPdu.rxId);
                // Dispatch by ID
                */
            dispatchIsoTpData(rxPdu.rxId, rxPdu.data, rxPdu.len);

            // Free the data once done
            // free(rxPdu->data); // will be done inside the receive function
        }
        else
        {
            Serial.print("Sender: No response or error");
        }
    }
}
#include "espnow_slave_motor.h"
#include "Wire.h"
#include "PinConfig.h"
#include "../motor/FocusMotor.h"
#include "../home/HomeMotor.h"
#include "espnow_master.h"

using namespace FocusMotor;
using namespace HomeMotor;
using namespace espnow_master;

namespace espnow_slave_motor
{

    void parseMotorEvent(int numBytes)
    {
    }

    void receiveEvent(int numBytes)
    {
    }

    void requestEvent()
    {
    }

    
    void setup()
    {
        WiFi.mode(WIFI_STA);

        // Initialize ESP-NOW
        if (esp_now_init() != ESP_OK)
        {
            log_i("Error initializing ESP-NOW");
            return;
        }

        // Register receive callback
        esp_now_register_recv_cb(onDataRecv);

        // Send pairing request
        sendPairingRequest();
    }

    void loop()
    {
    }

    /************************
     * ESP-NOW Callbacks
     * **********************/

    // Callback for receiving messages
    void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len)
    {
        uint8_t msgType = data[0];

        if (msgType == PAIRING)
        {
            // Handle pairing acknowledgment
            PairingMessage pairing;
            memcpy(&pairing, data, sizeof(PairingMessage));
            slaveAxis = pairing.axis;
            Serial.printf("Paired with axis %d\n", slaveAxis);
        }
        else if (msgType == POSITION_REQUEST)
        {
            // Handle position request
            PositionRequest request;
            memcpy(&request, data, sizeof(PositionRequest));

            if (request.axis == slaveAxis)
            {
                // Respond with motor position
                PositionResponse response = {POSITION_RESPONSE, slaveAxis, motorPosition, "STOPPED"};
                esp_now_send(mac_addr, (uint8_t *)&response, sizeof(response));
                Serial.printf("Position response sent for axis %d\n", slaveAxis);
            }
        }
    }

    // Send pairing request
    void sendPairingRequest()
    {
        sleep(2);
        PairingMessage pairing = {PAIRING, 0xFF};
        WiFi.macAddress(pairing.macAddr);
        esp_now_send(broadcastAddress, (uint8_t *)&pairing, sizeof(pairing));
        log_i("Pairing request sent");
    }

}
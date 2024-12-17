#pragma once

#include <esp_now.h>
#include <WiFi.h>



namespace espnow_slave_motor
{

    // Slave variables
    static uint8_t slaveAxis = 0xFF; // Axis not assigned yet
    static long motorPosition = 0;
    uint8_t clientMacAddress[6];

    void setup();
    void loop();
    void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len);
    void sendPairingRequest();


};

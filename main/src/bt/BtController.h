#pragma once

#include <BluetoothSerial.h>
#include "esp_err.h"
#include <ArduinoJson.h>
#include "../../config.h"
#include "../wifi/RestApiCallbacks.h"

namespace RestApi
{
    /*
    returns an array that contains the visible bt devices
        endpoint:/bt_scan
        input[]
        output
        [
            {
                "name" :"HyperX",
                "mac": "01:02:03:04:05:06"
            }
            ,
            {
                "name": "",
                "mac": "01:02:03:04:05:06"
            },
        ]
    */
    void Bt_startScan();
    void Bt_connect();
    void Bt_getPairedDevices();
    void Bt_remove();
};

namespace BtController
{
    void setup();
    void scanForDevices(DynamicJsonDocument * jdoc);
    
    void setMacAndConnect(String m);
    void removePairedDevice(String pairedmac);
    void getPairedDevices(DynamicJsonDocument *jdoc);
    char * bda2str(const uint8_t *bda, char *str, size_t size);
    bool connectToServer();
    
}
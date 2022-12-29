#pragma once

#include <BluetoothSerial.h>
#include "esp_err.h"
#include <ArduinoJson.h>
#include "../wifi/RestApiCallbacks.h"
#include "PsXController.h"
#include "../../PinConfig.h"

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

namespace BtController //class BtController : public Module
{
    //public:
    void setup();// override;
    void loop();// override;
    DynamicJsonDocument scanForDevices(DynamicJsonDocument  doc);
    
    void setMacAndConnect(String m);
    void connectPsxController(String mac, int type);
    void removePairedDevice(String pairedmac);
    DynamicJsonDocument getPairedDevices(DynamicJsonDocument doc);
    char * bda2str(const uint8_t *bda, char *str, size_t size);
    bool connectToServer();
    
    
}
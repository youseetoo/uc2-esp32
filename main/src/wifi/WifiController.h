#include "../../config.h"
#ifdef IS_WIFI
#pragma once

#include "WiFi.h"
#include "HardwareSerial.h"
#include "parameters_wifi.h"
#include "../state/State.h"
#include "RestApiCallbacks.h"
#include "esp_log.h"
#include "SPIFFS.h"

#ifdef IS_LASER
#include "../laser/LaserController.h"
#endif
#if defined IS_DAC || defined IS_DAC_FAKE
#include "../dac/DacController.h"
#endif
#ifdef IS_ANALOG
#include "../analog/AnalogController.h"
#endif
#ifdef IS_DIGITAL
#include "../digital/DigitalController.h"
#endif
#include "../config/ConfigController.h"
#if defined IS_DAC || defined IS_DAC_FAKE
    #include "../dac/DacController.h"
#endif
#include <WebSocketsServer.h>

namespace RestApi
{
    /*
        start a wifiscan and return the results
        endpoint:/wifi/scan
        input []
        output
        [
            "ssid1",
            "ssid2",
            ....
        ]
    */
    void scanWifi();
    /*
        connect to a wifi network or create ap
        endpoint:/wifi/connect
        input
        [
            "ssid": "networkid"
            "PW" : "password"
            "AP" : 0
        ]
        output[]
    */
    void connectToWifi();
};

namespace WifiController
{
    
    void createAp(String ssid, String password);
    
    /* data */

    
    
    void setup_routing();
    void handelMessages();
    void createJsonDoc();
    void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
    void sendJsonWebSocketMsg();
    
    //Wifi
    
    void setWifiConfig(String mSSID,String mPWD,bool ap);
    String getSsid();
    String getPw();
    bool getAp();
    void setup();
    DynamicJsonDocument * getJDoc();
    WebServer * getServer();
    void getIndexPage();
    void getCSS();
    void getjquery();
    void getjs();
}
#endif

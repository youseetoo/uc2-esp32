#include "../../config.h"
#pragma once

#include "WiFi.h"
#include "HardwareSerial.h"
#include "../state/State.h"
#include "RestApiCallbacks.h"
#include "esp_log.h"
#include "SPIFFS.h"
#include "../analog/AnalogController.h"
#include "../digitalout/DigitalOutController.h"
#include "../config/ConfigController.h"
#include "../dac/DacController.h"
#include "../pid/PidController.h"
#include "../laser/LaserController.h"
#include "../led/LedController.h"
#include <WebSocketsServer.h>
#include "WifiConfig.h"

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
    void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
    void sendJsonWebSocketMsg();
    void begin();
    void restartWebServer();

    // Wifi

    void setWifiConfig(String SSID, String PWD, bool ap);
    String getSsid();
    String getPw();
    bool getAp();
    void setup();
    DynamicJsonDocument *getJDoc();
    WebServer *getServer();
    void getIndexPage();
    void getCSS();
    void getjquery();
    void getjs();
    void connect();
}

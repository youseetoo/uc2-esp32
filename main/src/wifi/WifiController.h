#pragma once

#include "WiFi.h"
#include "HardwareSerial.h"
#include "RestApiCallbacks.h"
#include "esp_log.h"
#include "SPIFFS.h"
#include "../analogin/AnalogInController.h"
#include "../digitalout/DigitalOutController.h"
#include "../digitalin/DigitalInController.h"
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
    DynamicJsonDocument scanWifi();
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

    void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
    void getIndexPage();
    void getCSS();
    void getjquery();
    void getjs();
    void getOtaIndex();
};

void processWebSocketTask(void * p);
void processHttpTask(void * p);



class WifiController : public Module
{
    private:
    WebServer *server = nullptr;
	WebSocketsServer *webSocket = nullptr;
	WifiConfig *config;
    TaskHandle_t httpTaskHandle;
    TaskHandle_t socketTaskHandle;
	
    public:
    WifiController();
    ~WifiController();
    void createAp(String ssid, String password);
    void createTasks();

    /* data */

    void setup_routing();
    
    void sendJsonWebSocketMsg(DynamicJsonDocument doc);
    void begin();
    void restartWebServer();

    // Wifi

    void setWifiConfig(String SSID, String PWD, bool ap);
    String getSsid();
    String getPw();
    bool getAp();
    void setup() override;
    void loop() override;
    int act(DynamicJsonDocument doc) override;
    DynamicJsonDocument get(DynamicJsonDocument doc) override;
    WebServer *getServer();
    WebSocketsServer * getSocket();
    
    DynamicJsonDocument connect(DynamicJsonDocument doc);
};

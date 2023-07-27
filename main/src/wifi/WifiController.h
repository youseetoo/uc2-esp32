#pragma once

#include "EspHttpsServer.h"
#include "RestApiCallbacks.h"
#include "esp_log.h"
#include "Module.h"
#include "WifiConfig.h"
#include "EspWifiController.h"



class WifiController : public Module
{
    private:
	WifiConfig *config;
    EspWifiController espWifiController;
    EspHttpsServer httpsServer;

	
    public:
    WifiController();
    ~WifiController();

    /* data */

    void setup_routing();
    
    void sendJsonWebSocketMsg(DynamicJsonDocument doc);
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
    DynamicJsonDocument connect(DynamicJsonDocument doc);
    DynamicJsonDocument scan();
};

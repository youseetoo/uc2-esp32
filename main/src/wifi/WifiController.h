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
    
    void sendJsonWebSocketMsg(cJSON * doc);
    void restartWebServer();

    // Wifi

    void setWifiConfig(char* SSID, char* PWD, bool ap);
    char* getSsid();
    char* getPw();
    bool getAp();
    void setup() override;
    void loop() override;
    int act(cJSON * doc) override;
    cJSON * get(cJSON * doc) override;
    cJSON * connect(cJSON * doc);
    cJSON * scan();
};

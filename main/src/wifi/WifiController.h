#include <PinConfig.h>"
#ifdef WIFI
#pragma once

#include "cJSON.h"

namespace WifiController
{
	

    /* data */

    void setup_routing();
    
    void sendJsonWebSocketMsg(cJSON * doc);
    void restartWebServer();

    // Wifi

    void setWifiConfig(char* SSID, char* PWD, bool ap);
    char* getSsid();
    char* getPw();
    bool getAp();
    void setup();
    int act(cJSON * doc);
    cJSON * get(cJSON * doc);
    cJSON * connect(cJSON * doc);
    cJSON * scan();
};
#endif

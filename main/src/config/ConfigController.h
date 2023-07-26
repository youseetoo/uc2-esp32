#pragma once

#include "ArduinoJson.h"
#include "../wifi/WifiConfig.h"

namespace Config
{
    void setup();
    bool resetPreferences();
    void setWifiConfig(WifiConfig * config);
    WifiConfig * getWifiConfig();
    void setPsxMac(String mac);
    String getPsxMac();
    void setPsxControllerType(int type);
    int getPsxControllerType();
}

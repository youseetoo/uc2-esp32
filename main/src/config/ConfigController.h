#pragma once

#include "../wifi/WifiConfig.h"

namespace Config
{
    void setup();
    bool resetPreferences();
    void setWifiConfig(WifiConfig * config);
    WifiConfig * getWifiConfig();
    void setPsxMac(char* mac);
    char* getPsxMac();
    void setPsxControllerType(int type);
    int getPsxControllerType();
}

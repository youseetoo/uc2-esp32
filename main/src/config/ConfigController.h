#pragma once
#include "Preferences.h"
#include "ArduinoJson.h"
#include "JsonKeys.h"
#include "../wifi/WifiConfig.h"
#include "esp_log.h"
#include "../motor/FocusMotor.h"

namespace Config
{
    void setup();
    bool resetPreferences();
    bool isFirstRun();
    void setWifiConfig(WifiConfig * config);
    WifiConfig * getWifiConfig();
    void setPsxMac(String mac);
    String getPsxMac();
    void setPsxControllerType(int type);
    int getPsxControllerType();
}

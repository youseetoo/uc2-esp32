#pragma once
#include "Preferences.h"
#include "ArduinoJson.h"
#include "JsonKeys.h"
#include "../../pinstruct.h"
#include "../wifi/WifiController.h"
#include "esp_log.h"
#include "../motor/MotorPins.h"
#include "../motor/FocusMotor.h"
#include "../led/LedConfig.h"

namespace RestApi
{
    void Config_act();
    void Config_get();
    void Config_set();
};

namespace Config
{

    void setJsonToPref(const char *key);
    void setPinsToJson(const char *key, int val);
    void setup(PINDEF *pins);
    bool resetPreferences();
    bool setPreferences();
    bool getPreferences();
    void initempty();
    void savePreferencesFromPins(bool openPrefs);
    void applyPreferencesToPins();
    void loop();
    void act();
    void set();
    void get();
    bool isFirstRun();
    void setWifiConfig(String ssid, String pw, bool ap, bool prefopen);
    void setMotorPinConfig(bool prefsOpen, std::array<MotorPins, 4> pins);
    void getMotorPins(std::array<MotorPins, 4> pins);
    void setLedPins(bool prefsOpen, LedConfig config);
    void getLedPins(LedConfig config);
}

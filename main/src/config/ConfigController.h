#pragma once
#include "Preferences.h"
#include "ArduinoJson.h"
#include "JsonKeys.h"
#include "../wifi/WifiConfig.h"
#include "esp_log.h"
#include "../motor/MotorPins.h"
#include "../motor/FocusMotor.h"
#include "../led/LedConfig.h"
#include "../laser/LaserPins.h"
#include "../dac/DacPins.h"
#include "../analog/AnalogPins.h"
#include "../digital/DigitalPins.h"
#include "../../ModuleConfig.h"

namespace Config
{
    void setup();
    bool resetPreferences();
    bool isFirstRun();
    void setWifiConfig(WifiConfig config, bool prefopen);
    void getWifiConfig(WifiConfig config);
    void setMotorPinConfig(bool prefsOpen, std::array<MotorPins, 4> pins);
    void getMotorPins(std::array<MotorPins, 4> pins);
    void setLedPins(bool prefsOpen, LedConfig config);
    void getLedPins(LedConfig config);
    void setLaserPins(bool openPrefs,LaserPins pins);
    void getLaserPins(LaserPins pin);
    void setDacPins(bool openPrefs,DacPins pins);
    void getDacPins(DacPins pin);
    void setAnalogPins(bool openPrefs,AnalogPins pins);
    void getAnalogPins(AnalogPins pin);
    void setDigitalPins(bool openPrefs,DigitalPins pins);
    void getDigitalPins(DigitalPins pin);
    void setModuleConfig(bool openPrefs,ModuleConfig pins);
    void getModuleConfig(ModuleConfig pin);
}

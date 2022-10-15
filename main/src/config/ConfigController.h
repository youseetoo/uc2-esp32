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
    void setWifiConfig(WifiConfig * config);
    WifiConfig * getWifiConfig();
    void setMotorPinConfig(MotorPins * pins[]);
    void getMotorPins(MotorPins * pins[]);
    void setLedPins(LedConfig * config);
    LedConfig * getLedPins();
    void setLaserPins(LaserPins pins);
    void getLaserPins(LaserPins pin);
    void setDacPins(DacPins pins);
    void getDacPins(DacPins pin);
    void setAnalogPins(AnalogPins pins);
    void getAnalogPins(AnalogPins pin);
    void setDigitalPins(DigitalPins pins);
    void getDigitalPins(DigitalPins pin);
    void setModuleConfig(ModuleConfig * pins);
    ModuleConfig * getModuleConfig();
}

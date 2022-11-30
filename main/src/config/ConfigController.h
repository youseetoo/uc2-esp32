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
#include "../analogin/AnalogInPins.h"
#include "../analogout/AnalogOutPins.h"
#include "../analogin/JoystickPins.h"
#include "../digitalout/DigitalOutPins.h"
#include "../digitalin/DigitalInPins.h"
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
    void setAnalogInPins(AnalogInPins pins);
    void getAnalogInPins(AnalogInPins pin);
    void setDigitalOutPins(DigitalOutPins pins);
    void getDigitalOutPins(DigitalOutPins pins);
    void setDigitalInPins(DigitalInPins pins);
    void getDigitalInPins(DigitalInPins pins);    
    void getAnalogOutPins(AnalogOutPins pin);
    void setAnalogOutPins(AnalogOutPins pins);
    void setModuleConfig(ModuleConfig * pins);
    ModuleConfig * getModuleConfig();
    void setAnalogJoyStickPins(JoystickPins * pins);
    void getAnalogJoyStickPins(JoystickPins * pins);
    void setPsxMac(String mac);
    String getPsxMac();
    void setPsxControllerType(int type);
    int getPsxControllerType();
}

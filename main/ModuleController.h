#pragma once
#include "config.h"
#include <map>
#include "Module.h"
#include "ModuleConfig.h"
#include "src/led/LedController.h"
#include "src/motor/FocusMotor.h"
#include "src/home/HomeMotor.h"
#include "src/laser/LaserController.h"
#include "src/slm/SlmController.h"
#include "src/pid/PidController.h"
#include "src/analogin/AnalogInController.h"
#include "src/analogin/AnalogJoystick.h"
#include "src/dac/DacController.h"
#include "src/analogout/AnalogOutController.h"
#include "src/digitalout/DigitalOutController.h"
#include "src/digitalin/DigitalInController.h"
#include "src/scanner/ScannerController.h"

namespace RestApi
{
    void getModules();
    void setModules();
};

enum class AvailableModules
{
    analogout,
    dac,
    digitalout,
    digitalin,
    laser,
    led,
    motor,
    home,
    pid,
    scanner,
    analogin,
    slm,
    analogJoystick,
};

class ModuleController
{
private:
    std::map<AvailableModules, Module *> modules;
   
public:
    ModuleConfig * moduleConfig;
    void setup();
    void loop();
    Module *get(AvailableModules mod);
    void get();
    void set(JsonObject j);
};

extern ModuleController moduleController;
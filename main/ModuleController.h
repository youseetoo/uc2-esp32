#pragma once
#include <map>
#include "Module.h"
#include "PinConfig.h"
#include "src/led/LedController.h"
#include "src/motor/FocusMotor.h"
#include "src/rotator/Rotator.h"
#include "src/config/ConfigController.h"
#include "src/home/HomeMotor.h"
#include "src/laser/LaserController.h"
#include "src/pid/PidController.h"
#include "src/analogin/AnalogInController.h"
#include "src/analogin/AnalogJoystick.h"
#include "src/dac/DacController.h"
#include "src/analogout/AnalogOutController.h"
#include "src/digitalout/DigitalOutController.h"
#include "src/digitalin/DigitalInController.h"
#include "src/state/State.h"
#include "src/bt/BtController.h"
#include "src/scanner/ScannerController.h"

namespace RestApi
{
    void getModules();
};

enum class AvailableModules
{
    analogout,
    btcontroller,
    config,
    dac,
    digitalout,
    digitalin,
    laser,
    led,
    motor,
    rotator,
    home,
    pid,
    scanner,
    analogin,
    state,
    analogJoystick,
    wifi
};

class ModuleController
{
private:
    std::map<AvailableModules, Module *> modules;
   
public:
    void setup();
    void loop();
    Module *get(AvailableModules mod);
    DynamicJsonDocument get();
};

extern "C" ModuleController moduleController;
#pragma once
#include "config.h"
#include <map>
#include "Module.h"
#include "ModuleConfig.h"
#include "src/led/LedController.h"
#include "src/motor/FocusMotor.h"
#include "src/laser/LaserController.h"
#include "src/slm/SlmController.h"
#include "src/pid/PidController.h"
#include "src/sensor/SensorController.h"
#include "src/dac/DacController.h"
#include "src/analog/AnalogController.h"
#include "src/digital/DigitalController.h"
#include "src/scanner/ScannerController.h"

enum class AvailableModules
{
    analog,
    dac,
    digital,
    laser,
    led,
    motor,
    pid,
    scanner,
    sensor,
    slm,
};

class ModuleController
{
private:
    std::map<AvailableModules, Module *> modules;
    ModuleConfig * moduleConfig;

public:
    void setup();
    void loop();
    Module *get(AvailableModules mod);
    void get();
    void set();
};

extern ModuleController moduleController;
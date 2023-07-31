#pragma once
#include <map>
#include "Module.h"
#include "cJSON.h"

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
    cJSON * get();
};

extern ModuleController moduleController;
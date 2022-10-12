#pragma once
#include "config.h"
#include <map>
#include "Module.h"

#ifdef IS_LED
#include "src/led/LedController.h"
#endif
#ifdef IS_MOTOR
#include "src/motor/FocusMotor.h"
#endif
#ifdef IS_LASER
#include "src/laser/LaserController.h"
#endif
#ifdef IS_SLM
#include "src/slm/SlmController.h"
#endif
#ifdef IS_PID
#include "src/pid/PidController.h"
#endif
#ifdef IS_READSENSOR
#include "src/sensor/SensorController.h"
#endif

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

public:
    void setup();
    void loop();
    Module *get(AvailableModules mod);
};

extern ModuleController moduleController;
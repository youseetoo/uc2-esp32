#pragma once

struct ModuleConfig
{
    bool analogout = false;
    bool dac = false;
    bool digitalout = true;
    bool digitalin = true;
    bool laser = true;
    bool led = true;
    bool config = true;
    bool motor = true;
    bool pid = false;
    bool scanner = false;
    bool analogin = false;
    bool btcontroller = true;
    bool home = true;
    bool state = true;
    bool analogJoystick = false;
    bool wifi = false;
};

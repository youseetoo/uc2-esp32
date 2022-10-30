#pragma once
#include <Arduino.h>
#include "../../Module.h"
#include "../../ModuleController.h"
#include "../config/ConfigController.h"
#include "JoystickPins.h"

namespace RestApi
{
	void AnalogJoystick_set();
	void AnalogJoystick_get();
};



class AnalogJoystick : public Module
{

    private:
    JoystickPins * pins;
    bool joystick_drive_X = false;
    bool joystick_drive_Y = false;

    public:
    AnalogJoystick();
	virtual ~AnalogJoystick();
    void setup() override;
    void act(JsonObject  jsonDocument) override;
    void set(JsonObject  jsonDocument) override;
    void get(JsonObject  jsonDocument) override;
    void loop() override;

};
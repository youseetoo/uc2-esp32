#pragma once
#include <Arduino.h>
#include "../../Module.h"
#include "../../ModuleController.h"
#include "../config/ConfigController.h"
#include "../../PinConfig.h"

namespace RestApi
{
	void AnalogJoystick_get();
};


void processLoopAJoy(void *param);

class AnalogJoystick : public Module
{

    private:
    int joystick_drive_X = 0;
    int joystick_drive_Y = 0;
    int max_in_value = 4096;
    int zeropoint = 350;

    public:
    AnalogJoystick();
	virtual ~AnalogJoystick();
    void setup() override;
    int act(DynamicJsonDocument  jsonDocument) override;
    DynamicJsonDocument get(DynamicJsonDocument  jsonDocument) override;
    void loop() override;

};
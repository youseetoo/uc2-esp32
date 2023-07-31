#pragma once
#include "../../Module.h"


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
    int act(cJSON*  jsonDocument) override;
    cJSON* get(cJSON*  jsonDocument) override;
    void loop() override;

};
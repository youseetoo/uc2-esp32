#include "../../config.h"
#pragma once
#include "../../config.h"
#include "AccelStepper.h"
#include "ArduinoJson.h"
#include "../digitalin/DigitalInController.h"
#if defined IS_PS3 || defined IS_PS4
#include "../gamepads/ps_3_4_controller.h"
#endif
#include "../wifi/WifiController.h"
#include "../config/ConfigController.h"
#include "../../ModuleController.h"
#include "../digitalin/DigitalInPins.h" 





class HomeMotor : public Module
{

public:
	HomeMotor();
	~HomeMotor();
	bool DEBUG = true;

	void act() override;
	void set() override;
	void get() override;
	void setup() override;
	void loop() override;

private:
};

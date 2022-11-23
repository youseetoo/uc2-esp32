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



namespace RestApi
{
	void HomeMotor_act();
	void HomeMotor_get();
	void HomeMotor_set();
};

struct HomeData
{
	int homeEndposPin = 0;
	long homeTimeout = 10000; // ms
	long homeSpeed = 0;
	long homeMaxspeed = 20000;
	int homeDirection = 1;
	long homeTimeStarted = 0;
	bool homeIsActive = false;
	int homeEndposRelease = 1000;
};

class HomeMotor : public Module
{

public:
	HomeMotor();
	~HomeMotor();
	bool DEBUG = true;
	int homeEndposRelease = 2000;
	bool isHoming = false;
	std::array<HomeData *, 4> hdata;

	void act(JsonObject  ob) override;
	void set(JsonObject ob) override;
	void get(JsonObject ob) override;
	void setup() override;
	void loop() override;

private:
};

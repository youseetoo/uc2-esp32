#include "../../config.h"
#pragma once
#include "../../config.h"
#include "AccelStepper.h"
#include "ArduinoJson.h"
#if defined IS_PS3 || defined IS_PS4
#include "../gamepads/ps_3_4_controller.h"
#endif
#include "../wifi/WifiController.h"
#include "../config/ConfigController.h"
#include "../../ModuleController.h"

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
};



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

	std::array<HomeData *, 4> hdata;

private:
	int logcount;
	void doHome(int i);
};

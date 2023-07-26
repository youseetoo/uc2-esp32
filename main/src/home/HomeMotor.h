#pragma once

#include "../../ModuleController.h"

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
	bool homeEndStopPolarity = 0; // normally open
};

void processHomeLoop(void * p);

class HomeMotor : public Module
{

public:
	HomeMotor();
	~HomeMotor();
	bool DEBUG = true;
	int homeEndposRelease = 2000;
	bool isHoming = false;
	std::array<HomeData *, 4> hdata;

	int act(DynamicJsonDocument  ob) override;
	DynamicJsonDocument get(DynamicJsonDocument ob) override;
	void setup() override;
	void loop() override;

private:
};

#include <PinConfig.h>
#pragma once
#include "../motor/FocusMotor.h"
#include "cJSON.h"


struct HomeData
{
	int homeEndposPin = 0;
	long homeTimeout = 10000; // ms
	int homeSpeed = 10000;
	int homeMaxspeed = 20000;
	int homeDirection = 1;
	long homeTimeStarted = 0;
	bool homeIsActive = false;
	int homeEndposRelease = 1000;
	int homeInEndposReleaseMode = 0;
	bool homeEndStopPolarity = 0; // normally open
	int qid = -1;
};

void processHomeLoop(void * p);

namespace HomeMotor
{

	static bool DEBUG = true;
	static int homeEndposRelease = 2000;
	static bool isHoming = false;
	static std::array<HomeData *, 4> hdata;

	int act(cJSON * ob);
	cJSON * get(cJSON * ob);
	void setup();
	void loop();
    void checkAndProcessHome(Stepper s, int digitalin_val);
	int parseHomeData(cJSON *doc);
	void runStepper(int s);
	void startHome(int axis, int homeTimeout, int homeSpeed, int homeMaxspeed, int homeDirection, int homeEndStopPolarity);
};

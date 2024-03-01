#pragma once

#include "../../ModuleController.h"
#include "../motor/FocusMotor.h"

struct HomeData
{
	int homeEndposPin = 0;
	long homeTimeout = 10000; // ms
	int homeSpeed = 10000;
	int homeMaxspeed = 20000;
	int homeDirection = 1;
	long homeTimeStarted = 0;
	bool homeIsActive = false;
	int homeInEndposReleaseMode = 0;
	bool homeEndStopPolarity = 0; // normally open
	int qid = -1; // qeue id
};

void processHomeLoop(void * p);

class HomeMotor : public Module
{

public:
	HomeMotor();
	~HomeMotor();
	bool DEBUG = true;
	bool isHoming = false;
	std::array<HomeData *, 4> hdata;

	int act(cJSON * ob) override;
	cJSON * get(cJSON * ob) override;
	void setup() override;
	void loop() override;
	void sendHomeDone(int axis);
private:
	void checkAndProcessHome(Stepper s, int digitalin_val,FocusMotor *motor);

};

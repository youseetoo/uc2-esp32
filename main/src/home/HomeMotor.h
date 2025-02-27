#include <PinConfig.h>
#pragma once
#include "../motor/FocusMotor.h"
#include "cJSON.h"

#pragma pack(push,1)
struct HomeData
{
	int homeEndposPin = 0;
	uint32_t homeTimeout = 10000; // ms
	int homeSpeed = 10000;
	int homeMaxspeed = 20000;
	int homeDirection = 1;
	uint32_t homeTimeStarted = 0;
	bool homeIsActive = false;
	int homeEndposRelease = 1000;
	int homeInEndposReleaseMode = 0;
	bool homeEndStopPolarity = 0; // normally open
	int qid = -1;
};
#pragma pack(pop)


#pragma pack(push,1)
struct HomeState
{
	bool isHoming = false;
	bool isHomed = false;
	int homeInEndposReleaseMode = 0;
	uint32_t currentPosition = 0;
	int axis = 0;
};
#pragma pack(pop)

void processHomeLoop(void * p);

#pragma pack(push,1)
namespace HomeMotor
{

	static bool isDEBUG = true;
	static int homeEndposRelease = 2000;
	static bool isHoming = false;
	static bool isDualAxisZ = false;

	int act(cJSON * ob);
	cJSON * get(cJSON * ob);
	void setup();
	void loop();
    void checkAndProcessHome(Stepper s, int digitalin_val);
	int parseHomeData(cJSON *doc);
	void runStepper(int s);
	void startHome(int axis, int homeTimeout, int homeSpeed, int homeMaxspeed, int homeDirection, int homeEndStopPolarity, int qid, bool isDualAxisZ);
	HomeData** getHomeData();
	void sendHomeDone(int axis);
};
#pragma pack(pop)
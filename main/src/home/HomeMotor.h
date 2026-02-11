#include <PinConfig.h>
#pragma once
#include "../motor/FocusMotor.h"
#include "cJSON.h"

#include "Preferences.h"


#pragma pack(push,1)
struct HomeData
{
	uint8_t homeEndposPin = 0;
	uint32_t homeTimeout = 10000; // ms
	int32_t homeSpeed = 10000;
	uint32_t homeMaxspeed = 20000;
	int8_t homeDirection = 1;
	uint32_t homeTimeStarted = 0;
	bool homeIsActive = false;
	uint8_t homeInEndposReleaseMode = 0;  // Legacy field, kept for compatibility
	bool homeEndStopPolarity = 0; // normally open
	uint16_t qid = -1;
	bool precise = false; // Use encoder-based stall detection for homing
	
	// New CNC-style homing fields
	uint homingPhase = 0;  // 0=idle, 1=fast to endstop, 2=retract, 3=slow to endstop, 4=done
	uint16_t homeRetractDistance = 2000;  // Steps to retract after hitting endstop
	int32_t homeFirstHitPosition = 0;  // Position where endstop was first triggered
	int32_t homeEndOffset = 0;  // Final position offset from home (can be positive or negative)
	TaskHandle_t homingTaskHandle = nullptr;  // FreeRTOS task handle
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

#pragma pack(push,1)
struct StopHomeCommand
{
	uint8_t axis = 0;  // Axis to stop homing on
};
#pragma pack(pop)

void processHomeLoop(void * p);
void homingTaskFunction(void *parameter);

#pragma pack(push,1)
namespace HomeMotor
{

	// define preferences
	static Preferences preferences;

	static bool isDEBUG = true;
	static bool isHoming = false;
	
	// Task handles for each axis homing operation
	static TaskHandle_t homingTaskHandles[4] = {nullptr, nullptr, nullptr, nullptr};

	int act(cJSON * ob);
	cJSON * get(cJSON * ob);
	void setup();
	void loop();
    void checkAndProcessHome(Stepper s, int digitalin_val);
	int parseHomeData(cJSON *doc);
	void runStepper(int s);
	void startHome(int axis, int homeTimeout, int homeSpeed, int homeMaxspeed, int homeDirection, int homeEndStopPolarity, int homeEndOffset, int qid);
	void stopHome(int axis);
	HomeData** getHomeData();
	void sendHomeDone(int axis);
};
#pragma pack(pop)
#pragma once


const long MAX_VELOCITY_A = 40000;
const long MAX_ACCELERATION_A = 100000;
const long DEFAULT_ACCELERATION = 100000;

struct StageScanningData{
	int nStepsLine = 100;
	int dStepsLine = 1;
	int nTriggerLine = 1;
	int nStepsPixel = 100;
	int dStepsPixel = 1;
	int nTriggerPixel = 1;
	int delayTimeStep = 10;
	int stopped = 0;
	int nFrames = 1;
};

struct MotorData
{

	long speed = 0;
	long maxspeed = MAX_VELOCITY_A;
	long acceleration = 0;
	long targetPosition = 0;
	long currentPosition = 0;
	bool isforever = false;
	bool isaccelerated = 1;
	bool absolutePosition = 0; 	// running relative or aboslute position?
	bool stopped = true;
	// milliseconds to switch off motors after operation
	int timeoutDisable = 1000;
	int timeLastActive = 0;
	bool isEnable = 1; // keeping motor on after job is completed?

	bool wasRunning = false; // flag to check if the motor was running in the last loop
	bool isActivated = 0;//flag to check if the motor is functional or just there to allocate memory :P
	int qid = -1; // for keeping sync with commands sent by the serial and their responses

	// for triggering frame or lineclock
	bool isTriggered = false; // state if we send a pulse
	long offsetTrigger = 0;	// offset in steps
	long triggerPeriod = -1; // give a pulse every n steps
	int triggerPin = -1;	 // pin to trigger (0,1,2 - depends on pinConfig)
	int dirPin = -1;
	int stpPin = -1;
};

enum Stepper
{
	A,
	X,
	Y,
	Z
};

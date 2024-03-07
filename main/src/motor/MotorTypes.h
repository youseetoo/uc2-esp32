#pragma once


const long MAX_VELOCITY_A = 20000;
const long MAX_ACCELERATION_A = 100000;
const long DEFAULT_ACCELERATION = 500000;


struct MotorData
{

	long speed = 0;
	long maxspeed = 200000;
	long acceleration = 0;
	long targetPosition = 0;
	long currentPosition = 0;
	int isforever = false;
	bool isaccelerated = 0;
	// running relative or aboslute position! gets ignored when isforever is true
	bool absolutePosition = 0; 	
	bool stopped = true;
	// milliseconds to switch off motors after operation
	int timeoutDisable = 1000;
	int timeLastActive = 0;
	bool isEnable = 1; // keeping motor on after job is completed?

	//flag that indicate if a motor is realy availible and not just a motor wiht no function.
	//on earlier implementation, motors with no pin where nullptrs but now all motors gets initialized
	//and its needed to show only true initialized motors inside webui and android app.
	bool isActivated = 0;
	bool endstop_hit = false;

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
	A =0,
	X,
	Y,
	Z
};

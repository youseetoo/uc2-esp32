#pragma once


const long MAX_VELOCITY_A = 20000;
const long MAX_ACCELERATION_A = 100000;
const long DEFAULT_ACCELERATION = 500000;

struct MotorState {
	long currentPosition = 0;
	bool isRunning = 0;
};

struct MotorDataI2C
{
	// a stripped down version of MotorData to be sent over I2C
	long targetPosition = 0;
	long speed = 0;
	bool isforever = false;
	bool absolutePosition = false;
	bool isStop = false;
}__attribute__((packed));

struct MotorData
{
	bool directionPinInverted = false;
	long speed = 0;
	long maxspeed = 200000;
	long acceleration = 0;
	long targetPosition = 0;
	long currentPosition = 0;
	int isforever = false;
	bool isaccelerated = false;
	// running relative or aboslute position! gets ignored when isforever is true
	bool absolutePosition = false; 	
	bool isEnable = true; // keeping motor on after job is completed?
	int qid = -1;
	bool isStop = false; // stop motor or not

	//flag that indicate if a motor is realy availible and not just a motor wiht no function.
	//on earlier implementation, motors with no pin where nullptrs but now all motors gets initialized
	//and its needed to show only true initialized motors inside webui and android app.
	bool isActivated = 0;
	bool stopped = true;
	bool endstop_hit = false;

	// for triggering frame or lineclock
	bool isTriggered = false; // state if we send a pulse
	long offsetTrigger = 0;	// offset in steps
	long triggerPeriod = -1; // give a pulse every n steps
	int triggerPin = -1;	 // pin to trigger (0,1,2 - depends on pinConfig)
	int dirPin = -1;
	int stpPin = -1;
}__attribute__((packed));




enum Stepper
{
	A =0,
	X,
	Y,
	Z
};

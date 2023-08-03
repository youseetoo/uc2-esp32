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
	bool isforever = false;
	bool isaccelerated = 0;
	bool absolutePosition = 0; 	// running relative or aboslute position?
	bool stopped = true;
	// milliseconds to switch off motors after operation
	int timeoutDisable = 1000;
	int timeLastActive = 0;
	bool isEnable = 1; // keeping motor on after job is completed?
};

enum Stepper
{
	A,
	X,
	Y,
	Z
};
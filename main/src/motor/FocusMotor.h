#pragma once
#include "AccelStepper.h"
#include "ArduinoJson.h"
#include "../wifi/WifiController.h"
#include "../config/ConfigController.h"
#include "../../ModuleController.h"
#include "MotorPins.h"

namespace RestApi
{
	void FocusMotor_act();
	void FocusMotor_get();
	void FocusMotor_set();
	void FocusMotor_setCalibration();
};

struct MotorData
{
	long speed = 0;
	long maxspeed = 200000;
	long acceleration = 0;
	long targetPosition = 0;
	bool isforever = false;
	bool isaccelerated = false;
	bool absolutePosition = false;
	bool stopped = true;
};

enum Stepper
{
	A,
	X,
	Y,
	Z
};

class FocusMotor : public Module
{

public:
	FocusMotor();
	~FocusMotor();
	bool DEBUG = false;

// for stepper.h
#define MOTOR_STEPS 200
#define SLEEP 0
#define MS1 0
#define MS2 0
#define MS3 0
#define RPM 120

	// global variables for the motor

	int MOTOR_ACCEL = 5000;
	int MOTOR_DECEL = 5000;

	static const int FULLSTEPS_PER_REV_A = 200;
	static const int FULLSTEPS_PER_REV_X = 200;
	static const int FULLSTEPS_PER_REV_Y = 200;
	static const int FULLSTEPS_PER_REV_Z = 200;

	long MAX_VELOCITY_A = 20000;
	long MAX_VELOCITY_X = 20000;
	long MAX_VELOCITY_Y = 20000;
	long MAX_VELOCITY_Z = 20000;
	long MAX_ACCELERATION_A = 100000;
	long MAX_ACCELERATION_X = 100000;
	long MAX_ACCELERATION_Y = 100000;
	long MAX_ACCELERATION_Z = 100000;

	std::array<AccelStepper *, 4> steppers;
	std::array<MotorData *, 4> data;
	// std::array<MotorPins *, 4> pins;
	MotorPins *pins[4];

	int setMinMaxRange(DynamicJsonDocument  ob);
	int act(DynamicJsonDocument  ob) override;
	int set(DynamicJsonDocument  ob) override;
	/*
		returns
		{
  "steppers": [
	{
	  "stepperid": 0,
	  "dir": 0,
	  "step": 0,
	  "enable": 0,
	  "dir_inverted": false,
	  "step_inverted": false,
	  "enable_inverted": false,
	  "position": 0,
	  "speed": 0,
	  "speedmax": 20000
	},
	{
	  "stepperid": 1,
	  "dir": 21,
	  "step": 19,
	  "enable": 18,
	  "dir_inverted": false,
	  "step_inverted": false,
	  "enable_inverted": true,
	  "position": 0,
	  "speed": 0,
	  "speedmax": 20000
	},
	{
	  "stepperid": 2,
	  "dir": 0,
	  "step": 0,
	  "enable": 0,
	  "dir_inverted": false,
	  "step_inverted": false,
	  "enable_inverted": false,
	  "position": 0,
	  "speed": 0,
	  "speedmax": 20000
	},
	{
	  "stepperid": 3,
	  "dir": 0,
	  "step": 0,
	  "enable": 0,
	  "dir_inverted": false,
	  "step_inverted": false,
	  "enable_inverted": false,
	  "position": 0,
	  "speed": 0,
	  "speedmax": 20000
	}
  ]
  }
	*/
	DynamicJsonDocument get(DynamicJsonDocument ob) override;
	void setup() override;
	void loop() override;	
	void stopAllDrives();
	void stopStepper(int i);
	void startStepper(int i);

private:
	int logcount;
	unsigned long nextSocketUpdateTime;
	bool isShareEnable;

	
	void startAllDrives();
	void applyMinPos(int i);
	void applyMaxPos(int i);
	void sendMotorPos(int i, int arraypos);
	void resetMotorPos(int i);
	bool shareEnablePin();
	void disableEnablePin(int i);
	void enableEnablePin(int i);
};

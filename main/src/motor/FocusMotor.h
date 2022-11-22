#include "../../config.h"
#pragma once
#include "../../config.h"
#include "AccelStepper.h"
#include "ArduinoJson.h"
#if defined IS_PS3 || defined IS_PS4
#include "../gamepads/ps_3_4_controller.h"
#endif
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

	void setMinMaxRange();
	void act() override;
	void set() override;

	void get() override;
	void setup() override;
	void loop() override;
	void stopAllDrives();

private:
	int logcount;
	unsigned long nextSocketUpdateTime;
	bool isShareEnable;

	void stopStepper(int i);
	void startStepper(int i);
	void startAllDrives();
	void applyMinPos(int i);
	void applyMaxPos(int i);
	void sendMotorPos(int i, int arraypos);
	void resetMotorPos(int i);
	bool shareEnablePin();
};

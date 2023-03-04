#pragma once
#include "AccelStepper.h"
#include "ArduinoJson.h"
#include "../wifi/WifiController.h"
#include "../config/ConfigController.h"
#include "../../ModuleController.h"
#include "../../PinConfig.h"

namespace RestApi
{
	void FocusMotor_act();
	void FocusMotor_get();
	void FocusMotor_set();
	void FocusMotor_setCalibration();
};


void sendUpdateToClients(void *p);

void driveMotorXLoop(void *pvParameter);
void driveMotorYLoop(void *pvParameter);
void driveMotorZLoop(void *pvParameter);
void driveMotorALoop(void *pvParameter);

struct MotorData
{
	long speed = 0;
	long maxspeed = 200000;
	long acceleration = 0;
	long targetPosition = 0;
	long currentPosition = 0;
	bool isforever = false;
	bool isaccelerated = 0;
	bool absolutePosition = 0;
	bool stopped = true;
	// milliseconds to switch off motors after operation
	int timeoutDisable = 1000;
	int timeLastActive = 0;
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

	// global variables for the motor
	long MAX_VELOCITY_A = 20000;
	long MAX_ACCELERATION_A = 100000;

	std::array<AccelStepper *, 4> steppers;
	std::array<MotorData *, 4> data;

	int act(DynamicJsonDocument  ob) override;
	DynamicJsonDocument get(DynamicJsonDocument ob) override;
	int set(DynamicJsonDocument doc);
	void setup() override;
	void loop() override;
	void stopAllDrives();
	void stopStepper(int i);
	void startStepper(int i);
	void sendMotorPos(int i, int arraypos);
	void driveMotorLoop(int stepperid);

private:
	int logcount;
	bool power_enable = false;

	void startAllDrives();
	void disableEnablePin(int i);
	void enableEnablePin(int i);
};

#pragma once
#include "FastAccelStepper.h"
#include "../config/ConfigController.h"
#include "../../ModuleController.h"
#include <Preferences.h>
#include "../i2c/tca9535.h"



void sendUpdateToClients(void *p);

void driveMotorXLoop(void *pvParameter);
void driveMotorYLoop(void *pvParameter);
void driveMotorZLoop(void *pvParameter);
void driveMotorALoop(void *pvParameter);

bool externalPinCallback();

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

class FocusMotor : public Module
{

public:
	FocusMotor();
	~FocusMotor();
	Preferences preferences;
	FastAccelStepperEngine engine = FastAccelStepperEngine();
	
	// global variables for the motor
	long MAX_VELOCITY_A = 20000;
	long MAX_ACCELERATION_A = 100000;
	long DEFAULT_ACCELERATION = 500000;

	std::array<FastAccelStepper *, 4> faststeppers;
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
	bool setExternalPin(uint8_t pin, uint8_t value);

private:
	tca9535 * _tca9535;
	int logcount;
	bool power_enable = false;

	void startAllDrives();
	void disableEnablePin(int i);
	void enableEnablePin(int i);
};

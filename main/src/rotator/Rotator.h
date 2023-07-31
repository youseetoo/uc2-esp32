#pragma once
#include "Module.h"
#include "AccelStepper.h"

void sendUpdateToClients(void *p);

void driveMotorXLoop(void *pvParameter);
void driveMotorYLoop(void *pvParameter);
void driveMotorZLoop(void *pvParameter);
void driveMotorALoop(void *pvParameter);

struct RotatorData
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
	bool isEnable = 1; // keeping motor on after job is completed?
};

enum Rotators
{
	rA,
	rX,
	rY,
	rZ
};

class Rotator : public Module
{

public:
	Rotator();
	~Rotator();

	// global variables for the motor
	long MAX_VELOCITY_A = 20000;
	long MAX_ACCELERATION_A = 100000;
	long DEFAULT_ACCELERATION_A = 200;

	std::array<AccelStepper *, 4> steppers;
	std::array<RotatorData *, 4> data;

	int act(cJSON *ob) override;
	cJSON *get(cJSON *ob) override;
	int set(cJSON *doc);
	void setup() override;
	void loop() override;
	void stopAllDrives();
	void stopStepper(int i);
	void startStepper(int i);
	void driveMotorLoop(int stepperid);
	void sendRotatorPos(int i, int arraypos);

private:
	int logcount;
	bool power_enable = false;

	void startAllDrives();
};
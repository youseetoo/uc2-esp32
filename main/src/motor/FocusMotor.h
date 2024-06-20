#pragma once
#include "FAccelStep.h"
#include "AccelStep.h"
#include "../config/ConfigController.h"
#include "../../ModuleController.h"
#include <Preferences.h>
#include "../i2c/tca9535.h"
#include "MotorTypes.h"

void sendUpdateToClients(void *p);

bool externalPinCallback();

class FocusMotor : public Module
{

public:
	FocusMotor();
	~FocusMotor();
	Preferences preferences;

	// global variables for the motor
	AccelStep accel;
	FAccelStep faccel;
	
	std::array<MotorData *, 4> data;

	StageScanningData *stageScanningData;

	int act(cJSON *ob) override;
	cJSON *get(cJSON *ob) override;
	int set(cJSON *doc);
	void setup() override;
	void loop() override;
	void stopStepper(int i);
	void startStepper(int i);
	void sendMotorPos(int i, int arraypos);
	bool setExternalPin(uint8_t pin, uint8_t value);
	int getExternalPinValue(uint8_t pin);
	void setPosition(Stepper s, int pos);
	void move(Stepper s, int steps, bool blocking);
	bool isRunning(int i);
	long getCurrentPosition(Stepper s);
	bool isDualAxisZ = false;
	
	TaskHandle_t TaskHandle_stagescan_t;
	
private:
	tca9535 *_tca9535;
	TCA9535_Register outRegister;
	TCA9535_Register inRegister;
	
	
	int logcount;
	bool power_enable = false;

	void disableEnablePin(int i);
	void enableEnablePin(int i);

	void init_tca();
	void dumpRegister(const char * name, TCA9535_Register configRegister);
};

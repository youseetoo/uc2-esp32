#include <PinConfig.h>"
#ifdef FOCUS_MOTOR
#pragma once
#include "cJsonTool.h"
#include "Arduino.h"
#include <PinConfig.h>"
#include "JsonKeys.h"
#include "FAccelStep.h"
#include "AccelStep.h"
#include "../config/ConfigController.h"

#include <Preferences.h>
#include "../i2c/tca9535.h"
#include "MotorTypes.h"

void sendUpdateToClients(void *p);

bool externalPinCallback();

namespace FocusMotor
{
	static Preferences preferences;

	// global variables for the motor
	static AccelStep accel;
	static std::array<MotorData *, 4> data;

	int act(cJSON *ob);
	cJSON *get(cJSON *ob);
	int set(cJSON *doc);
	void setup();
	void loop();
	void stopStepper(int i);
	void startStepper(int i);
	void sendMotorPos(int i, int arraypos);
	bool setExternalPin(uint8_t pin, uint8_t value);
	void setPosition(Stepper s, int pos);
	void move(Stepper s, int steps, bool blocking);
	static bool isRunning(int i);
	#ifdef USE_TCA9535
	static TCA9535_Register outRegister;
	void dumpRegister(const char * name, TCA9535_Register configRegister);
	void init_tca();
	#endif
	static FAccelStep faccel;
	
	static int logcount;
	static bool power_enable = false;

	void disableEnablePin(int i);
	void enableEnablePin(int i);


	
	
};
#endif

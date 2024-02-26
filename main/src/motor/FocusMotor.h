#include <PinConfig.h>
#ifdef FOCUS_MOTOR
#pragma once
#include "cJsonTool.h"
#include "Arduino.h"
#include "JsonKeys.h"

#include "../config/ConfigController.h"

#include <Preferences.h>
#ifdef USE_TCA9535
#include "../i2c/tca9535.h"
#endif
#include "MotorTypes.h"

#if !defined USE_FASTACCEL && !defined USE_ACCELSTEP
#error Pls set USE_FASTACCEL or USE_ACCELSTEP
#endif
#if defined USE_FASTACCEL && defined USE_ACCELSTEP
#error Pls set only USE_FASTACCEL or USE_ACCELSTEP, currently both are active
#endif

void sendUpdateToClients(void *p);

bool externalPinCallback();

namespace FocusMotor
{
	static Preferences preferences;

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
	bool isRunning(int i);
	#ifdef USE_TCA9535
	static TCA9535_Register outRegister;
	void dumpRegister(const char * name, TCA9535_Register configRegister);
	void init_tca();
	#endif
	
	static int logcount;
	static bool power_enable = false;

	MotorData ** getData();


	
	
};
#endif

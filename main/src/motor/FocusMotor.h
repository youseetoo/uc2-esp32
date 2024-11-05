#include <PinConfig.h>
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

#if !defined USE_FASTACCEL && !defined USE_ACCELSTEP && !defined I2C_MOTOR && !defined DIAL_CONTROLLER
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
	void setup();
	void loop();
	void stopStepper(int i);
	void startStepper(int i);
	void sendMotorPos(int i, int arraypos);
	void setPosition(Stepper s, int pos);
	void move(Stepper s, int steps, bool blocking);
	bool isRunning(int i);
	void enable(bool en);
	void toggleStepper(Stepper s, bool isStop);
	void setAutoEnable(bool enable);
	void setEnable(bool enable);
	static int logcount;
	static bool power_enable = false;

	static bool isDualAxisZ = false;
	MotorData **getData();
	void setData(int axis, MotorData *data);
};

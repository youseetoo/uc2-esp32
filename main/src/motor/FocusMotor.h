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

#if !defined USE_FASTACCEL && !defined USE_ACCELSTEP && !defined USE_I2C_MOTOR
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
	void setPosition(Stepper s, int pos);
	void move(Stepper s, int steps, bool blocking);
	bool isRunning(int i);
	void enable(bool en);
	void sendMotorDataI2C(MotorData motorData, int axis);
	int axis2address(int axis);
	void sendMotorDataI2C(MotorData motorData, uint8_t axis);
	MotorState pullMotorDataI2C(int axis);

	String motorDataToJson(MotorData motorData);
	void parseJsonI2C(cJSON *doc);
	static int logcount;
	static bool power_enable = false;

	static bool isDualAxisZ = false;
	static int pullMotorDataI2CTick = 0;
	MotorData **getData();
};

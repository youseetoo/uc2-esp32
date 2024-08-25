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

	// for A,X,Y,Z intialize the I2C addresses
	uint8_t i2c_addresses[] = {
		pinConfig.I2C_ADD_MOT_A,
		pinConfig.I2C_ADD_MOT_X,
		pinConfig.I2C_ADD_MOT_Y,
		pinConfig.I2C_ADD_MOT_Z};

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
	
	int axis2address(int axis);
	void sendMotorDataI2C(MotorData motorData, uint8_t axis);
	String motorDataToJson(MotorData motorData);

	static int logcount;
	static bool power_enable = false;

	static bool isDualAxisZ = false;

	MotorData **getData();
};

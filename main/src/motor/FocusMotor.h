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

#ifdef MOTOR_CONTROLLER
#if !defined USE_FASTACCEL && !defined USE_ACCELSTEP && !defined I2C_MOTOR && !defined DIAL_CONTROLLER
#error Pls set USE_FASTACCEL or USE_ACCELSTEP
#endif
#endif
#if defined USE_FASTACCEL && defined USE_ACCELSTEP
#error Pls set only USE_FASTACCEL or USE_ACCELSTEP, currently both are active
#endif


namespace FocusMotor
{
	void setup();
	void loop();
	void stopStepper(int i);
	void startStepper(int i, bool reduced);
	void sendMotorPos(int i, int arraypos);
	void setPosition(Stepper s, int pos);
	void move(Stepper s, int steps, bool blocking);
	bool isRunning(int i);
	void toggleStepper(Stepper s, bool isStop, bool reduced);
	void setAutoEnable(bool enable);
	void setEnable(bool enable);
	void setDualAxisZ(bool dual);
	bool getDualAxisZ();
	
	void updateData(int axis); // pull motor data to the data-array
	MotorData **getData();
	void setData(int axis, MotorData *data);
};

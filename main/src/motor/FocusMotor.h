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
#if !defined USE_FASTACCEL && !defined USE_ACCELSTEP && !defined I2C_MOTOR && !defined DIAL_CONTROLLER && !defined CAN_SLAVE_MOTOR
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
	// array of MOTOR_AXIS_COUNT to keep track which motor is activated depending on the size of MOTOR_AXIS_COUNT
	static bool isActivated[MOTOR_AXIS_COUNT] = {false};

	void stopStepper(int i);
	void startStepper(int i, int reduced);
	void sendMotorPos(int i, int arraypos, int qid = -1);
	void setPosition(Stepper s, int pos);
	void move(Stepper s, int steps, bool blocking);
	bool isRunning(int i);
	void toggleStepper(Stepper s, bool isStop, int reduced); // reduced=> 0: full values MotorData, 1: reduced MotorDataReduced, 2: single VAlue for CAN
	void setAutoEnable(bool enable);
	void setEnable(bool enable);
	void setDualAxisZ(bool dual);
	bool getDualAxisZ();
	void setSoftLimits(int axis, int32_t minPos, int32_t maxPos, bool isEnabled);
	void updateData(int axis); // pull motor data to the data-array
	MotorData **getData();
	void setData(int axis, MotorData *data);
	
	// Motor position functions
	long getCurrentMotorPosition(int axis); // Get real-time position from FastAccelStepper
	
	// Encoder-based motion control functions
	bool isEncoderBasedMotionEnabled(int axis);
	void setEncoderBasedMotion(int axis, bool enabled);
	void startEncoderBasedMotion(int axis);
};

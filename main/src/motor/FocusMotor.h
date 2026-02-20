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
#if !defined USE_FASTACCEL && !defined USE_ACCELSTEP &&  !defined DIAL_CONTROLLER && !defined CAN_RECEIVE_MOTOR && !defined CAN_SEND_COMMANDS
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
	void setSoftLimits(int axis, int32_t minPos, int32_t maxPos, bool isEnabled);
	void updateData(int axis); // pull motor data to the data-array
	MotorData **getData();
	void setData(int axis, MotorData *data);
	void moveMotor(int32_t pos, int32_t speed, int s, bool isRelative);
	
	// Motor position functions
	long getCurrentMotorPosition(int axis); // Get real-time position from FastAccelStepper
	
	// Hard limit functions (emergency stop on endstop hit during normal operation)
	// Only relevant on CAN slave/satellite motor controllers
	void setHardLimit(int axis, bool enabled, bool polarity);
	void checkHardLimits(); // Called in loop to check endstops and emergency stop if triggered (only on slaves)
	void clearHardLimitTriggered(int axis); // Called after successful homing
	
	// Encoder-based motion control functions
	bool isEncoderBasedMotionEnabled(int axis);
	void setEncoderBasedMotion(int axis, bool enabled);
	void startEncoderBasedMotion(int axis);
	
	// Hybrid mode support: determines if an axis should use CAN or native driver
	// Returns true if axis should be routed to CAN bus, false for native driver
	bool shouldUseCANForAxis(int axis);
	
	// Hybrid mode support: converts internal hybrid axis (4,5,6,7) to CAN axis (0,1,2,3)
	// Used when sending commands to CAN satellites in hybrid mode
	int getCANAxisForHybrid(int axis);
};

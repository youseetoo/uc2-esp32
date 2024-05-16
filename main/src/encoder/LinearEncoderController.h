#pragma once
#include "../motor/FocusMotor.h"
#include "AS5311AB.h"
#include "PIDController.h"

struct LinearEncoderData
{	
	float conversionFactor = 2.0 / 1024.0; // 2mm per 1024 steps from AS5311
	bool encoderDirection = false;
	bool motorDirection = false;
	float posval = 0.0f;
	bool requestPosition = false;
	int linearencoderID = -1;
	bool requestCalibration = false;
	bool homeAxis = false;
	bool movePrecise = false;
	bool isAbsolute = true;
	int calibsteps = 0;
	int dataPin = -1;
	int clkPin = -1;
	int endPosRelease = 0;
	float positionPreMove = 0.0f;
	float positionToGo = 0.0f;
	float valuePostCalib = 0.0f;
	float stepsPerMM = 0.0f;
	long timeSinceMotorStart = 0;
	float lastPosition = -1000000.0f;
	float maxSpeed = 10000.0f;
	bool correctResidualOnly = false; // correct for residual error after open-loop steps only
	float stp2phys = 1.0f; // conversion factor from steps to physical units
	// PID controller variablexs
	float c_p = 2.;
	float c_i = 0.1;
	float c_d = 0.1;
	float mumPerStep = 1.95f; // dividided by 2048 steps
	PIDController pid = PIDController(c_p, c_i, c_d);
};

void processHomeLoop(void * p);

namespace LinearEncoderController
{
	void setCurrentPosition(int encoderIndex, float offsetPos);
	float getCurrentPosition(int encoderIndex);

	int act(cJSON * ob);
	cJSON * get(cJSON * ob);
	void setup();
	void loop();

	float calculateRollingAverage(float newValue);
};

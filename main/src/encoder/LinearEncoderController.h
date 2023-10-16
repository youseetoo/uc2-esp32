#pragma once

#include "../../ModuleController.h"
#include "../motor/FocusMotor.h"
#include "AS5311.h"
#include "AS5311AB.h"


struct LinearEncoderData
{	
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
	float valuePreCalib = 0.0f;
	float positionToGo = 0.0f;
	float valuePostCalib = 0.0f;
	float stepsPerMM = 0.0f;
	float offset = 0.0f;
	float lastPosition = -1000000.0f;
	// PID controller variables
	float c_p = 2.;
	float c_i = 0.1;
	float c_d = 0.1;
};

void processHomeLoop(void * p);

class LinearEncoderController : public Module
{

public:
	LinearEncoderController();
	~LinearEncoderController();
	
    std::array<LinearEncoderData *, 4> edata;
    //std::array<AS5311 *, 4> encoders;
	std::array<AS5311AB, 4> encoders;

	void setCurrentPosition(int encoderIndex, float offsetPos);
	float getCurrentPosition(int encoderIndex);

	int act(cJSON * ob) override;
	cJSON * get(cJSON * ob) override;
	void setup() override;
	void loop() override;

private:
	float conversionFactor = 2.0/1024.0; // 2mm per 1024 steps from AS5311
	float calculateRollingAverage(float newValue);
};

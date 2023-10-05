#pragma once

#include "../../ModuleController.h"
#include "../motor/FocusMotor.h"
#include "AS5311.h"

struct LinearEncoderData
{	
	float posval = 0.0f;
	bool requestPosition = false;
	int linearencoderID = -1;
	bool requestCalibration = false;
	bool movePrecise = false;
	int calibsteps = 0;
	int dataPin = -1;
	int clkPin = -1;
	float valuePreCalib = 0.0f;
	float positionToGo = 0.0f;
	float valuePostCalib = 0.0f;
	float stepsPerMM = 0.0f;
};

void processHomeLoop(void * p);

class LinearEncoderController : public Module
{

public:
	LinearEncoderController();
	~LinearEncoderController();
	
    std::array<LinearEncoderData *, 4> edata;
    std::array<AS5311 *, 4> encoders;


	int act(cJSON * ob) override;
	cJSON * get(cJSON * ob) override;
	void setup() override;
	void loop() override;

private:
	float readValue(int clkPin, int dataPin);
};

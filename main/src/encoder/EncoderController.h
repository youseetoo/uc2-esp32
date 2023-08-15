#pragma once

#include "../../ModuleController.h"
#include "../motor/FocusMotor.h"

struct EncoderData
{	
	float posval = 0.0f;
	bool requestPosition = false;
	int encoderID = -1;
	bool requestCalibration = false;
	int calibsteps = 0;
	int dataPin = -1;
	int clkPin = -1;
	float valuePreCalib = 0.0f;
	float valuePostCalib = 0.0f;
	float stepsPerMM = 0.0f;
};

void processHomeLoop(void * p);

class EncoderController : public Module
{

public:
	EncoderController();
	~EncoderController();
	bool DEBUG = true;
	int homeEndposRelease = 2000;
	bool isHoming = false;
	std::array<EncoderData *, 4> edata;

	int act(cJSON * ob) override;
	cJSON * get(cJSON * ob) override;
	void setup() override;
	void loop() override;

private:
	float readValue(int clkPin, int dataPin);
	float decode(int clkPin, int dataPin);
};
#include <PinConfig.h>
#pragma once
#include "cJSON.h"
#include "Arduino.h"

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

namespace EncoderController
{

	static bool DEBUG = true;
	static int homeEndposRelease = 2000;
	static bool isHoming = false;
	static std::array<EncoderData *, 4> edata;

	int act(cJSON * ob);
	cJSON * get(cJSON * ob);
	void setup();
	void loop();

	float readValue(int clkPin, int dataPin);
	float decode(int clkPin, int dataPin);
};

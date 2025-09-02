#pragma once
#include "AS5311AB.h"
#include "PIDController.h"
#include "cJSON.h"
#include "../motor/FocusMotor.h"

// Encoder interface selection enum
enum EncoderInterface {
    ENCODER_INTERRUPT_BASED = 1,  // Default interrupt-based implementation
    ENCODER_PCNT_BASED = 0       // PCNT hardware-based implementation
};

// Forward declare to avoid circular dependency
namespace PCNTEncoderController {
    void setup();
    float getCurrentPosition(int encoderIndex);
    void setCurrentPosition(int encoderIndex, float offsetPos);
    int16_t getPCNTCount(int encoderIndex);
    void resetPCNTCount(int encoderIndex);
    bool isPCNTAvailable();
}

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
	// Encoder interface selection
	EncoderInterface encoderInterface = ENCODER_INTERRUPT_BASED; // Default to interrupt-based
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

	static bool isPlot = 0; // plot position values 

	float calculateRollingAverage(float newValue);
	
	// Encoder interface selection functions
	void setEncoderInterface(int encoderIndex, EncoderInterface interface);
	EncoderInterface getEncoderInterface(int encoderIndex);
	bool isPCNTEncoderSupported();
};

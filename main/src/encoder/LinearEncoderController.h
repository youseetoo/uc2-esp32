#pragma once
#include "AS5311AB.h"
#include "PIDController.h"
#include "cJSON.h"
#include "../motor/FocusMotor.h"

// Encoder interface selection enum
enum EncoderInterface {
    ENCODER_INTERRUPT_BASED = 0,  // Default interrupt-based implementation
    ENCODER_PCNT_BASED = 1       // PCNT hardware-based implementation
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
	bool encoderDirection = false;
	bool motorDirection = false;
	float posval = 0.0f; // Calculated position in µm (derived from stepCount * globalConversionFactor)
	long stepCount = 0; // Raw integer step count for accuracy
	int linearencoderID = -1;
	bool homeAxis = false;
	bool movePrecise = false;
	bool isAbsolute = true;
	float positionPreMove = 0.0f;
	float positionToGo = 0.0f;
	long timeSinceMotorStart = 0;
	float lastPosition = -1000000.0f;
	float maxSpeed = 10000.0f;
	
	// Stalling detection parameters
	float stallThreshold = 10.0f; // steps - threshold for detecting no movement
	unsigned long stallTimeout = 300; // ms before considering motor stuck
	
	// Debug message control
	bool enableDebug = false; // Enable detailed debug output during motion
	
	
	// PID controller variables
	float c_p = 2.;
	float c_i = 0.1;
	float c_d = 0.1;
	PIDController pid = PIDController(c_p, c_i, c_d);
	// Encoder interface selection
	EncoderInterface encoderInterface = ENCODER_INTERRUPT_BASED; // Default to interrupt-based
};

namespace LinearEncoderController
{
	void setCurrentPosition(int encoderIndex, float offsetPos);
	float getCurrentPosition(int encoderIndex);
	
	// Helper function for motor position tracking
	long getCurrentMotorPosition(int stepperIndex);

	int act(cJSON * ob);
	cJSON * get(cJSON * ob);
	void setup();
	void loop();

	static bool isPlot = 0; // plot position values
	
	// Encoder interface selection functions
	void setEncoderInterface(int encoderIndex, EncoderInterface interface);
	EncoderInterface getEncoderInterface(int encoderIndex);
	bool isPCNTEncoderSupported();
	
	// Encoder position persistence functions
	void saveEncoderPosition(int encoderIndex);
	void loadEncoderPosition(int encoderIndex);
	void saveAllEncoderPositions();
	void loadAllEncoderPositions();
	
	// Fast precision motion control
	void executePrecisionMotionBlocking(int stepperIndex);
	void executeHomingBlocking(int stepperIndex, int speed);
	
	// Task-based non-blocking alternatives (optional)
	bool startPrecisionMotionTask(int stepperIndex);
	bool startHomingTask(int stepperIndex, int speed);
	void setTaskBasedOperation(bool enabled);
	bool isTaskBasedOperationEnabled();

};

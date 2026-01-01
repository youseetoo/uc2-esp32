#pragma once
#include "AS5311AB.h"
#include "SimplePIDController.h"
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

/**
 * Simplified encoder data for single-axis feedback control.
 * Uses two-stage PID (far/near).
 */
struct LinearEncoderData
{	
	// Encoder state
	bool encoderDirection = false;
	bool motorDirection = false;
	volatile int32_t stepCount = 0;  // Raw encoder count (ISR updated)
	float posval = 0.0f;             // Position value (for compatibility)
	int linearencoderID = -1;
	
	// Motion state
	bool homeAxis = false;
	bool movePrecise = false;
	bool isAbsolute = true;
	int32_t positionPreMove = 0;
	int32_t positionToGo = 0;        // Target in encoder counts
	unsigned long timeSinceMotorStart = 0;
	float lastPosition = -1000000.0f; // For compatibility
	
	// Two-stage PID controller (from testFeedbackStepper)
	SimplePIDController pid;
	
	// Legacy PID values (kept for JSON parsing compatibility)
	float c_p = 80.0f;   // Maps to kpFar
	float c_i = 5.0f;    // Maps to kiNear
	float c_d = 8.0f;    // Maps to kd
	float maxSpeed = 15000.0f;
	
	// Stall detection (mapped to SimplePIDController)
	float stallThreshold = 3.0f;     // Maps to pid.stallNoiseThreshold
	unsigned long stallTimeout = 150; // Maps to pid.stallTimeMs
	float minDistanceForStallCheck = 20.0f;
	
	// Debug output
	bool enableDebug = false;
	unsigned long lastDebugTime = 0;
	unsigned long debugInterval = 50;
	
	// Encoder interface selection
	EncoderInterface encoderInterface = ENCODER_PCNT_BASED;
};

namespace LinearEncoderController
{
	// ============= Position Functions =============
	
	// Get/set encoder position in counts
	int32_t getEncoderPosition(int encoderIndex = 1);
	void setEncoderPosition(int encoderIndex, int32_t position);
	void zeroEncoder(int encoderIndex = 1);
	
	// Legacy float interface (for compatibility)
	void setCurrentPosition(int encoderIndex, float offsetPos);
	float getCurrentPosition(int encoderIndex);
	
	// ============= Motion Control =============
	
	/**
	 * Precision motion with two-stage PID control.
	 * Position in encoder counts, computed on host.
	 * 
	 * JSON: {"task":"/motor_act", "motor":{"steppers":[
	 *   {"stepperid":1, "position":1000, "precise":1}
	 * ]}}
	 */
	void movePrecise(int32_t targetPosition, bool isAbsolute = true, int axisIndex = 1);
	
	/**
	 * Stall-based homing. Position set to 0 when stall detected.
	 * 
	 * JSON: {"task":"/linearencoder_act", "home":{"speed":-5000}}
	 */
	void homeAxis(int speed, int axisIndex = 1);
	
	/**
	 * Stop any ongoing motion.
	 */
	void stopMotion(int axisIndex = 1);
	
	/**
	 * Check if motion is active.
	 */
	bool isMotionActive(int axisIndex = 1);

	// ============= JSON Interface =============
	int act(cJSON * ob);
	cJSON * get(cJSON * ob);
	void setup();
	void loop();

	static bool isPlot = 0;
	
	// ============= Configuration =============
	
	/**
	 * Get encoder data for direct configuration access.
	 */
	LinearEncoderData* getEncoderData(int axisIndex = 1);
	
	/**
	 * Configure two-stage PID parameters.
	 */
	void configurePID(int axisIndex, float kpFar, float kpNear, float ki, float kd, int32_t nearThreshold);
	
	void setEncoderInterface(int encoderIndex, EncoderInterface interface);
	EncoderInterface getEncoderInterface(int encoderIndex);
	bool isPCNTEncoderSupported();
	
	// ============= Persistence =============
	void saveEncoderPosition(int encoderIndex);
	void loadEncoderPosition(int encoderIndex);
	void saveAllEncoderPositions();
	void loadAllEncoderPositions();
	
	// ============= Blocking Execution =============
	void executePrecisionMotionBlocking(int stepperIndex);
	void executeHomingBlocking(int stepperIndex, int speed);
	
	// Task-based (optional, for non-blocking)
	bool startPrecisionMotionTask(int stepperIndex);
	bool startHomingTask(int stepperIndex, int speed);
	void setTaskBasedOperation(bool enabled);
	bool isTaskBasedOperationEnabled();
};

#pragma once
#include "SimplePIDController.h"
#include "cJSON.h"

// Forward declarations
namespace FocusMotor {
    struct MotorData;
    MotorData **getData();
    void startStepper(int i, int reduced);
    void stopStepper(int i);
    bool isRunning(int i);
}

/**
 * Simplified encoder feedback data for single axis operation.
 * Designed for one ESP32 per motor-encoder pair.
 */
struct LinearEncoderDataSimple
{   
    // Encoder state
    volatile int32_t encoderCount = 0;  // Raw encoder count (updated by ISR/PCNT)
    bool encoderDirection = false;       // Encoder counting direction
    bool motorDirection = false;         // Motor direction inversion
    int linearencoderID = 1;             // Default to X-axis (1)
    
    // Motion state
    bool isHoming = false;
    bool isMovePrecise = false;
    int32_t targetPosition = 0;          // Target in encoder counts
    
    // Two-stage PID controller
    SimplePIDController pid;
    
    // Debug
    bool enableDebug = false;
    unsigned long lastDebugTime = 0;
    unsigned long debugInterval = 50;     // Debug print interval in ms
};

namespace LinearEncoderController
{
    // ============= Main Interface =============
    
    /**
     * Initialize encoder controller.
     * Sets up PCNT or interrupt-based counting for X-axis.
     */
    void setup();
    
    /**
     * Main loop function - should not block.
     */
    void loop();
    
    /**
     * Handle JSON commands via /linearencoder_act endpoint.
     * Simplified interface for:
     * - Precision motion: {"task":"/linearencoder_act", "moveP":{"position":1000}}
     * - Homing: {"task":"/linearencoder_act", "home":{"speed":-5000}}
     * - Get position: {"task":"/linearencoder_get"}
     */
    int act(cJSON *ob);
    cJSON *get(cJSON *ob);
    
    // ============= Encoder Position =============
    
    /**
     * Get current encoder position in counts.
     * @param encoderIndex Axis index (1 = X-axis)
     */
    int32_t getEncoderPosition(int encoderIndex = 1);
    
    /**
     * Set/reset encoder position to specific count value.
     * @param encoderIndex Axis index (1 = X-axis)
     * @param position New position value in encoder counts
     */
    void setEncoderPosition(int encoderIndex, int32_t position);
    
    /**
     * Zero the encoder position.
     * @param encoderIndex Axis index (1 = X-axis)
     */
    void zeroEncoder(int encoderIndex = 1);
    
    // ============= Motion Control =============
    
    /**
     * Move to target position using encoder feedback.
     * Called from /motor_act with "precise":1 flag.
     * 
     * @param targetPosition Target in encoder counts
     * @param isAbsolute true = absolute position, false = relative
     * @param axisIndex Motor/encoder axis (default 1 = X)
     */
    void movePrecise(int32_t targetPosition, bool isAbsolute = true, int axisIndex = 1);
    
    /**
     * Execute stall-based homing.
     * Motor runs until stall is detected, then position is set to 0.
     * 
     * @param speed Homing speed (sign determines direction)
     * @param axisIndex Motor/encoder axis (default 1 = X)
     */
    void homeAxis(int speed, int axisIndex = 1);
    
    /**
     * Stop any ongoing motion.
     * @param axisIndex Motor/encoder axis (default 1 = X)
     */
    void stopMotion(int axisIndex = 1);
    
    /**
     * Check if motion is in progress.
     * @param axisIndex Motor/encoder axis (default 1 = X)
     */
    bool isMotionActive(int axisIndex = 1);
    
    // ============= Configuration =============
    
    /**
     * Get pointer to encoder data for direct configuration.
     * Allows setting PID parameters, stall thresholds, etc.
     */
    LinearEncoderDataSimple* getEncoderData(int axisIndex = 1);
    
    /**
     * Configure two-stage PID parameters.
     * @param axisIndex Motor/encoder axis
     * @param kpFar P gain for far distance
     * @param kpNear P gain for near distance
     * @param ki I gain (used only in near mode)
     * @param kd D gain
     * @param nearThreshold Distance to switch far->near mode
     */
    void configurePID(int axisIndex, 
                      float kpFar, float kpNear, 
                      float ki, float kd,
                      int32_t nearThreshold);
    
    /**
     * Configure stall detection parameters.
     * @param axisIndex Motor/encoder axis
     * @param noiseThreshold Position change below this = not moving
     * @param timeoutMs Time without movement to detect stall
     * @param maxRetries Max retry attempts before error
     */
    void configureStallDetection(int axisIndex,
                                 int32_t noiseThreshold,
                                 uint32_t timeoutMs,
                                 uint8_t maxRetries);
    
    /**
     * Configure velocity limits.
     * @param axisIndex Motor/encoder axis  
     * @param maxVelocity Maximum velocity in steps/sec
     * @param minVelocity Minimum velocity threshold
     */
    void configureVelocity(int axisIndex, float maxVelocity, float minVelocity);
    
    /**
     * Set encoder counting direction.
     * @param axisIndex Motor/encoder axis
     * @param inverted true to invert counting direction
     */
    void setEncoderDirection(int axisIndex, bool inverted);
    
    /**
     * Set motor direction.
     * @param axisIndex Motor/encoder axis
     * @param inverted true to invert motor direction
     */
    void setMotorDirection(int axisIndex, bool inverted);
    
    // ============= Persistence =============
    
    /**
     * Save encoder position to flash.
     */
    void saveEncoderPosition(int axisIndex = 1);
    
    /**
     * Load encoder position from flash.
     */
    void loadEncoderPosition(int axisIndex = 1);
    
    // ============= Debug =============
    
    /**
     * Enable/disable debug output during motion.
     */
    void setDebugOutput(int axisIndex, bool enabled, unsigned long intervalMs = 50);
    
    /**
     * Check if PCNT hardware encoder is available.
     */
    bool isPCNTAvailable();
}

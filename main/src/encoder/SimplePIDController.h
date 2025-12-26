#pragma once
#include <Arduino.h>

/**
 * Two-stage PID velocity controller for encoder-based stepper motion.
 * Ported from testFeedbackStepper standalone implementation.
 * 
 * Features:
 * - Two-stage control: aggressive when far, precise when near
 * - Built-in stall detection with retry logic
 * - Anti-windup integral limiting
 * - 1kHz control loop optimized
 */
class SimplePIDController
{
public:
    // ============= PID Parameters (FAR mode - aggressive approach) =============
    float kpFar = 80.0f;      // Higher P for fast response
    float kiFar = 0.0f;       // No integral during fast approach
    float kdFar = 8.0f;       // Some damping to prevent overshoot
    
    // ============= PID Parameters (NEAR mode - precise positioning) =============
    float kpNear = 100.0f;    // High P for strong final correction
    float kiNear = 5.0f;      // Integral to eliminate steady-state error
    float kdNear = 8.0f;      // Higher D to prevent oscillation
    
    // ============= Motion Parameters =============
    int32_t nearThreshold = 50;      // Encoder counts threshold to switch FAR->NEAR
    float maxVelocity = 15000.0f;    // Max commanded velocity (steps/sec)
    float minVelocity = 80.0f;       // Min velocity before stopping
    int32_t positionTolerance = 1;   // Encoder counts tolerance for "arrived"
    float integralLimit = 500.0f;    // Anti-windup limit for integral term
    
    // ============= Stall Detection Parameters =============
    int32_t stallNoiseThreshold = 3;     // Position change below this = not moving
    int32_t stallErrorThreshold = 20;    // Only detect stall if error > this
    uint32_t stallTimeMs = 150;          // Time without movement to detect stall
    uint8_t stallMaxRetries = 3;         // Max retry attempts before error
    float stallVelocityReduction = 0.5f; // Reduce velocity by this factor on retry
    
    // ============= Control State =============
    int32_t setpoint = 0;
    float integralError = 0.0f;
    int32_t lastError = 0;
    uint32_t lastControlTimeUs = 0;
    bool positionReached = true;
    bool isActive = false;
    
    // Stall detection state
    int32_t stallReferencePos = 0;
    uint32_t stallCheckStartMs = 0;
    uint8_t stallRetryCount = 0;
    float velocityLimitFactor = 1.0f;
    bool stallErrorState = false;

    SimplePIDController() = default;
    
    /**
     * Initialize with custom PID parameters
     */
    SimplePIDController(float kpF, float kiF, float kdF, float kpN, float kiN, float kdN)
        : kpFar(kpF), kiFar(kiF), kdFar(kdF), kpNear(kpN), kiNear(kiN), kdNear(kdN) {}
    
    /**
     * Set new target position in encoder counts.
     * Resets PID state and stall detection.
     */
    void setTargetPosition(int32_t targetEncPos, int32_t currentEncPos)
    {
        setpoint = targetEncPos;
        
        // Reset PID state for new target
        integralError = 0.0f;
        lastError = targetEncPos - currentEncPos;
        positionReached = false;
        isActive = true;
        
        // Reset stall detection state
        stallRetryCount = 0;
        velocityLimitFactor = 1.0f;
        stallErrorState = false;
        stallReferencePos = currentEncPos;
        stallCheckStartMs = millis();
    }
    
    /**
     * Compute velocity command based on current encoder position.
     * Call at high frequency (1kHz recommended) for smooth control.
     * 
     * @param currentEncPos Current encoder position in counts
     * @param dt Time since last call in seconds (use 0 for auto-timing)
     * @return Velocity command in steps/sec, or 0 if position reached or error
     */
    float compute(int32_t currentEncPos, float dt = 0.0f)
    {
        // Auto-timing if dt not provided
        if (dt <= 0.0f) {
            uint32_t nowUs = micros();
            uint32_t dtUs = nowUs - lastControlTimeUs;
            lastControlTimeUs = nowUs;
            dt = dtUs * 1e-6f;
        }
        
        // Sanity check dt
        if (dt <= 0.0f || dt > 0.1f) {
            dt = 0.001f; // Default to 1ms
        }
        
        // Get current position error
        int32_t error = setpoint - currentEncPos;
        
        // === Stall Detection ===
        if (stallErrorState) {
            // Motor is in error state - do nothing until new setpoint
            return 0.0f;
        }
        
        // Only check for stall if we have significant error (not near target)
        if (abs(error) > stallErrorThreshold) {
            uint32_t nowMs = millis();
            int32_t posChange = abs(currentEncPos - stallReferencePos);
            
            if (posChange > stallNoiseThreshold) {
                // Position is changing - reset stall detection
                stallReferencePos = currentEncPos;
                stallCheckStartMs = nowMs;
            } else if (nowMs - stallCheckStartMs > stallTimeMs) {
                // Stall detected! Position hasn't changed enough
                stallRetryCount++;
                
                if (stallRetryCount >= stallMaxRetries) {
                    // Max retries exceeded - enter error state
                    stallErrorState = true;
                    isActive = false;
                    log_e("STALL ERROR: Max retries exceeded at pos=%d, target=%d", currentEncPos, setpoint);
                    return 0.0f;
                }
                
                // Reduce velocity and retry
                velocityLimitFactor *= stallVelocityReduction;
                stallReferencePos = currentEncPos;
                stallCheckStartMs = nowMs;
                integralError = 0.0f; // Reset integral
                
                log_w("STALL: Retry %d/%d - reducing velocity to %.0f%%", 
                      stallRetryCount, stallMaxRetries, velocityLimitFactor * 100.0f);
            }
        } else {
            // Near target - reset stall detection
            stallReferencePos = currentEncPos;
            stallCheckStartMs = millis();
        }
        
        // Check if we've reached the target
        if (abs(error) <= positionTolerance) {
            if (!positionReached) {
                integralError = 0.0f; // Reset integral when target reached
                positionReached = true;
                isActive = false;
            }
            lastError = error;
            return 0.0f;
        }
        
        positionReached = false;
        
        // === Two-Stage PID: Select gains based on distance to target ===
        float kp, ki, kd;
        if (abs(error) > nearThreshold) {
            // FAR mode: aggressive approach
            kp = kpFar;
            ki = kiFar;
            kd = kdFar;
            // Reset integral when in far mode to prevent windup
            integralError = 0.0f;
        } else {
            // NEAR mode: precise positioning
            kp = kpNear;
            ki = kiNear;
            kd = kdNear;
        }
        
        // === PID Calculation ===
        
        // Proportional term
        float pTerm = kp * error;
        
        // Integral term with anti-windup
        integralError += error * dt;
        integralError = constrain(integralError, -integralLimit, integralLimit);
        float iTerm = ki * integralError;
        
        // Derivative term (on error)
        float dTerm = 0.0f;
        if (dt > 0) {
            dTerm = kd * (error - lastError) / dt;
        }
        lastError = error;
        
        // Calculate velocity command
        float velocityCmd = pTerm + iTerm + dTerm;
        
        // Apply velocity limits (with stall reduction factor)
        float currentMaxVelocity = maxVelocity * velocityLimitFactor;
        velocityCmd = constrain(velocityCmd, -currentMaxVelocity, currentMaxVelocity);
        
        // Apply minimum velocity threshold to prevent stalling
        if (fabsf(velocityCmd) < minVelocity && abs(error) > positionTolerance) {
            velocityCmd = (error > 0) ? minVelocity : -minVelocity;
        }
        
        return velocityCmd;
    }
    
    /**
     * Stop motion and reset state
     */
    void stop()
    {
        isActive = false;
        positionReached = true;
        integralError = 0.0f;
    }
    
    /**
     * Check if motor is in stall error state
     */
    bool isStallError() const { return stallErrorState; }
    
    /**
     * Check if motion is complete
     */
    bool isComplete() const { return positionReached && !isActive; }
    
    /**
     * Get current stall retry count
     */
    uint8_t getStallRetryCount() const { return stallRetryCount; }
    
    /**
     * Get current setpoint
     */
    int32_t getSetpoint() const { return setpoint; }
};

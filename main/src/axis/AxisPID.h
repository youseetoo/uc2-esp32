#pragma once
#include <Arduino.h>

// ============================================================================
// AxisPID — two-stage velocity controller for SERVO mode (design v2, WP7).
//
// Ported from the legacy encoder/SimplePIDController.h control law, but with the
// built-in stall detection REMOVED: stalls are now handled uniformly by
// AxisWatchdog (WP4), independent of any PID. This class is pure control math.
//
// Setpoint / measured are in encoder COUNTS; the returned command is a signed
// velocity in STEPS/second (the gains fold the count-error → step-velocity
// scaling, matching the legacy tuning).
// ============================================================================

class AxisPID
{
public:
    // FAR mode — aggressive approach.
    float kpFar = 80.0f;
    float kiFar = 0.0f;
    float kdFar = 8.0f;
    // NEAR mode — precise positioning.
    float kpNear = 100.0f;
    float kiNear = 5.0f;
    float kdNear = 8.0f;

    int32_t nearThreshold = 50;    // counts: FAR -> NEAR switch
    float   maxVelocity = 15000.0f; // steps/s ceiling
    float   minVelocity = 80.0f;    // steps/s floor while not yet arrived
    int32_t positionTolerance = 1;  // counts: "arrived"
    float   integralLimit = 500.0f; // anti-windup clamp

    void setTarget(int32_t targetCounts, int32_t currentCounts)
    {
        setpoint = targetCounts;
        integralError = 0.0f;
        lastError = targetCounts - currentCounts;
        positionReached = false;
        active = true;
    }

    // Compute the velocity command (steps/s, signed). dt<=0 → auto-timed.
    float compute(int32_t currentCounts, float dt = 0.0f)
    {
        if (dt <= 0.0f)
        {
            uint32_t nowUs = micros();
            uint32_t dtUs = nowUs - lastControlTimeUs;
            lastControlTimeUs = nowUs;
            dt = dtUs * 1e-6f;
        }
        if (dt <= 0.0f || dt > 0.1f)
            dt = 0.001f;

        int32_t error = setpoint - currentCounts;

        if (abs(error) <= positionTolerance)
        {
            if (!positionReached)
            {
                integralError = 0.0f;
                positionReached = true;
                active = false;
            }
            lastError = error;
            return 0.0f;
        }
        positionReached = false;

        float kp, ki, kd;
        if (abs(error) > nearThreshold)
        {
            kp = kpFar; ki = kiFar; kd = kdFar;
            integralError = 0.0f; // no windup during the fast approach
        }
        else
        {
            kp = kpNear; ki = kiNear; kd = kdNear;
        }

        float pTerm = kp * (float)error;
        integralError += (float)error * dt;
        integralError = constrain(integralError, -integralLimit, integralLimit);
        float iTerm = ki * integralError;
        float dTerm = kd * (float)(error - lastError) / dt;
        lastError = error;

        float v = pTerm + iTerm + dTerm;
        v = constrain(v, -maxVelocity, maxVelocity);
        if (fabsf(v) < minVelocity)
            v = (error > 0) ? minVelocity : -minVelocity;
        return v;
    }

    void stop()
    {
        active = false;
        positionReached = true;
        integralError = 0.0f;
    }

    bool isComplete() const { return positionReached && !active; }
    int32_t getSetpoint() const { return setpoint; }

private:
    int32_t  setpoint = 0;
    float    integralError = 0.0f;
    int32_t  lastError = 0;
    uint32_t lastControlTimeUs = 0;
    bool     positionReached = true;
    bool     active = false;
};

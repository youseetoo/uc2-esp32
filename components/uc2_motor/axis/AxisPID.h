#pragma once
// T4.1: pure-C (was a C++ class with default member initializers). Header-only:
// struct + static inline functions. Uses the uc2 time/log façades, which are
// C-compatible (see uc2_time.h / uc2_log.h).
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h> // abs()
#include "uc2_hal.h"

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

typedef struct AxisPID {
    // FAR mode — aggressive approach.
    float kpFar;
    float kiFar;
    float kdFar;
    // NEAR mode — precise positioning.
    float kpNear;
    float kiNear;
    float kdNear;

    int32_t nearThreshold;     // counts: FAR -> NEAR switch
    float   maxVelocity;       // steps/s ceiling
    float   minVelocity;       // steps/s floor while not yet arrived
    int32_t positionTolerance; // counts: "arrived"
    float   integralLimit;     // anti-windup clamp

    // ---- state ----
    int32_t  setpoint;
    float    integralError;
    int32_t  lastError;
    uint32_t lastControlTimeUs;
    bool     positionReached;
    bool     active;
} AxisPID;

static inline float axisPidClampF(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Set the legacy default gains/limits (call once per axis at setup).
static inline void axis_pid_init(AxisPID *p)
{
    p->kpFar = 80.0f;
    p->kiFar = 0.0f;
    p->kdFar = 8.0f;
    p->kpNear = 100.0f;
    p->kiNear = 5.0f;
    p->kdNear = 8.0f;
    p->nearThreshold = 50;
    p->maxVelocity = 15000.0f;
    p->minVelocity = 80.0f;
    p->positionTolerance = 1;
    p->integralLimit = 500.0f;
    p->setpoint = 0;
    p->integralError = 0.0f;
    p->lastError = 0;
    p->lastControlTimeUs = 0;
    p->positionReached = true;  // matches the legacy default member init
    p->active = false;
}

static inline void axis_pid_set_target(AxisPID *p, int32_t targetCounts, int32_t currentCounts)
{
    log_i("AxisPID: setTarget=%d, currentCounts=%d", targetCounts, currentCounts);
    p->setpoint = targetCounts;
    p->integralError = 0.0f;
    p->lastError = targetCounts - currentCounts;
    p->positionReached = false;
    p->active = true;
}

// Compute the velocity command (steps/s, signed). dt<=0 → auto-timed.
static inline float axis_pid_compute(AxisPID *p, int32_t currentCounts, float dt)
{
    if (dt <= 0.0f)
    {
        uint32_t nowUs = micros();
        uint32_t dtUs = nowUs - p->lastControlTimeUs;
        p->lastControlTimeUs = nowUs;
        dt = dtUs * 1e-6f;
    }
    if (dt <= 0.0f || dt > 0.1f)
        dt = 0.001f;

    int32_t error = p->setpoint - currentCounts;

    if (abs(error) <= p->positionTolerance)
    {
        if (!p->positionReached)
        {
            p->integralError = 0.0f;
            p->positionReached = true;
            p->active = false;
        }
        p->lastError = error;
        return 0.0f;
    }
    p->positionReached = false;

    float kp, ki, kd;
    if (abs(error) > p->nearThreshold)
    {
        kp = p->kpFar; ki = p->kiFar; kd = p->kdFar;
        p->integralError = 0.0f; // no windup during the fast approach
    }
    else
    {
        kp = p->kpNear; ki = p->kiNear; kd = p->kdNear;
    }

    float pTerm = kp * (float)error;
    p->integralError += (float)error * dt;
    p->integralError = axisPidClampF(p->integralError, -p->integralLimit, p->integralLimit);
    float iTerm = ki * p->integralError;
    float dTerm = kd * (float)(error - p->lastError) / dt;
    p->lastError = error;

    float v = pTerm + iTerm + dTerm;
    v = axisPidClampF(v, -p->maxVelocity, p->maxVelocity);
    if (fabsf(v) < p->minVelocity)
        v = (error > 0) ? p->minVelocity : -p->minVelocity;
    return v;
}

static inline void axis_pid_stop(AxisPID *p)
{
    p->active = false;
    p->positionReached = true;
    p->integralError = 0.0f;
}

static inline bool axis_pid_is_complete(const AxisPID *p)
{
    return p->positionReached && !p->active;
}

static inline int32_t axis_pid_get_setpoint(const AxisPID *p)
{
    return p->setpoint;
}

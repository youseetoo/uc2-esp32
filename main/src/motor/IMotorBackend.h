#pragma once
#include "MotorTypes.h"
#include <stdint.h>

// Abstract interface for motor backends.
// The routing decision (local GPIO vs CAN vs hybrid) is made ONCE at boot
// in the factory function, not on every command.
class IMotorBackend {
public:
    virtual ~IMotorBackend() = default;

    // Lifecycle
    virtual void setup() = 0;
    virtual void loop()  = 0;

    // Commands
    virtual void startMove(int axis, MotorData* data, int reduced) = 0;
    virtual void stopMove(int axis) = 0;

    // State queries
    virtual long  getPosition(int axis) const = 0;
    virtual bool  isRunning(int axis)   const = 0;
    virtual void  updateData(int axis)        = 0;
    virtual void  setPosition(int axis, int pos) = 0;

    // Enable/disable
    virtual void setAutoEnable(bool enable) = 0;
    virtual void setEnable(bool enable)     = 0;

    // Identification (for logging / diagnostics)
    virtual const char* name() const = 0;
};

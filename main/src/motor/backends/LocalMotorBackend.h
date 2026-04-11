#pragma once
#include "../IMotorBackend.h"

// LocalMotorBackend — drives motors via the local GPIO stepper driver
// (FastAccelStepper or AccelStepper). This is what runs today on a standalone
// UC2_3 board or a CAN slave.
class LocalMotorBackend : public IMotorBackend {
public:
    void setup() override;
    void loop()  override;

    void startMove(int axis, MotorData* data, int reduced) override;
    void stopMove(int axis) override;

    long  getPosition(int axis) const override;
    bool  isRunning(int axis)   const override;
    void  updateData(int axis)        override;
    void  setPosition(int axis, int pos) override;

    void setAutoEnable(bool enable) override;
    void setEnable(bool enable)     override;

    const char* name() const override { return "LocalMotor"; }
};

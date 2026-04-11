#pragma once
#include "../IMotorBackend.h"
#include <array>

// HybridMotorBackend — holds a per-axis backend pointer.
// Some axes are local (LocalMotorBackend), some are remote (CANopenMotorBackend).
// Each call dispatches to the right per-axis backend.
class HybridMotorBackend : public IMotorBackend {
public:
    static constexpr int MAX_AXES = 4;

    // Set the backend for a specific axis. Takes ownership.
    void setAxisBackend(int axis, IMotorBackend* backend);

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

    const char* name() const override { return "HybridMotor"; }

    ~HybridMotorBackend() override;

private:
    std::array<IMotorBackend*, MAX_AXES> _backends = {nullptr, nullptr, nullptr, nullptr};
};

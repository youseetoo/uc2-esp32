#pragma once
#include "../IMotorBackend.h"
#include <stdint.h>

// CANopenMotorBackend — forwards motor commands to a remote CAN slave via SDO writes.
// Used on a pure CAN master where no local motor GPIO exists.
// Constructor takes a per-axis node-ID map so it knows where to send each axis.
class CANopenMotorBackend : public IMotorBackend {
public:
    // nodeIds[0..3] maps axis 0..3 to the CANopen node-id of the slave that owns it.
    // A nodeId of 0 means "axis not mapped / unavailable".
    explicit CANopenMotorBackend(const uint8_t nodeIds[4]);

    void setup() override;
    void loop()  override;

    void startMove(int axis, MotorData* data, int reduced) override;
    void stopMove(int axis) override;

    long  getPosition(int axis) const override;
    bool  isRunning(int axis)   const override;
    void  updateData(int axis)        override;
    void  setPosition(int axis, int pos) override;

    void setAutoEnable(bool /*enable*/) override {}
    void setEnable(bool /*enable*/)     override {}

    const char* name() const override { return "CANopenMotor"; }

private:
    uint8_t _nodeIds[4];

    // Cached state (populated via SDO reads or TPDO)
    mutable int32_t _cachedPos[4]     = {0, 0, 0, 0};
    mutable bool    _cachedRunning[4] = {false, false, false, false};

    bool writeMotorSDO(int axis, MotorData* data, bool isStop);
};

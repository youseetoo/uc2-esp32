// SdoEmit.h — expedited-SDO command helpers for nodes that only ORIGINATE
// commands (dial, PS4 bridge, PTZ bridge). Header-only; the dead-node muting
// state is per translation unit, which is what each originator wants anyway.
//
// Frame sequences mirror DeviceRouter::handleMotorAct / handleLaserAct so a
// slave can't tell whether the master or a bridge sent the command.
#pragma once
#ifdef CAN_CONTROLLER_CANOPEN

#include <Arduino.h>
#include "CANopenModule.h"
#include "UC2_OD_Indices.h"

namespace SdoEmit
{
    constexpr uint32_t kSdoTimeoutMs          = 40;
    constexpr uint8_t  kDeadNodeFailThreshold = 2;
    constexpr uint32_t kDeadNodeBackoffMs     = 2000;

    struct NodeHealth {
        uint8_t  nodeId        = 0;
        uint8_t  fails         = 0;
        uint32_t silentUntilMs = 0;
    };
    static NodeHealth s_health[8];

    static inline NodeHealth* healthSlot(uint8_t nodeId)
    {
        for (auto& h : s_health) if (h.nodeId == nodeId) return &h;
        for (auto& h : s_health) if (h.nodeId == 0)      { h.nodeId = nodeId; return &h; }
        return &s_health[0];
    }

    static inline bool nodeMuted(uint8_t nodeId)
    {
        return millis() < healthSlot(nodeId)->silentUntilMs;
    }

    static inline void noteSdo(uint8_t nodeId, bool ok)
    {
        NodeHealth* h = healthSlot(nodeId);
        if (ok) { h->fails = 0; h->silentUntilMs = 0; return; }
        if (++h->fails >= kDeadNodeFailThreshold) {
            h->silentUntilMs = millis() + kDeadNodeBackoffMs;
            log_w("node 0x%02X muted for %u ms (SDO timeouts)", nodeId, (unsigned)kDeadNodeBackoffMs);
        }
    }

    // 6-frame REMOTE motor command (OD 0x2000.. + command word 0x2006).
    static inline bool motor(uint8_t nodeId, uint8_t subAxis,
                             int32_t pos, int32_t speed, uint32_t accel,
                             bool isAbs, bool isForever, bool isStop)
    {
        if (nodeMuted(nodeId)) return false;
        const uint8_t sub = subAxis + 1;
        bool ok = true;
        ok &= CANopenModule::writeSDO_i32(nodeId, UC2_OD::MOTOR_TARGET_POSITION, sub, pos, kSdoTimeoutMs);
        ok &= CANopenModule::writeSDO_u32(nodeId, UC2_OD::MOTOR_SPEED, sub, (uint32_t)speed, kSdoTimeoutMs);
        if (accel > 0)
            ok &= CANopenModule::writeSDO_u32(nodeId, UC2_OD::MOTOR_ACCELERATION, sub, accel, kSdoTimeoutMs);
        ok &= CANopenModule::writeSDO_u8(nodeId, UC2_OD::MOTOR_IS_ABSOLUTE, sub, isAbs ? 1 : 0, kSdoTimeoutMs);
        ok &= CANopenModule::writeSDO_u8(nodeId, UC2_OD::MOTOR_IS_FOREVER, sub, isForever ? 1 : 0, kSdoTimeoutMs);
        const uint8_t cmdWord = isStop ? (uint8_t)(1u << (subAxis + 4)) : (uint8_t)(1u << subAxis);
        ok &= CANopenModule::writeSDO_u8(nodeId, UC2_OD::MOTOR_COMMAND_WORD, 0x00, cmdWord, kSdoTimeoutMs);
        noteSdo(nodeId, ok);
        return ok;
    }

    // LED matrix uniform fill: colour 0xRRGGBB (OD 0x2202) then array mode
    // (OD 0x2200: 0 = off, 1 = fill). Colour first so the slave's change
    // detector applies both in one pass.
    static inline bool led(uint8_t nodeId, bool on, uint32_t rgb)
    {
        if (nodeMuted(nodeId)) return false;
        bool ok = true;
        ok &= CANopenModule::writeSDO_u32(nodeId, UC2_OD::LED_UNIFORM_COLOUR, 0, rgb, kSdoTimeoutMs);
        ok &= CANopenModule::writeSDO_u8(nodeId, UC2_OD::LED_ARRAY_MODE, 0, on ? 1 : 0, kSdoTimeoutMs);
        noteSdo(nodeId, ok);
        return ok;
    }

    // Single-frame laser PWM write (OD 0x2100, sub = channel + 1).
    static inline bool laser(uint8_t nodeId, uint8_t subAxis, uint16_t pwm)
    {
        if (nodeMuted(nodeId)) return false;
        bool ok = CANopenModule::writeSDO_u16(nodeId, UC2_OD::LASER_PWM_VALUE,
                                              (uint8_t)(subAxis + 1), pwm, kSdoTimeoutMs);
        noteSdo(nodeId, ok);
        return ok;
    }
}

#endif // CAN_CONTROLLER_CANOPEN

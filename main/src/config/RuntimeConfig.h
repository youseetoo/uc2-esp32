#pragma once
#include <stdint.h>

enum class NodeRole : uint8_t {
    STANDALONE = 0,   // No CAN — serial/WiFi only (existing UC2_2, UC2_3 boards)
    CAN_MASTER = 1,   // CAN HAT — bridges serial to CAN bus
    CAN_SLAVE  = 2    // Satellite — receives commands via CAN
};

struct RuntimeConfig {
    // Module enables — defaults match the most common board (UC2_3)
    bool motor       = false;
    bool laser       = false;
    bool led         = false;
    bool home        = false;
    bool digitalIn   = false;
    bool digitalOut  = false;
    bool analogIn    = false;
    bool analogOut   = false;
    bool dac         = false;
    bool tmc         = false;
    bool encoder     = false;
    bool joystick    = false;
    bool galvo       = false;
    bool bluetooth   = false;
    bool wifi        = false;
    bool pid         = false;
    bool scanner     = false;
    bool message     = false;
    bool dial        = false;
    bool objective   = false;
    bool heat        = false;

    // CAN role
    NodeRole canRole = NodeRole::STANDALONE;
    uint8_t  canNodeId = 10;     // CANopen node-id (1-127), stored in NVS
    uint8_t  canMotorAxis = 1;  // Local FocusMotor index dispatched on slave (0-3), stored in NVS

    // Convenience
    bool isMaster() const { return canRole == NodeRole::CAN_MASTER; }
    bool isSlave()  const { return canRole == NodeRole::CAN_SLAVE; }
    bool hasCan()   const { return canRole != NodeRole::STANDALONE; }
};

// Global instance — defined in NVSConfig.cpp
extern RuntimeConfig runtimeConfig;

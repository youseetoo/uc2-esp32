#pragma once
// T4.1: C (was C++). enum class NodeRole -> C enum; struct RuntimeConfig loses
// its in-class default initializers (now a designated initializer in NVSConfig.c)
// and its isMaster/isSlave/hasCan methods (now static inline accessors). The
// field set + names are unchanged so the NVS keys / JSON keys round-trip as before.
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum NodeRole {
    NODE_ROLE_STANDALONE = 0,   // No CAN — serial/WiFi only (existing UC2_2, UC2_3 boards)
    NODE_ROLE_CAN_MASTER = 1,   // CAN HAT — bridges serial to CAN bus
    NODE_ROLE_CAN_SLAVE  = 2    // Satellite — receives commands via CAN
} NodeRole;

typedef struct RuntimeConfig {
    // Module enables — defaults match the most common board (UC2_3)
    bool motor;
    bool laser;
    bool led;
    bool home;
    bool digitalIn;
    bool digitalOut;
    bool analogIn;
    bool analogOut;
    bool dac;
    bool tmc;
    bool encoder;
    bool joystick;
    bool galvo;
    bool bluetooth;
    bool wifi;
    bool pid;
    bool scanner;
    bool message;
    bool dial;
    bool objective;
    bool heat;
    bool fan; // MCP4017-controlled case fan + TMP102 sensors

    // CAN role
    NodeRole canRole;
    uint8_t  canNodeId;     // CANopen node-id (1-127), stored in NVS
    uint8_t  canMotorAxis;  // Local FocusMotor index dispatched on slave (0-3), stored in NVS
} RuntimeConfig;

// Global instance — defined in NVSConfig.c
extern RuntimeConfig runtimeConfig;

// Convenience accessors (C equivalents of the old C++ methods).
static inline bool runtime_config_is_master(const RuntimeConfig *c) { return c->canRole == NODE_ROLE_CAN_MASTER; }
static inline bool runtime_config_is_slave(const RuntimeConfig *c)  { return c->canRole == NODE_ROLE_CAN_SLAVE; }
static inline bool runtime_config_has_can(const RuntimeConfig *c)   { return c->canRole != NODE_ROLE_STANDALONE; }

#ifdef __cplusplus
}
#endif

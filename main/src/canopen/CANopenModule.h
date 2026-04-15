/**
 * CANopenModule.h
 *
 * CANopenNode integration for UC2 — replaces can_transport for CANopen builds.
 * Bridges the CANopenNode Object Dictionary to existing UC2 Module controllers.
 *
 * Only compiled when CAN_CONTROLLER_CANOPEN is defined in the build flags.
 */
#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "cJSON.h"

#ifdef CAN_CONTROLLER_CANOPEN

extern "C" {
#include <CANopen.h>
#include "OD.h"
}

// Cached state of a remote slave, populated by RPDO consumers on the master.
// Read by handleMotorGet / async serial notifications. Zero SDO traffic.
struct RemoteSlaveState {
    int32_t  motorPosition;     // last known position from TPDO
    uint8_t  motorStatus;       // bit 0 = running
    uint32_t lastUpdateMs;      // millis() when the cache was last written
    bool     seen;              // has this slave EVER pushed a TPDO?
};

class CANopenModule {
public:
    void setup();   // Init TWAI + CANopenNode + start FreeRTOS tasks
    void loop();    // NMT state monitoring, LED indication

    // SDO access (used by master's serial->CAN bridge)
    static bool writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                         uint8_t* data, size_t dataSize);
    static bool readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                        uint8_t* buf, size_t bufSize, size_t* readSize);

    // Typed SDO write helpers
    static bool writeSDO_u8 (uint8_t nodeId, uint16_t idx, uint8_t sub, uint8_t  v);
    static bool writeSDO_u16(uint8_t nodeId, uint16_t idx, uint8_t sub, uint16_t v);
    static bool writeSDO_u32(uint8_t nodeId, uint16_t idx, uint8_t sub, uint32_t v);
    static bool writeSDO_i32(uint8_t nodeId, uint16_t idx, uint8_t sub, int32_t  v);

    // SDO domain write for arbitrary-length data (segmented/block transfer)
    static bool writeSDODomain(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                               const uint8_t* data, size_t dataSize);

    static cJSON* get(cJSON* doc);
    static cJSON* act(cJSON* doc);

    static bool isOperational();

    // Remote slave cache — master only, populated by RPDO consumers
    static constexpr uint8_t REMOTE_SLAVE_SLOTS = 4;
    static RemoteSlaveState s_remoteSlaves[REMOTE_SLAVE_SLOTS];

private:
    static void CAN_ctrl_task(void* arg);
    static void CO_main_task(void* arg);
    static void CO_tmr_task(void* arg);
    static void CO_interrupt_task(void* arg);

    // OD <-> Module sync (called from CO_tmr_task)
    static void syncRpdoToModules();
    static void syncRpdoToModules_slave();
    static void syncRpdoToModules_master();
    static void syncModulesToTpdo();
};

// Global CANopenNode object (same pattern as MWE)
extern CO_t* CO;

#endif // CAN_CONTROLLER_CANOPEN

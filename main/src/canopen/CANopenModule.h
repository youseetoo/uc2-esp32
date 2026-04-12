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

class CANopenModule {
public:
    void setup();   // Init TWAI + CANopenNode + start FreeRTOS tasks
    void loop();    // NMT state monitoring, LED indication

    // SDO access (used by master's serial->CAN bridge)
    static bool writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,  // CANopen Service Data Object (SDO) write helper
                         uint8_t* data, size_t dataSize);
    static bool readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,   // CANopen Service Data Object (SDO) read helper
                        uint8_t* buf, size_t bufSize, size_t* readSize);

    // Typed SDO write helpers — avoid (uint8_t*, size_t) boilerplate at call sites
    static bool writeSDO_u8 (uint8_t nodeId, uint16_t idx, uint8_t sub, uint8_t  v);
    static bool writeSDO_u16(uint8_t nodeId, uint16_t idx, uint8_t sub, uint16_t v);
    static bool writeSDO_u32(uint8_t nodeId, uint16_t idx, uint8_t sub, uint32_t v);
    static bool writeSDO_i32(uint8_t nodeId, uint16_t idx, uint8_t sub, int32_t  v);

    // /can_get and /can_act endpoints for CANopen builds
    // {"task":"/can_get"} → returns node ID, NMT state, TWAI status
    // {"task":"/can_act", "nodeId": 10} → sets this node's ID (persisted to NVS)
    static cJSON* get(cJSON* doc);
    static cJSON* act(cJSON* doc);

    // Check if CANopen stack is running
    static bool isOperational();                // Useful for slaves to check if they can trust the OD state, and for master to check if we can send commands

private:
    static void CAN_ctrl_task(void* arg);       // TWAI controller task for sending/receiving CAN frames
    static void CO_main_task(void* arg);        // CANopenNode main task for processing CANopen logic
    static void CO_tmr_task(void* arg);         // CANopenNode timer task for SYNC, PDO processing
    static void CO_interrupt_task(void* arg);   // CANopenNode interrupt task for handling incoming CAN frames

    // OD <-> Module sync (slave only, called from CO_tmr_task)
    static void syncRpdoToModules();   // OD_RAM -> Module.act()
    static void syncModulesToTpdo();   // Module.get() -> OD_RAM
};

// Global CANopenNode object (same pattern as MWE)
extern CO_t* CO;

#endif // CAN_CONTROLLER_CANOPEN

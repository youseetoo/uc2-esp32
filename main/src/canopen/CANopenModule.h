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
#include "PinConfig.h"

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

    // SDO access (used by master's serial->CAN bridge).
    // The default timeoutMs (250 ms) covers the common case where the slave's
    // OD write handler is a simple in-memory store. For OD entries whose
    // handler does slow work (e.g. esp_ota_begin on the slave erases a flash
    // partition, ~400 ms), the caller must pass a larger timeout.
    static bool writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                         uint8_t* data, size_t dataSize,
                         uint32_t timeoutMs = 250);
    static bool readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                        uint8_t* buf, size_t bufSize, size_t* readSize);

    // Returns true if the slave at nodeId has been heard from recently
    // (TPDO/heartbeat). For non-motor peripherals always returns true if a
    // route exists. Used to gate fast-fail SDO writes.
    static bool isNodeReachable(uint8_t nodeId);
    // Block until isNodeReachable(nodeId) returns true or timeoutMs elapses.
    // Polls every 50 ms. Returns true on success, false on timeout.
    static bool waitForNodeReachable(uint8_t nodeId, uint32_t timeoutMs);

    // Typed SDO write helpers. timeoutMs defaults to 250 ms; pass a larger
    // value for SDOs whose write handler on the slave is slow.
    static bool writeSDO_u8 (uint8_t nodeId, uint16_t idx, uint8_t sub, uint8_t  v, uint32_t timeoutMs = 250);
    static bool writeSDO_u16(uint8_t nodeId, uint16_t idx, uint8_t sub, uint16_t v, uint32_t timeoutMs = 250);
    static bool writeSDO_u32(uint8_t nodeId, uint16_t idx, uint8_t sub, uint32_t v, uint32_t timeoutMs = 250);
    static bool writeSDO_i32(uint8_t nodeId, uint16_t idx, uint8_t sub, int32_t  v, uint32_t timeoutMs = 250);

    // SDO domain write for arbitrary-length data (segmented/block transfer)
    static bool writeSDODomain(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                               const uint8_t* data, size_t dataSize);

    // Streaming SDO download — for large transfers that don't fit in RAM
    // (e.g. OTA firmware images). The caller manages chunk buffering;
    // CANopenModule holds the SDO session open across multiple chunks.
    // The total size is known up front and passed to Begin; each Chunk call
    // appends data; End finalises and closes the session. Only one streaming
    // session can be active at a time. The s_sdoMutex is held for the entire
    // session (taken in Begin, released in End/Abort).
    static bool sdoDownloadBegin(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                                  uint32_t totalSize);
    static bool sdoDownloadChunk(const uint8_t* data, size_t count);
    static bool sdoDownloadEnd();
    static void sdoDownloadAbort();
    // True while a streaming SDO download is in progress.
    static bool sdoDownloadActive();

    static cJSON* get(cJSON* doc);
    static cJSON* act(cJSON* doc);

    static bool isOperational();

    // Remote slave cache — master only, populated by RPDO consumers
    static constexpr uint8_t REMOTE_SLAVE_SLOTS = 4;  // TODO: Is this motors only? We probably have more?!
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

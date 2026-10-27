/**
 * canopen_module_priv.h
 *
 * T4.4: private shared state for the split CANopenModule.cpp translation units
 * (canopen_module.cpp / canopen_sdo.c / canopen_tpdo.cpp / canopen_ota.cpp).
 *
 * The original single file kept these as file-static; after the split the ones
 * touched by more than one TU live here as externs. Definitions stay in
 * canopen_module.cpp (the `static` keyword was dropped on those lines).
 *
 * The shared module-state globals and the EMCY consumer are C-visible (the C
 * TUs — canopen_sdo.c / canopen_tpdo.c / canopen_ota.c — read them too). The
 * pending-command structs keep in-class initializers and stay C++-only.
 */
#pragma once

#ifdef CAN_CONTROLLER_CANOPEN

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#ifdef __cplusplus
extern "C" {
#endif
#include <CANopen.h>
#ifdef __cplusplus
}
#endif

// ============================================================================
// Shared module state — defined in canopen_module.cpp, used across TUs
// ============================================================================

// C linkage (matching the definitions in canopen_module.cpp) so the split C TUs
// reference the same symbols.
#ifdef __cplusplus
extern "C" {
#endif
// SDO client session mutex (streaming download holds it for the whole session;
// plain SDO writes take it per call).
extern SemaphoreHandle_t s_sdoMutex;

// Updated whenever the TWAI driver reports a BUS_ERROR alert (e.g. missing
// ACK because no other node is on the bus).  Used by canopen_is_node_reachable
// (sdo TU) to fast-fail SDO writes when the bus is unhealthy.
extern volatile uint32_t s_lastBusErrorMs;

// Per-node liveness timestamp, indexed by CANopen nodeId (1..127).
// Updated from the TWAI RX drain (module TU) on any frame whose source nodeId
// matches a known function code (TPDO/SDO-resp/heartbeat). Used by
// canopen_is_node_reachable (sdo TU), the RPDO sync (tpdo TU) and streaming
// (ota TU).
extern volatile uint32_t s_nodeLastSeenMs[128];

// Set to CO_RESET_COMM from any context to trigger a live node-ID reload
// without esp_restart(): act() (module TU) and the RPDO pending-reset path
// (tpdo TU). CO_main_task (module TU) checks it in its inner loop.
extern volatile CO_NMT_reset_cmd_t s_requestedReset;
#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __cplusplus
extern "C" {
#endif
// Master-side EMCY consumer (defined in canopen_sdo.c): registered via
// CO_EM_initCallbackRx in CANopenModule::setup().
void axisEmcyRxCallback(const uint16_t ident, const uint16_t errorCode,
                        const uint8_t errorRegister, const uint8_t errorBit,
                        const uint32_t infoCode);
#ifdef __cplusplus
}
#endif

// ============================================================================
// Pending commands — C-visible (tpdo TU posts, module TU consumes).
// ============================================================================
// The C tpdo TU (canopen_tpdo.c) writes these, so they need C types + C
// linkage: the array definitions in canopen_module.cpp inherit C linkage from
// the extern "C" declarations below (same pattern as the SDO globals). The
// in-class initializers are dropped — every field is written by the tpdo TU
// before `pending` is set, and `loop()` only reads a field when `pending` is
// true, so the old defaults were dead. Array definitions stay in
// canopen_module.cpp (zero-initialized as static storage).
// ============================================================================
#define REMOTE_SLAVE_SLOTS 4

#ifdef __cplusplus
extern "C" {
#endif
struct PendingAxisCmd {
    volatile bool pending;
    int32_t pos;
    int32_t speed;
    int32_t accel;
    bool    isAbs;
    bool    isStop;
    bool    isForever;
};
#ifdef MOTOR_CONTROLLER  // T1.3: only the motor dispatch path touches this
extern struct PendingAxisCmd s_axisCmds[4];
#endif

struct PendingHomingCmd {
    volatile bool pending;
    int32_t speed;
    int8_t  direction;
    int32_t timeout;
    int32_t endstopRelease;
    uint8_t polarity;
};
#ifdef HOME_MOTOR  // T1.3: only the homing dispatch path touches this
extern struct PendingHomingCmd s_homingCmds[4];
#endif

struct PendingLaserCmd {
    volatile bool pending;
    uint16_t      value;
};
extern struct PendingLaserCmd s_laserCmds[4];

struct PendingHardLimitCmd {
    volatile bool pending;
    bool          isCommand;
    bool          isConfig;
    uint8_t       enabled;
    uint8_t       polarity;
};
#ifdef MOTOR_CONTROLLER  // T1.3: hard limits belong to the motor path
extern struct PendingHardLimitCmd s_hardlimitCmds[4];
#endif

// Remote slave state — posted by syncModulesToTpdo (tpdo TU, C), consumed by
// loop() (module TU, C++). Moved here from CANopenModule.h so the C tpdo TU
// sees it (it was C++-only before); the typedef keeps the unqualified
// `RemoteSlaveState` name valid in both C and C++.
struct RemoteSlaveState {
    int32_t  motorPosition;
    uint8_t  motorStatus;
    uint8_t  homingStatus;
    uint32_t lastUpdateMs;
    bool     seen;
};
typedef struct RemoteSlaveState RemoteSlaveState;
extern struct RemoteSlaveState s_remoteSlaves[REMOTE_SLAVE_SLOTS];

// tpdo TU public C API (called via inline shims from CO_tmr_task, module TU).
void canopen_tpdo_sync_rpdo_to_modules(void);
void canopen_tpdo_sync_modules_to_tpdo(void);
#ifdef __cplusplus
}  // extern "C"
#endif
#endif // CAN_CONTROLLER_CANOPEN

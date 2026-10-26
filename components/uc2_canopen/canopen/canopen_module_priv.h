/**
 * canopen_module_priv.h
 *
 * T4.4: private shared state for the split CANopenModule.cpp translation units
 * (canopen_module.cpp / canopen_sdo.cpp / canopen_tpdo.cpp / canopen_ota.cpp).
 *
 * The original single file kept these as file-static; after the split the ones
 * touched by more than one TU live here as externs. Definitions stay in
 * canopen_module.cpp (the `static` keyword was dropped on those lines).
 *
 * C++-only (structs with in-class initializers); all four TUs are .cpp.
 */
#pragma once

#ifdef CAN_CONTROLLER_CANOPEN
#ifdef __cplusplus

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
extern "C" {
#include <CANopen.h>
}

// ============================================================================
// Shared module state — defined in canopen_module.cpp, used across TUs
// ============================================================================

// SDO client session mutex (streaming download holds it for the whole session;
// plain SDO writes take it per call).
extern SemaphoreHandle_t s_sdoMutex;

// Updated whenever the TWAI driver reports a BUS_ERROR alert (e.g. missing
// ACK because no other node is on the bus).  Used by isNodeReachable() (sdo
// TU) to fast-fail SDO writes when the bus is unhealthy.
extern volatile uint32_t s_lastBusErrorMs;

// Per-node liveness timestamp, indexed by CANopen nodeId (1..127).
// Updated from the TWAI RX drain (module TU) on any frame whose source nodeId
// matches a known function code (TPDO/SDO-resp/heartbeat). Used by
// isNodeReachable (sdo TU), the RPDO sync (tpdo TU) and streaming (ota TU).
extern volatile uint32_t s_nodeLastSeenMs[128];

// Set to CO_RESET_COMM from any context to trigger a live node-ID reload
// without esp_restart(): act() (module TU) and the RPDO pending-reset path
// (tpdo TU). CO_main_task (module TU) checks it in its inner loop.
extern volatile CO_NMT_reset_cmd_t s_requestedReset;

// Master-side EMCY consumer (defined in canopen_sdo.cpp): registered via
// CO_EM_initCallbackRx in CANopenModule::setup().
void axisEmcyRxCallback(const uint16_t ident, const uint16_t errorCode,
                        const uint8_t errorRegister, const uint8_t errorBit,
                        const uint32_t infoCode);

// ============================================================================
// Pending commands — posted by CO_tmr_task (tpdo TU), consumed by loop()
// (main context, module TU). Per-axis arrays for motors; separate structs for
// homing and lasers.
// Writing 'pending = true' last acts as a commit flag (single-byte assignment
// is atomic on Xtensa, so no dedicated mutex is needed for this simple
// producer-consumer pattern).
// Array definitions stay in canopen_module.cpp.
// ============================================================================
struct PendingAxisCmd {
    volatile bool pending = false;
    int32_t pos   = 0;
    int32_t speed = 0;
    int32_t accel = 0;
    bool    isAbs     = false;
    bool    isStop    = false;
    bool    isForever = false;
};
#ifdef MOTOR_CONTROLLER  // T1.3: only the motor dispatch path touches this
extern PendingAxisCmd s_axisCmds[4];
#endif

struct PendingHomingCmd {
    volatile bool pending  = false;
    int32_t speed          = 0;
    int8_t  direction      = 1;
    int32_t timeout        = 5000;
    int32_t endstopRelease = 0;
    uint8_t polarity       = 0;
};
#ifdef HOME_MOTOR  // T1.3: only the homing dispatch path touches this
extern PendingHomingCmd s_homingCmds[4];
#endif

struct PendingLaserCmd {
    volatile bool pending = false;
    uint16_t      value   = 0;
};
extern PendingLaserCmd s_laserCmds[4];

struct PendingHardLimitCmd {
    volatile bool pending  = false;
    bool          isCommand = false;   // true  -> run command (clear)
    bool          isConfig  = false;   // true  -> update enabled/polarity
    uint8_t       enabled  = 0;
    uint8_t       polarity = 0;
};
#ifdef MOTOR_CONTROLLER  // T1.3: hard limits belong to the motor path
extern PendingHardLimitCmd s_hardlimitCmds[4];
#endif

#endif // __cplusplus
#endif // CAN_CONTROLLER_CANOPEN

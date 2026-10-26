/**
 * canopen_sdo.cpp
 * T4.4: split from CANopenModule.cpp (behavior-identical). SDO client helpers, node reachability, typed SDO wrappers, axis EMCY map.
 */

#include "CANopenModule.h"


#include "PinConfig.h"
#ifdef CAN_CONTROLLER_CANOPEN

#include "RuntimeConfig.h"
#include "NVSConfig.h"
#include "RoutingTable.h"
// Named OD constants — generated from tools/canopen/uc2_canopen_registry.yaml.
// Constants are declared here for compile-time verification; not yet used in logic.
#include "UC2_OD_Indices.h"

// SerialProcess is needed by the GPIO-slave TPDO2 sniffer (master-only) as
// well as the motor-completion path below, so it's hoisted out of the
// MOTOR_CONTROLLER block.
#include "SerialProcess.h"
#ifdef MOTOR_CONTROLLER
#include "FocusMotor.h"
#include "QidRegistry.h"
#endif
#ifdef HOME_MOTOR
#include "HomeMotor.h"
#endif
#ifdef LASER_CONTROLLER
#include "LaserController.h"
#endif
#ifdef LED_CONTROLLER
#include "LedController.h"
#endif
#ifdef GALVO_CONTROLLER
#include "GalvoController.h"
#endif
#ifdef TMC_CONTROLLER
#include "TMCController.h"
#endif
// Axis fault enum (header-only; safe to include on any CAN build) for the
// EMCY code map used by emitAxisEmcy / the master EMCY consumer.
#include "AxisTypes.h"
#ifdef AXIS_CONTROLLER
#include "AxisController.h"
#endif

#include "CanOpenOTA.h"

extern "C" {
#include <CANopen.h>
#include "OD.h"
}

#ifdef UNIT_TEST
// Host-test routing: SDO write/read and node-reachability checks go through
// these C shims, provided by the mock (test/unity/mocks/mock_canopen.cpp).
// The production ESP32 build never defines UNIT_TEST, so the real paths
// (CO->SDOclient, s_nodeLastSeenMs liveness) are untouched.
extern "C" {
    int  CO_writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                     uint8_t* data, size_t dataSize, int timeoutMs);
    int  CO_readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                    uint8_t* data, size_t dataSize, size_t* bytesRead,
                    int timeoutMs);
    bool CO_nodeReachable(uint8_t nodeId);
}
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"   // esp_read_mac() — populate 0x2509 MAC + 0x1018 serialNumber
#include <strings.h>   // strcasecmp() — case-insensitive MAC compare in act()
#include "driver/gpio.h"
#include "driver/twai.h"

// ============================================================================
// Constants
// ============================================================================
#define TAG_CAN    "UC2_CAN"
#define TAG_CO     "UC2_CO"
// T1.3: uc2_log.h façades (log_i/log_e/log_w/log_d) need a file-local TAG.
// TAG_CO is the dominant tag in this file; TAG_CAN is kept for the raw
// ESP_LOG* calls in the TWAI layer.
#define TAG TAG_CO

// Task priorities (same scheme as MWE trainer)
// TWAI_ctrl is the I/O pump between CAN_TX/RX_queue and the hardware. It
// blocks on driver alerts and only runs when a frame completed or arrived,
// so it must outrank the tasks that feed it — otherwise a queued frame
// waits for the next scheduler tick instead of going out back-to-back.
#define CAN_CTRL_TASK_PRIO          5
#define CANOPEN_TASK_PRIO           3
#define CANOPEN_TMR_TASK_PRIO       4
#define CANOPEN_INT_TASK_PRIO       2

// NMT control flags.
// Boot to OPERATIONAL and STAY there. We deliberately do NOT set
// CO_NMT_ERR_ON_ERR_REG (nor CO_NMT_ERR_ON_BUSOFF_HB): a UC2 node is routinely
// alone on the bus (master powered before its slaves, a slave unplugged, ...)
// with no peer to ACK its frames, so the CAN communication error bit sets.
// Tying NMT state to that demoted the node to PRE-OPERATIONAL, where it stopped
// producing PDOs/heartbeats; and because the error never clears while alone it
// got stuck there and stayed unreachable even after the peer returned (the
// "long disconnect can't recover" symptom). Bus-off is still recovered at the
// TWAI hardware layer (CAN_ctrl_task), so a node that stays OPERATIONAL simply
// resumes talking the instant the peer reappears — no NMT round-trip needed.
#define NMT_CONTROL CO_NMT_STARTUP_TO_OPERATIONAL
#define FIRST_HB_TIME           500
// SDO server timeout: must be generous for block download OTA.
// The slave's SDO server aborts with 0x05040000 if it doesn't receive
// the next expected frame within this period.  1000ms was too tight —
// the master needs time to process the initiate response and begin
// sending sub-blocks (CAN_ctrl_task drain rate + task scheduling jitter).
#define SDO_SRV_TIMEOUT_TIME   5000
#define SDO_CLI_TIMEOUT_TIME   5000
#define SDO_CLI_BLOCK          false
#define OD_STATUS_BITS         NULL

#include "canopen_module_priv.h"

// ============================================================================
// SDO helpers (reused from MWE trainer)
// ============================================================================
static CO_SDO_abortCode_t _read_SDO(CO_SDOclient_t* SDO_C, uint8_t nodeId,
    uint16_t index, uint8_t subIndex,
    uint8_t* buf, size_t bufSize, size_t* readSize,
    uint32_t timeoutMs = 100)
{
    /*
    This function handles SDO (Service Data Object) read operations by setting 
    up the SDO client, initiating the upload, and reading the data into the 
    provided buffer.
    */
    CO_SDO_return_t SDO_ret;

    SDO_ret = CO_SDOclient_setup(SDO_C,
        CO_CAN_ID_SDO_CLI + nodeId,
        CO_CAN_ID_SDO_SRV + nodeId, nodeId);
    if (SDO_ret != CO_SDO_RT_ok_communicationEnd) return CO_SDO_AB_GENERAL;

    SDO_ret = CO_SDOclientUploadInitiate(SDO_C, index, subIndex, timeoutMs, false);
    if (SDO_ret != CO_SDO_RT_ok_communicationEnd) return CO_SDO_AB_GENERAL;

    do {
        CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;
        SDO_ret = CO_SDOclientUpload(SDO_C, 1000, false,
            &abortCode, NULL, NULL, NULL);
        if (SDO_ret < 0) {
            CO_SDOclientClose(SDO_C);
            log_e("Read abort: %d", SDO_ret);
            return abortCode;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (SDO_ret > 0);

    *readSize = CO_SDOclientUploadBufRead(SDO_C, buf, bufSize);
    CO_SDOclientClose(SDO_C);
    return CO_SDO_AB_NONE;
}

// Default SDO write timeout. Bumped from 100ms because the slave's CO_tmr_task
// can take >100ms to respond when it is busy dispatching (e.g. JSON parsing,
// LedController FastLED show, FocusMotor stepping). 100ms produced spurious
// "Laser SDO failed" logs even though OD_RAM was actually updated and
// syncRpdoToModules_slave dispatched the value to the local controller.
static CO_SDO_abortCode_t _write_SDO(CO_SDOclient_t* SDO_C, uint8_t nodeId,
    uint16_t index, uint8_t subIndex,
    uint8_t* data, size_t dataSize,
    uint32_t timeoutMs = 250)
{
    /*
    This function handles SDO (Service Data Object) write operations by setting 
    up the SDO client, initiating the download, and writing the data from the 
    provided buffer.
    */
    CO_SDO_return_t SDO_ret;

    SDO_ret = CO_SDOclient_setup(SDO_C,
        CO_CAN_ID_SDO_CLI + nodeId,
        CO_CAN_ID_SDO_SRV + nodeId, nodeId);
    if (SDO_ret != CO_SDO_RT_ok_communicationEnd) return CO_SDO_AB_NO_DATA;

    SDO_ret = CO_SDOclientDownloadInitiate(SDO_C, index, subIndex, dataSize, timeoutMs, false);
    if (SDO_ret != CO_SDO_RT_ok_communicationEnd) return CO_SDO_AB_NO_DATA;

    CO_SDOclientDownloadBufWrite(SDO_C, data, dataSize);

    do {
        CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;
        SDO_ret = CO_SDOclientDownload(SDO_C, 1000, false, false,
            &abortCode, NULL, NULL);
        if (SDO_ret < 0) {
            log_e("Write abort: %d", SDO_ret);
            
            return abortCode;
        }
        // SDO_ret constants:
        // >0: in progress (waiting for response)
        // 0: completed successfully
        // <0: error
        // We Download the information about the SDO transfer in a loop until 
        // it's complete, with a small delay to allow the CANopen stack to 
        // process incoming messages (e.g. SDO responses).
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (SDO_ret > 0);

    CO_SDOclientClose(SDO_C);
    return CO_SDO_AB_NONE;
}

// ============================================================================
// Public SDO API
// ============================================================================

// Returns true if the slave at nodeId has been heard from on the CAN bus
// within the last 5 seconds (any TPDO/SDO-response/heartbeat frame).
// Uniform across all peripheral types — replaces the old motor-only TPDO
// check + permissive "always true if NMT operational" branch that caused
// 500 ms SDO timeouts against absent laser/LED/galvo nodes during boot.
bool CANopenModule::isNodeReachable(uint8_t nodeId)
{
#ifdef UNIT_TEST
    return CO_nodeReachable(nodeId);
#endif
    if (nodeId < 1 || nodeId > 127) return false;

    if (CO == NULL || CO->NMT == NULL ||
        CO->NMT->operatingState != CO_NMT_OPERATIONAL) {
        return false;
    }

    // Recent bus errors mean the slave likely isn't ACKing; skip the SDO
    // and let the caller fall back instead of stalling for the full timeout.
    if (s_lastBusErrorMs != 0 && (millis() - s_lastBusErrorMs) < 1000) {
        return false;
    }

    uint32_t lastSeen = s_nodeLastSeenMs[nodeId];
    if (lastSeen == 0) return false;
    return (millis() - lastSeen) < 5000;
}

// Gate for SDO *command* attempts — deliberately more permissive than
// isNodeReachable():
//   * SDO is a valid service in PRE-OPERATIONAL as well as OPERATIONAL, so we
//     don't refuse just because the master briefly dropped to pre-op.
//   * UC2 slaves emit only a CANopen boot-up frame plus on-demand TPDOs (no
//     periodic heartbeat), so a node that booted a while ago looks "stale" to
//     isNodeReachable() yet is physically present and will answer. We therefore
//     attempt the SDO for any node that has EVER announced itself; the transfer
//     itself is the real liveness test. writeSDO()/readSDO() reset a node to
//     "absent" when it fails to respond at all, so a genuinely removed node
//     fast-fails on the next command instead of stalling every time.
static bool canReachForSDO(uint8_t nodeId)
{
    if (nodeId < 1 || nodeId > 127) return false;
    if (CO == NULL || CO->NMT == NULL || CO->SDOclient == NULL) return false;
    uint8_t st = CO->NMT->operatingState;
    if (st != CO_NMT_OPERATIONAL && st != CO_NMT_PRE_OPERATIONAL) return false;
    // Recent bus errors mean nobody is ACKing — let the caller fall back
    // instead of stalling for the full SDO timeout.
    if (s_lastBusErrorMs != 0 && (millis() - s_lastBusErrorMs) < 1000) return false;
    return s_nodeLastSeenMs[nodeId] != 0; // ever announced itself (boot-up/HB/TPDO/SDO-resp)
}

// Poll isNodeReachable() until it returns true or timeoutMs elapses.
// Used by OTA: after a slave reboot it takes ~1 s before the master sees
// the first TPDO/heartbeat, and we don't want the very first SDO write
// (CRC32 / size) to fail just because we asked too early.
bool CANopenModule::waitForNodeReachable(uint8_t nodeId, uint32_t timeoutMs)
{
#ifdef UNIT_TEST
    return CO_nodeReachable(nodeId);
#endif
    const uint32_t deadline = millis() + timeoutMs;
    while (millis() < deadline) {
        if (isNodeReachable(nodeId)) return true;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return isNodeReachable(nodeId);
}

bool CANopenModule::writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                             uint8_t* data, size_t dataSize,
                             uint32_t timeoutMs)
{
#ifdef UNIT_TEST
    return CO_writeSDO(nodeId, index, subIndex, data, dataSize,
                       (int)timeoutMs) == 0;
#endif
    /*
        writeSDO: This function writes data to a specified SDO (Service Data Object) 
        on a remote node in the CANopen network. It checks if the SDO client is 
        initialized, verifies if the target node is reachable, and then performs 
        the SDO write operation while handling potential errors and timeouts.
    */
    if (CO == NULL || CO->SDOclient == NULL) {
        log_e("SDO client not initialized");
        return false;
    }   
    // Fast-fail only for nodes that have never announced themselves; a node
    // that booted at least once is attempted even if currently quiet (see
    // canReachForSDO — slaves here have no periodic heartbeat).
    if (!canReachForSDO(nodeId)) {
        log_e("writeSDO: node 0x%02X not reachable, skipping idx=0x%04X sub=0x%02X", nodeId, index, subIndex);
        return false;
    }
    if (s_sdoMutex && xSemaphoreTake(s_sdoMutex, pdMS_TO_TICKS(200)) != pdTRUE)
    {
        log_i("writeSDO: node 0x%02X idx=0x%04X sub=0x%02X failed to take SDO mutex", nodeId, index, subIndex);
        return false;
    }
    uint32_t seenBefore = s_nodeLastSeenMs[nodeId];
    CO_SDO_abortCode_t ret = _write_SDO(CO->SDOclient, nodeId,
        index, subIndex, data, dataSize, timeoutMs);
    if (s_sdoMutex) {
        xSemaphoreGive(s_sdoMutex);
        log_i("writeSDO: node 0x%02X idx=0x%04X sub=0x%02X write %u bytes, abort=0x%08lX",
              nodeId, index, subIndex, (unsigned)dataSize, (unsigned long)ret);
    }
    if (ret != CO_SDO_AB_NONE) {
        log_e("writeSDO: node 0x%02X idx=0x%04X sub=0x%02X failed, abort=0x%08lX", nodeId, index, subIndex, (unsigned long)ret);
        // If no response frame arrived during the whole transfer (s_nodeLastSeenMs
        // unchanged), the node is gone — mark it absent so the next command
        // fast-fails instead of stalling for the full timeout again. A protocol
        // abort (node answered) leaves liveness refreshed, so we keep it.
        if (s_nodeLastSeenMs[nodeId] == seenBefore)
            s_nodeLastSeenMs[nodeId] = 0;
    }
    return (ret == CO_SDO_AB_NONE);
}

bool CANopenModule::readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                            uint8_t* buf, size_t bufSize, size_t* readSize)
{
#ifdef UNIT_TEST
    return CO_readSDO(nodeId, index, subIndex, buf, bufSize, readSize,
                      1000) == 0;
#endif
    if (CO == NULL || CO->SDOclient == NULL) return false;
    if (!canReachForSDO(nodeId)) return false;
    if (s_sdoMutex && xSemaphoreTake(s_sdoMutex, pdMS_TO_TICKS(200)) != pdTRUE) return false;
    uint32_t seenBefore = s_nodeLastSeenMs[nodeId];
    CO_SDO_abortCode_t ret = _read_SDO(CO->SDOclient, nodeId,
        index, subIndex, buf, bufSize, readSize);
    if (s_sdoMutex) xSemaphoreGive(s_sdoMutex);
    // No response at all -> mark node absent so the next call fast-fails.
    if (ret != CO_SDO_AB_NONE && s_nodeLastSeenMs[nodeId] == seenBefore)
        s_nodeLastSeenMs[nodeId] = 0;
    return (ret == CO_SDO_AB_NONE);
}

bool CANopenModule::isOperational()
{
    return (CO != NULL && CO->NMT != NULL &&
            CO->NMT->operatingState == CO_NMT_OPERATIONAL);
}

// ── Axis feedback (design v2, WP5): EMCY emit + JSON conversion ──────────────
// Reverse of AxisNotify::emcyCodeForFault — turn a manufacturer EMCY code back
// into the fault name used in the {"axisEvent":...} JSON.
static const char *axisFaultNameFromEmcy(uint16_t code)
{
    switch (code)
    {
    case 0xFF01: return "STALL";
    case 0xFF02: return "LOST_STEPS";
    case 0xFF03: return "DIVERGENCE";
    case 0xFF04: return "TIMEOUT";
    case 0xFF05: return "CAL_INVALID";
    case 0xFF06: return "CAL_FAILED";
    case 0xFF07: return "ENC_NOISE";
    default:     return "NONE";
    }
}

// Local copy of the AxisFault -> EMCY code map (kept self-contained so this file
// links on CAN builds that don't compile the axis/ module, e.g. the master).
static uint16_t axisEmcyCodeFromFault(uint8_t fault)
{
    switch (fault)
    {
    case FAULT_STALL:       return 0xFF01;
    case FAULT_LOST_STEPS:  return 0xFF02;
    case FAULT_DIVERGENCE:  return 0xFF03;
    case FAULT_TIMEOUT:     return 0xFF04;
    case FAULT_CAL_INVALID: return 0xFF05;
    case FAULT_CAL_FAILED:  return 0xFF06;
    case FAULT_ENC_NOISE:   return 0xFF07;
    default:                return 0xFF00;
    }
}

void CANopenModule::emitAxisEmcy(uint8_t axis, uint8_t fault, int32_t posErrSteps)
{
    if (CO == NULL || CO->em == NULL)
        return;
    uint16_t code = axisEmcyCodeFromFault(fault);
    // Encode the axis into a manufacturer-specific error status bit (0x30+).
    uint8_t errorBit = (uint8_t)(CO_EM_MANUFACTURER_START + (axis & 0x0F));
    CO_error(CO->em, true, errorBit, code, (uint32_t)posErrSteps);
}

#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
// Master-side EMCY consumer: convert an inbound axis-fault EMCY into the same
// {"axisEvent":{...}} JSON the standalone path emits. Runs in CO stack context
// — keep it short and non-blocking.
// T4.4: shared across the split TUs — extern in canopen_module_priv.h
// (registered from CANopenModule::setup() in canopen_module.cpp).
void axisEmcyRxCallback(const uint16_t ident, const uint16_t errorCode,
                        const uint8_t errorRegister, const uint8_t errorBit,
                        const uint32_t infoCode)
{
    (void)errorRegister;
    // Only our manufacturer axis codes.
    if (errorCode < 0xFF01 || errorCode > 0xFF07)
        return;
    uint8_t node = (uint8_t)(ident & 0x7F); // EMCY COB-ID = 0x80 + node-id
    uint8_t axis = (errorBit >= CO_EM_MANUFACTURER_START)
                       ? (uint8_t)(errorBit - CO_EM_MANUFACTURER_START)
                       : errorBit;
    int32_t posErr = (int32_t)infoCode;

    cJSON *root = cJSON_CreateObject();
    cJSON *ev = cJSON_AddObjectToObject(root, "axisEvent");
    cJSON_AddNumberToObject(ev, "node", node);
    cJSON_AddNumberToObject(ev, "axis", axis);
    cJSON_AddStringToObject(ev, "fault", axisFaultNameFromEmcy(errorCode));
    cJSON_AddNumberToObject(ev, "posErrSteps", posErr);
    cJSON_AddNumberToObject(ev, "event", 1);
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        serial_out_safe_send_json_string(json);
        free(json);
    }
    cJSON_Delete(root);
}
#endif

// Typed SDO write helpers
bool CANopenModule::writeSDO_u8(uint8_t nodeId, uint16_t idx, uint8_t sub, uint8_t v, uint32_t timeoutMs) {
    return writeSDO(nodeId, idx, sub, &v, sizeof(v), timeoutMs);
}
bool CANopenModule::writeSDO_u16(uint8_t nodeId, uint16_t idx, uint8_t sub, uint16_t v, uint32_t timeoutMs) {
    return writeSDO(nodeId, idx, sub, (uint8_t*)&v, sizeof(v), timeoutMs);
}
bool CANopenModule::writeSDO_u32(uint8_t nodeId, uint16_t idx, uint8_t sub, uint32_t v, uint32_t timeoutMs) {
    return writeSDO(nodeId, idx, sub, (uint8_t*)&v, sizeof(v), timeoutMs);
}
bool CANopenModule::writeSDO_i32(uint8_t nodeId, uint16_t idx, uint8_t sub, int32_t v, uint32_t timeoutMs) {
    return writeSDO(nodeId, idx, sub, (uint8_t*)&v, sizeof(v), timeoutMs);
}

// SDO domain write — handles segmented transfer for arbitrary-length data.
// Uses the same _write_SDO helper which calls CO_SDOclientDownloadInitiate +
// CO_SDOclientDownloadBufWrite + CO_SDOclientDownload loop.  CANopenNode v4
// automatically switches to segmented mode when dataSize > 4 bytes.
bool CANopenModule::writeSDODomain(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                                   const uint8_t* data, size_t dataSize)
{
    if (CO == NULL || CO->SDOclient == NULL) {
        log_e("SDO client not initialized");
        return false;
    }
    // NOTE: intentionally NO isNodeReachable() check here. Domain transfers
    // (e.g. OTA firmware ~1 MB) take minutes; the slave stops pushing TPDOs
    // while busy writing flash, so its "seen recently" timestamp would go
    // stale and abort an in-flight transfer. The 2 s per-transaction SDO
    // timeout below already handles unresponsive nodes gracefully.
    if (s_sdoMutex && xSemaphoreTake(s_sdoMutex, pdMS_TO_TICKS(5000)) != pdTRUE) return false;

    log_i("Writing SDO domain: node 0x%02X idx 0x%04X sub 0x%02X size %u",
          nodeId, index, subIndex, (unsigned)dataSize);
    CO_SDO_abortCode_t ret = _write_SDO(CO->SDOclient, nodeId,
        index, subIndex, (uint8_t*)data, dataSize, /*timeoutMs=*/2000);

    if (s_sdoMutex) xSemaphoreGive(s_sdoMutex);
    if (ret != CO_SDO_AB_NONE) {
        log_e("SDO domain write failed: node 0x%02X idx 0x%04X abort=0x%08lX",
              nodeId, index, (unsigned long)ret);
    }
    return (ret == CO_SDO_AB_NONE);
}

#endif // CAN_CONTROLLER_CANOPEN

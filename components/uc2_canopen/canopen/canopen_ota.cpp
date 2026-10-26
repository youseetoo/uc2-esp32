/**
 * canopen_ota.cpp
 * T4.4: split from CANopenModule.cpp (behavior-identical). Streaming (chunked) SDO download for OTA.
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
// Streaming SDO download — chunked transfer for large blobs (e.g. OTA)
// ============================================================================
//
// CANopenNode v4 supports refilling the SDO client FIFO between calls by
// passing bufferPartial=true. This lets us stream a multi-MB firmware image
// to a remote node in small chunks (4 KB), keeping the master's RAM
// footprint constant and independent of the total transfer size.
//
// The state machine of CO_SDOclient is driven exclusively by
// CO_SDOclientDownload(); it is NOT ticked by CO_process. Therefore we
// must keep calling it from this module until the chunk's data is fully
// queued and the client is parked in CO_SDO_RT_waitingResponse (i.e. the
// FIFO is empty and the next segment ACK from the server is awaited).
// ============================================================================

static bool                s_sdoStreamActive       = false;
static CO_SDOclient_t*     s_sdoStreamClient       = nullptr;
static uint32_t            s_sdoStreamTotalSize    = 0;
static uint32_t            s_sdoStreamBytesQueued  = 0;
static uint64_t            s_sdoStreamLastUs       = 0;
static uint32_t            s_sdoStreamLastLoggedKB = 0;

// Maximum time we will spin inside a single chunk call before yielding back
// to the caller, even if the FIFO is not yet drained. Prevents the WDT from
// firing on extremely slow links. The state machine resumes on the next
// chunk call (data still queued in FIFO).
static constexpr uint32_t SDO_STREAM_CHUNK_BUDGET_MS = 10000;
// Per-call SDO timeout (passed to CO_SDOclientDownloadInitiate). Must be
// long enough to cover the slave's flash-write latency.
static constexpr uint16_t SDO_STREAM_TIMEOUT_MS      = 15000;

bool CANopenModule::sdoDownloadActive() { return s_sdoStreamActive; }

// Drive the SDO client state machine once per caller tick.
//
// In block download CO_SDOclientDownload() sends exactly ONE 7-byte segment
// per call and returns blockDownldInProgress with timerNext_us=0, i.e.
// "call me again immediately". Calling it once and sleeping a tick capped
// the transfer at 1 frame/ms (~7 KB/s). So burst: keep calling while it
// keeps sending, until it parks (waiting for the server's block ACK, TX
// queue full, FIFO < 7 bytes and needs a refill, or error).
//
// The burst is bounded by the CAN_TX_queue depth (32): past that CO_CANsend
// marks the buffer full and the client returns transmittBufferFull anyway.
// The "no progress" check catches the FIFO-needs-refill case, where the
// client returns blockDownldInProgress without sending.
static CO_SDO_return_t sdoStreamPump(bool bufferPartial,
                                     CO_SDO_abortCode_t* abortCode)
{
    uint64_t now  = esp_timer_get_time();
    uint32_t dtUs = (uint32_t)(now - s_sdoStreamLastUs);
    s_sdoStreamLastUs = now;
    // Clamp: a huge dt (first call) would advance internal timers too far.
    if (dtUs > 100000U) dtUs = 100000U;
    if (dtUs == 0)      dtUs = 1;

    CO_SDO_return_t r = CO_SDO_RT_waitingResponse;
    size_t sentBefore = 0, sentAfter = 0;
    for (int burst = 0; burst < 32; ++burst) {
        r = CO_SDOclientDownload(s_sdoStreamClient, dtUs, /*abort=*/false,
                                 bufferPartial, abortCode, &sentAfter, NULL);
        if (r != CO_SDO_RT_blockDownldInProgress || sentAfter == sentBefore)
            break;
        sentBefore = sentAfter;
        dtUs = 1;
    }
    return r;
}

bool CANopenModule::sdoDownloadBegin(uint8_t nodeId, uint16_t index,
                                     uint8_t subIndex, uint32_t totalSize)
{
    if (s_sdoStreamActive) {
        log_e("sdoDownloadBegin: another stream already active");
        return false;
    }
    if (CO == NULL || CO->SDOclient == NULL) {
        log_e("sdoDownloadBegin: SDO client not initialised");
        return false;
    }
    if (totalSize == 0) {
        log_e("sdoDownloadBegin: totalSize=0");
        return false;
    }

    // Hold the SDO mutex for the entire streaming session so other writes do
    // not interleave segments. Use a longer wait than the regular helpers.
    if (s_sdoMutex && xSemaphoreTake(s_sdoMutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        log_e("sdoDownloadBegin: failed to take SDO mutex");
        return false;
    }

    s_sdoStreamClient = CO->SDOclient;
    CO_SDO_return_t r = CO_SDOclient_setup(s_sdoStreamClient,
        CO_CAN_ID_SDO_CLI + nodeId,
        CO_CAN_ID_SDO_SRV + nodeId, nodeId);
    if (r != CO_SDO_RT_ok_communicationEnd) {
        log_e("sdoDownloadBegin: CO_SDOclient_setup ret=%d", r);
        if (s_sdoMutex) xSemaphoreGive(s_sdoMutex);
        s_sdoStreamClient = nullptr;
        return false;
    }

    // Initiate with the full transfer size so the slave can call
    // esp_ota_begin(totalSize) before any data arrives.
    // Last argument enables BLOCK transfer (127 segments per ACK instead
    // of one ACK per 7-byte segment) — critical for OTA throughput.
    r = CO_SDOclientDownloadInitiate(s_sdoStreamClient, index, subIndex,
                                     totalSize, SDO_STREAM_TIMEOUT_MS, true);
    if (r != CO_SDO_RT_ok_communicationEnd) {
        log_e("sdoDownloadBegin: CO_SDOclientDownloadInitiate ret=%d", r);
        if (s_sdoMutex) xSemaphoreGive(s_sdoMutex);
        s_sdoStreamClient = nullptr;
        return false;
    }

    s_sdoStreamTotalSize    = totalSize;
    s_sdoStreamBytesQueued  = 0;
    s_sdoStreamLastUs       = esp_timer_get_time();
    s_sdoStreamLastLoggedKB = 0;
    s_sdoStreamActive       = true;

    log_i("sdoDownloadBegin: node 0x%02X idx 0x%04X sub 0x%02X total=%u",
          nodeId, index, subIndex, (unsigned)totalSize);
    return true;
}

bool CANopenModule::sdoDownloadChunk(const uint8_t* data, size_t count)
{
    if (!s_sdoStreamActive || s_sdoStreamClient == nullptr) {
        log_e("sdoDownloadChunk: no active stream");
        return false;
    }
    if (data == nullptr || count == 0) 
    {
        log_e("sdoDownloadChunk: invalid data/count");
        return true;
    }

    // bufferPartial is true while more chunks may follow. The very last
    // byte of the very last chunk is finalised in sdoDownloadEnd() with
    // bufferPartial=false, so we always pass true here.
    const bool bufferPartial = true;

    size_t   offset       = 0;
    uint32_t deadlineMs   = (uint32_t)(esp_timer_get_time() / 1000ULL)
                            + SDO_STREAM_CHUNK_BUDGET_MS;

    while (offset < count) {
        // Refill the FIFO with whatever fits.
        size_t nWritten = CO_SDOclientDownloadBufWrite(s_sdoStreamClient,
                                                       data + offset,
                                                       count - offset);
        offset += nWritten;

        // Drive the state machine to drain over CAN.
        CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;
        CO_SDO_return_t r = sdoStreamPump(bufferPartial, &abortCode);
        if (r < 0) {
            log_e("sdoDownloadChunk(fill): node 0x%02X ret=%d abort=0x%08lX queued %u/%u of chunk (totalQueued=%u)",
                  s_sdoStreamClient ? (unsigned)s_sdoStreamClient->nodeIDOfTheSDOServer : 0u,
                  r, (unsigned long)abortCode, // ABORTCODE: -10 = endedWithServerAbort
                  (unsigned)offset, (unsigned)count,
                  (unsigned)s_sdoStreamBytesQueued);
            sdoDownloadAbort();
            return false;
        }

        // Watchdog: bail out if we have spent too long inside this call.
        // (Chunk is partially queued; caller will not retry — treat as fail.)
        uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000ULL);
        if ((int32_t)(nowMs - deadlineMs) > 0) {
            log_e("sdoDownloadChunk: budget %ums exceeded after %u/%u bytes (totalQueued=%u, last ret=%d)",
                  (unsigned)SDO_STREAM_CHUNK_BUDGET_MS,
                  (unsigned)offset, (unsigned)count,
                  (unsigned)s_sdoStreamBytesQueued, r);
            sdoDownloadAbort();
            return false;
        }

        // Yield UNCONDITIONALLY every iteration: the pump has either filled
        // the TX queue or is waiting on the slave, and running hot at
        // priority-1 (Arduino loopTask) would starve IDLE and trip its WDT.
        // TWAI_ctrl drains ~4 frames per tick at 500 kbit/s, so one tick is
        // exactly the refill cadence.
        vTaskDelay(1);
    }

    // All bytes from this chunk are now in the FIFO. Keep ticking the
    // state machine until it parks in waitingResponse (1) — i.e. the FIFO
    // has been drained over CAN and we are waiting on the server's ACK —
    // OR until it returns blockDownldInProgress (3), meaning the FIFO is
    // nearly empty (< 7 bytes) and the client wants a refill. Since this
    // is a chunk boundary, a refill will come from the next
    // sdoDownloadChunk() call or sdoDownloadEnd().
    for (;;) {
        CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;
        CO_SDO_return_t r = sdoStreamPump(bufferPartial, &abortCode);
        if (r < 0) {
            log_e("sdoDownloadChunk(drain): ret=%d abort=0x%08lX totalQueued=%u",
                  r, (unsigned long)abortCode,
                  (unsigned)s_sdoStreamBytesQueued);
            sdoDownloadAbort();
            return false;
        }
        // waitingResponse (1): waiting on server block ACK — chunk is done.
        if (r == CO_SDO_RT_waitingResponse) break;
        // blockDownldInProgress (3): FIFO < 7 bytes, client wants refill.
        // All data for this chunk has been transmitted; exit and let the
        // next sdoDownloadChunk() provide more bytes.
        if (r == CO_SDO_RT_blockDownldInProgress) break;

        uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000ULL);
        if ((int32_t)(nowMs - deadlineMs) > 0) {
            break;
        }
        vTaskDelay(1);
    }

    s_sdoStreamBytesQueued += count;

    // Throttled progress log so we can see the master pumping segments
    // without flooding the UART (which would itself starve the CO task).
    uint32_t curKB = s_sdoStreamBytesQueued / 1024U;
    if (curKB >= s_sdoStreamLastLoggedKB + 16U) {
        log_i("sdoDownloadChunk: queued %u / %u bytes (%u KB)",
              (unsigned)s_sdoStreamBytesQueued,
              (unsigned)s_sdoStreamTotalSize,
              (unsigned)curKB);
        s_sdoStreamLastLoggedKB = curKB;
    }
    return true;
}

bool CANopenModule::sdoDownloadEnd()
{
    if (!s_sdoStreamActive || s_sdoStreamClient == nullptr) {
        log_e("sdoDownloadEnd: no active stream");
        return false;
    }

    // Final pass with bufferPartial=false to finalise the transfer.
    // The state machine may still need to push remaining FIFO contents
    // and wait for the server's final ACK before returning
    // CO_SDO_RT_ok_communicationEnd (0).
    uint32_t deadlineMs = (uint32_t)(esp_timer_get_time() / 1000ULL)
                          + (uint32_t)SDO_STREAM_TIMEOUT_MS + 2000U;
    CO_SDO_return_t r;
    for (;;) {
        CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;
        r = sdoStreamPump(/*bufferPartial=*/false, &abortCode);
        if (r < 0) {
            log_e("sdoDownloadEnd: ret=%d abort=0x%08lX",
                  r, (unsigned long)abortCode);
            sdoDownloadAbort();
            return false;
        }
        if (r == CO_SDO_RT_ok_communicationEnd) break;

        uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000ULL);
        if ((int32_t)(nowMs - deadlineMs) > 0) {
            log_e("sdoDownloadEnd: timeout waiting for completion");
            sdoDownloadAbort();
            return false;
        }
        // vTaskDelay(1), not taskYIELD: this loop can spin for hundreds
        // of ms waiting on the slave's final block ACK; starving IDLE
        // would trip the IDLE WDT and panic.
        vTaskDelay(1);
    }

    CO_SDOclientClose(s_sdoStreamClient);
    s_sdoStreamActive = false;
    s_sdoStreamClient = nullptr;
    if (s_sdoMutex) xSemaphoreGive(s_sdoMutex);
    log_i("sdoDownloadEnd: stream complete (%u bytes)",
          (unsigned)s_sdoStreamBytesQueued);
    return true;
}

void CANopenModule::sdoDownloadAbort()
{
    if (!s_sdoStreamActive || s_sdoStreamClient == nullptr) return;

    // Best-effort abort: tell the server, ignore errors.
    CO_SDO_abortCode_t abortCode = CO_SDO_AB_GENERAL;
    (void)CO_SDOclientDownload(s_sdoStreamClient, 1000,
                               /*abort=*/true, false,
                               &abortCode, NULL, NULL);
    CO_SDOclientClose(s_sdoStreamClient);
    s_sdoStreamActive = false;
    s_sdoStreamClient = nullptr;
    if (s_sdoMutex) xSemaphoreGive(s_sdoMutex);
    log_i("sdoDownloadAbort: stream aborted after %u bytes",
          (unsigned)s_sdoStreamBytesQueued);
}
#endif // CAN_CONTROLLER_CANOPEN


// ==========================================================================
// T4.4 step 3: merged from CanOpenOTA.cpp - slave-side OTA receiver (OD 0x2F00 SDO domain to esp_ota_write)
// (behavior-identical; per-file preprocessor guards preserved)
// ==========================================================================
/**
 * @file CanOpenOTA.cpp
 * @brief CANopen OTA firmware update via SDO domain transfer (slave side)
 *
 * Implements the OD write extension for 0x2F00 (OTA_FIRMWARE_DATA).
 * Each SDO segment is streamed directly to esp_ota_write(), avoiding
 * full-binary RAM buffering.
 *
 * Flow:
 *   1. Master writes firmware size → 0x2F01 (triggers OTA begin)
 *   2. Master writes CRC32        → 0x2F02 (stored for later verification)
 *   3. Master writes firmware data → 0x2F00 (domain / segmented SDO)
 *      onOtaWriteChunk() is called for each segment → esp_ota_write()
 *   4. When the last segment arrives (dataOffset+count >= dataLength),
 *      CRC32 is verified and boot partition is set.
 */

#include "CanOpenOTA.h"
#include "PinConfig.h"

#if defined(CAN_CONTROLLER_CANOPEN) && !defined(CANOPEN_OTA_NO_RECEIVER)

#include "UC2_OD_Indices.h"
#include <CANopen.h>

extern "C" {
#include "OD.h"
}

#include <esp_ota_ops.h>
#include <esp_log.h>
#include <rom/crc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#undef TAG
#define TAG "OTA_SDO"

// ============================================================================
// Static state
// ============================================================================

static esp_ota_handle_t        s_otaHandle = 0;
static const esp_partition_t*  s_otaPartition = nullptr;
static uint32_t                s_crc32Running = 0;
static uint32_t                s_bytesWritten = 0;
static bool                    s_otaStarted = false;
static uint32_t                s_nextProgressMark      = 64U * 1024U;

// Tracks the millis() time of the last successful onOtaWriteChunk so a
// background task can abort the OTA if the master goes silent mid-transfer.
// Updated from the SDO context; read from the watchdog task.
static volatile uint32_t       s_lastChunkMillis = 0;
// millis() when esp_ota_begin succeeded; used for the KB/s figure in the
// progress log so throughput can be read straight off the slave UART.
static uint32_t                s_otaStartMillis  = 0;

// OTA inactivity timeout: if no chunks arrive for this long, the watchdog
// task aborts the OTA so the slave doesn't stay wedged in "receiving" forever.
// 30 s comfortably exceeds the master's normal inter-chunk gap (~1.5 s) and
// the post-CRC settle time (a few hundred ms).
static constexpr uint32_t      OTA_INACTIVITY_TIMEOUT_MS = 30000;

static OD_extension_t s_otaDataExt;
static OD_extension_t s_otaSizeExt;

// Forward declarations (resetOtaState is defined further down with the
// rest of the internal helpers).
static void resetOtaState();

// ============================================================================
// Deferred reboot task — fires after the SDO END_RSP has had time to go out.
// ============================================================================

static void deferredRestartTask(void* arg) {
    // The SDO END_RSP frame the server sent before we return from the OD
    // callback needs ~ms to make it onto the wire and be processed by the
    // master. Give it a comfortable 2 s buffer so the master's
    // sdoDownloadEnd() returns CO_SDO_RT_ok_communicationEnd cleanly before
    // we kill ourselves.
    vTaskDelay(pdMS_TO_TICKS(2000));
    log_i("Deferred OTA reboot now");
    esp_restart();
    // Never reached.
    vTaskDelete(NULL);
}

static void scheduleDeferredRestart() {
    // Low priority is fine — the only thing this task does is sleep then
    // call esp_restart(). 2 KB stack is plenty.
    BaseType_t rc = xTaskCreate(deferredRestartTask, "ota_restart",
                                2048, NULL, 1, NULL);
    if (rc != pdPASS) {
        log_e("Failed to spawn deferred restart task — falling back to inline restart");
        // Inline restart still drops the SDO response, but better than not
        // restarting at all (the OTA flash already committed).
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }
}

// ============================================================================
// OTA inactivity watchdog — aborts a wedged OTA without external help.
// ============================================================================

static void otaWatchdogTask(void* arg) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        if (!s_otaStarted) continue;
        // s_lastChunkMillis is updated by onOtaWriteChunk; if it goes stale
        // while OTA is "active", the master has disappeared.
        uint32_t now = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        uint32_t last = s_lastChunkMillis;
        if (last != 0 && (now - last) > OTA_INACTIVITY_TIMEOUT_MS) {
            log_w("OTA inactivity timeout (%lu ms since last chunk); aborting",
                  (unsigned long)(now - last));
            // resetOtaState closes the esp_ota handle, clears all state,
            // and returns the slave to an OTA-idle posture so the next
            // /ota_start works without a manual slave reboot.
            resetOtaState();
        }
    }
}

// ============================================================================
// Internal helpers
// ============================================================================

static void resetOtaState() {
    const bool wasActive = s_otaStarted;
    const uint32_t lastBytes = s_bytesWritten;
    if (s_otaStarted && s_otaHandle != 0) {
        esp_ota_abort(s_otaHandle);
    }
    s_otaHandle = 0;
    s_otaPartition = nullptr;
    s_crc32Running = 0;
    s_bytesWritten = 0;
    s_otaStarted = false;
    s_nextProgressMark = 64U * 1024U;
    s_lastChunkMillis = 0;        // disarm the watchdog
    OD_OTA_STATUS = CANOPEN_OTA_IDLE;
    OD_OTA_BYTES_RECEIVED = 0;
    OD_OTA_ERROR_CODE = CANOPEN_OTA_ERR_NONE;
    if (wasActive) {
        log_i("OTA state reset (was active, %lu bytes written)",
              (unsigned long)lastBytes);
    }
}

static void setError(uint8_t errCode) {
    OD_OTA_STATUS = CANOPEN_OTA_ERROR;
    OD_OTA_ERROR_CODE = errCode;
    log_e("OTA error: %d", errCode);
    resetOtaState();
    // Keep error code visible after reset
    OD_OTA_STATUS = CANOPEN_OTA_ERROR;
    OD_OTA_ERROR_CODE = errCode;
}

// ============================================================================
// OD write callback for 0x2F01 (firmware size) — triggers OTA begin
// ============================================================================

static ODR_t onOtaSizeWrite(OD_stream_t* stream, const void* buf,
                            OD_size_t count, OD_size_t* countWritten) {
    // Let CANopenNode write the value to RAM first
    // THIS is on the slave side, so this is called when the master writes to 0x2F01 on the slave.
    // The master is expected to write the firmware size here first, which triggers the OTA process on the slave.
    log_i("onOtaSizeWrite: count=%u", (unsigned)count);
    ODR_t ret = OD_writeOriginal(stream, buf, count, countWritten);
    if (ret != ODR_OK) return ret;

    uint32_t firmwareSize = OD_OTA_FIRMWARE_SIZE;

    if (firmwareSize == 0) {
        // Size 0 = abort/reset
        resetOtaState();
        log_i("OTA reset (size=0)");
        return ODR_OK;
    }

    // Validate size
    if (firmwareSize > 0x140000) {  // 1.25 MB max partition size
        log_e("Invalid OTA size: %lu", (unsigned long)firmwareSize);
        setError(CANOPEN_OTA_ERR_SIZE_TOO_LARGE);
        return ODR_HW;
    }

    // Abort any previous OTA
    log_i("OTA is still active: %s, bytesWritten=%lu. Resetting state for new OTA.",
          s_otaStarted ? "yes" : "no", (unsigned long)s_bytesWritten);
    if (s_otaStarted) {
        log_i("OTA already in progress, aborting and starting new OTA");
        resetOtaState();
    }

    // Find OTA partition
    s_otaPartition = esp_ota_get_next_update_partition(NULL);
    if (s_otaPartition == nullptr) {
        log_e("No OTA partition found");
        setError(CANOPEN_OTA_ERR_NO_PARTITION);
        return ODR_HW;
    }

    // esp_ota_begin erases the target partition. This blocks the calling
    // thread for 200-500ms. During the erase, flash SPI operations may
    // momentarily starve the CAN peripheral of bus cycles (especially on
    // single-core ESP32-S3 variants sharing the SPI bus). Yield briefly
    // before AND after to let the CAN ctrl task drain its queues.
    vTaskDelay(pdMS_TO_TICKS(10));
    esp_err_t err = esp_ota_begin(s_otaPartition, firmwareSize, &s_otaHandle);
    if (err != ESP_OK) {
        log_e("esp_ota_begin failed: 0x%x", err);
        setError(CANOPEN_OTA_ERR_BEGIN_FAILED);
        return ODR_HW;
    }
    // Let the CAN stack recover from any frames missed during flash erase
    vTaskDelay(pdMS_TO_TICKS(50));

    s_crc32Running = 0;
    s_bytesWritten = 0;
    s_otaStarted = true;
    OD_OTA_STATUS = CANOPEN_OTA_RECEIVING;
    OD_OTA_BYTES_RECEIVED = 0;
    OD_OTA_ERROR_CODE = CANOPEN_OTA_ERR_NONE;
    // Arm the inactivity watchdog with "now" as the last-activity reference
    // so it doesn't fire while waiting for the first chunk.
    s_lastChunkMillis = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    s_otaStartMillis  = s_lastChunkMillis;

    log_i("OTA started: size=%lu, partition=%s",
          (unsigned long)firmwareSize, s_otaPartition->label);
    return ODR_OK;
}

// ============================================================================
// OD write callback for 0x2F00 (firmware data domain) — streams to flash
// ============================================================================

static ODR_t onOtaWriteChunk(OD_stream_t* stream, const void* buf,
                             OD_size_t count, OD_size_t* countWritten) {
    if (!s_otaStarted) {
        log_e("OTA data received but OTA not started (write 0x2F01 size first) count=%u",
              (unsigned)count);
        return ODR_DEV_INCOMPAT;
    }

    if (count == 0) {
        // Stack calls us with count=0 to query state; not an error.
        *countWritten = 0;
        return ODR_OK;
    }

    // Write chunk to flash. esp_ota_write does not block long for small
    // counts (<= 256 B) but we still avoid logging here on every call —
    // the SDO segment rate is several kHz and per-segment logs over
    // 921600 baud UART would starve the CANopen task and force bus-off.
    esp_err_t err = esp_ota_write(s_otaHandle, buf, count);
    if (err != ESP_OK) {
        log_e("esp_ota_write failed at offset %lu count=%u err=0x%x",
              (unsigned long)s_bytesWritten, (unsigned)count, err);
        setError(CANOPEN_OTA_ERR_WRITE_FAILED);
        return ODR_HW;
    }

    // Update CRC32 incrementally
    s_crc32Running = crc32_le(s_crc32Running, (const uint8_t*)buf, count);
    s_bytesWritten += count;
    *countWritten = count;

    // Update OD status for TPDO reporting
    OD_OTA_BYTES_RECEIVED = s_bytesWritten;

    // Feed the OTA inactivity watchdog. If the master goes silent for
    // OTA_INACTIVITY_TIMEOUT_MS, otaWatchdogTask will resetOtaState() so
    // the next /ota_start can start cleanly without a slave reboot.
    s_lastChunkMillis = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

    // Throttled progress log: every 64 KB. This callback runs inside the
    // SDO server's block-ACK path, so a log line here delays the ACK and
    // every segment behind it — never log per call.
    if (s_bytesWritten >= s_nextProgressMark) {
        uint32_t elapsedMs = s_lastChunkMillis - s_otaStartMillis;
        log_i("OTA progress: %lu / %lu bytes, %lu KB/s",
              (unsigned long)s_bytesWritten,
              (unsigned long)OD_OTA_FIRMWARE_SIZE,
              (unsigned long)(elapsedMs ? s_bytesWritten / elapsedMs : 0));
        s_nextProgressMark += 64U * 1024U;
    }

    // Detect end-of-transfer.
    //
    // For a domain entry (0x2F00 has dataLength=0 in the OD), the SDO
    // server only fills in stream->dataLength on the FINAL segment via
    // validateAndWriteToOD() — on intermediate buffer flushes (every
    // ~CO_CONFIG_SDO_SRV_BUFFER_SIZE bytes) it stays 0. Comparing against
    // dataLength alone would therefore terminate the transfer on the very
    // first partial flush (~28 bytes for the default 32-byte server FIFO).
    //
    // The master writes OD_OTA_FIRMWARE_SIZE (0x2F01) BEFORE streaming any
    // data, so we use that as the authoritative total and only consult
    // stream->dataLength as a tie-breaker when it is non-zero (which only
    // happens on the very last segment).
    const uint32_t expectedSize  = OD_OTA_FIRMWARE_SIZE;
    const bool     sizeReached   = (expectedSize > 0)
                                   && (s_bytesWritten >= expectedSize);
    const bool     streamFinished = (stream->dataLength > 0)
                                    && ((stream->dataOffset + count)
                                        >= stream->dataLength);
    if (sizeReached || streamFinished) {
        {
            uint32_t totalMs = s_lastChunkMillis - s_otaStartMillis;
            log_i("OTA transfer complete: %lu bytes in %lu ms (%lu KB/s)",
                  (unsigned long)s_bytesWritten, (unsigned long)totalMs,
                  (unsigned long)(totalMs ? s_bytesWritten / totalMs : 0));
        }
        OD_OTA_STATUS = CANOPEN_OTA_VERIFYING;

        // Verify CRC32
        uint32_t expectedCrc = OD_OTA_FIRMWARE_CRC32;
        if (expectedCrc != 0 && s_crc32Running != expectedCrc) {
            log_e("CRC32 mismatch: expected=0x%08lx, got=0x%08lx",
                  (unsigned long)expectedCrc, (unsigned long)s_crc32Running);
            esp_ota_abort(s_otaHandle);
            s_otaHandle = 0;
            s_otaStarted = false;
            setError(CANOPEN_OTA_ERR_CRC_MISMATCH);
            return ODR_HW;
        }
        else{
            log_i("CRC32 verified: 0x%08lx", (unsigned long)s_crc32Running);
        }

        // Commit OTA
        err = esp_ota_end(s_otaHandle);
        s_otaHandle = 0;
        if (err != ESP_OK) {
            log_e("esp_ota_end failed: 0x%x", err);
            s_otaStarted = false;
            setError(CANOPEN_OTA_ERR_COMMIT_FAILED);
            return ODR_HW;
        }

        err = esp_ota_set_boot_partition(s_otaPartition);
        if (err != ESP_OK) {
            log_e("esp_ota_set_boot_partition failed: 0x%x", err);
            s_otaStarted = false;
            setError(CANOPEN_OTA_ERR_COMMIT_FAILED);
            return ODR_HW;
        }

        OD_OTA_STATUS = CANOPEN_OTA_COMPLETE;
        s_otaStarted = false;
        log_i("OTA complete and verified. Boot partition set to %s",
              s_otaPartition->label);

        // Schedule a deferred reboot. Critically, we DO NOT call
        // esp_restart() inline here — that would kill the slave before the
        // SDO server has a chance to send the END_RSP frame back to the
        // master, leaving the master stuck in sdoDownloadEnd() waiting on
        // a response that will never arrive. The master's IDLE WDT then
        // fires at 5 s and the master panics — exactly the failure we
        // were seeing in field logs. Instead, return ODR_OK now so the
        // SDO server sends the END_RSP, and let the deferred task reboot
        // us 2 s later (plenty of time for the response to hit the wire
        // and the master to clean up its end of the stream).
        scheduleDeferredRestart();

        // Final segment: tell the SDO server the OD variable is complete.
        return ODR_OK;
    }

    // IMPORTANT: return ODR_PARTIAL while more data is expected. Returning
    // ODR_OK before all expectedSize bytes have arrived makes CANopenNode's
    // validateAndWriteToOD() interpret the OD variable as "fully written"
    // and abort the next incoming segment with CO_SDO_AB_DATA_LONG
    // (0x06070012, "Length of service parameter too high").
    // See CO_SDOserver.c lines 575-579 in lib/ESP32_CanOpenNode.
    return ODR_PARTIAL;
}

// ============================================================================
// Public API
// ============================================================================

void CanOpenOTA::registerOdExtensions() {
    // Register write callback for 0x2F00 (firmware data domain)
    OD_entry_t* dataEntry = OD_find(OD, UC2_OD::OTA_FIRMWARE_DATA);
    if (dataEntry) {
        s_otaDataExt.object = nullptr;
        s_otaDataExt.read   = nullptr;  // write-only
        s_otaDataExt.write  = onOtaWriteChunk;
        OD_extension_init(dataEntry, &s_otaDataExt);
        log_i("OTA firmware data OD extension registered (0x2F00)");
    } else {
        log_e("OD entry 0x2F00 not found!");
    }

    // Register write callback for 0x2F01 (firmware size — triggers begin)
    OD_entry_t* sizeEntry = OD_find(OD, UC2_OD::OTA_FIRMWARE_SIZE);
    if (sizeEntry) {
        s_otaSizeExt.object = nullptr;
        s_otaSizeExt.read   = nullptr;
        s_otaSizeExt.write  = onOtaSizeWrite;
        OD_extension_init(sizeEntry, &s_otaSizeExt);
        log_i("OTA firmware size OD extension registered (0x2F01)");
    } else {
        log_e("OD entry 0x2F01 not found!");
    }

    // Initialize status
    OD_OTA_STATUS = CANOPEN_OTA_IDLE;
    OD_OTA_BYTES_RECEIVED = 0;
    OD_OTA_ERROR_CODE = CANOPEN_OTA_ERR_NONE;

    // Spawn the inactivity watchdog. If an OTA is started but no chunks
    // arrive for OTA_INACTIVITY_TIMEOUT_MS, the watchdog calls
    // resetOtaState() so the slave can accept a fresh /ota_start without
    // a manual reboot. One-shot creation — task lives for the lifetime
    // of the slave.
    BaseType_t rc = xTaskCreate(otaWatchdogTask, "ota_wdog",
                                3072, NULL, 1, NULL);
    if (rc != pdPASS) {
        log_e("Failed to spawn OTA watchdog task — aborted OTAs will require manual reboot");
    }
}

#endif // CAN_CONTROLLER_CANOPEN && !CANOPEN_OTA_NO_RECEIVER


// ==========================================================================
// T4.4 step 3: merged from CanOpenOTAStreaming.cpp - master-side OTA sender (flashSlave / readSlaveStatus / actFromJson)
// (behavior-identical; per-file preprocessor guards preserved)
// ==========================================================================
/**
 * @file CanOpenOTAStreaming.cpp
 * @brief Master-side OTA streaming to a slave via CANopen SDO domain transfer
 *
 * Uses CANopenModule::writeSDO*() and writeSDODomain() to push firmware
 * to the slave. CANopenNode handles segmented/block SDO automatically.
 */

#include "CanOpenOTAStreaming.h"

#if defined(CAN_CONTROLLER_CANOPEN) && !defined(CANOPEN_OTA_NO_SENDER)

#include "CANopenModule.h"
#include "UC2_OD_Indices.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdlib>
#include <rom/crc.h>

extern "C" {
#include "OD.h"
}

#undef TAG
#define TAG "OTA_MASTER"

// ============================================================================
// Public API
// ============================================================================

bool CanOpenOTAStreaming::flashSlave(uint8_t nodeId, const uint8_t* data,
                                     size_t dataSize, uint32_t crc32) {
    ESP_LOGI(TAG, "Flashing node %d: %u bytes, CRC32=0x%08lx",
             nodeId, (unsigned)dataSize, (unsigned long)crc32);

    // Step 1: Write CRC32 to slave (so it can verify after transfer)
    if (!CANopenModule::writeSDO_u32(nodeId, UC2_OD::OTA_FIRMWARE_CRC32, 0, crc32)) {
        ESP_LOGE(TAG, "Failed to write CRC32 to node %d", nodeId);
        return false;
    }

    // Step 2: Write firmware size (triggers OTA begin on slave)
    if (!CANopenModule::writeSDO_u32(nodeId, UC2_OD::OTA_FIRMWARE_SIZE, 0, (uint32_t)dataSize)) {
        ESP_LOGE(TAG, "Failed to write firmware size to node %d", nodeId);
        return false;
    }

    // Brief delay for slave to initialize OTA partition
    vTaskDelay(pdMS_TO_TICKS(200));

    // Step 3: Write firmware data via SDO domain transfer
    // CANopenNode handles segmented/block transfer automatically
    if (!CANopenModule::writeSDODomain(nodeId, UC2_OD::OTA_FIRMWARE_DATA, 0,
                                        data, dataSize)) {
        ESP_LOGE(TAG, "SDO domain write failed for node %d", nodeId);
        return false;
    }

    ESP_LOGI(TAG, "OTA transfer complete for node %d, waiting for verification...", nodeId);

    // Step 4: Read back status (slave should be COMPLETE or rebooting)
    vTaskDelay(pdMS_TO_TICKS(500));
    uint8_t status = readSlaveStatus(nodeId);
    // Node may have already rebooted, so timeout is acceptable
    if (status == 0x03) {  // CANOPEN_OTA_COMPLETE
        ESP_LOGI(TAG, "Node %d: OTA verified and committed", nodeId);
    } else if (status == 0xFF) {
        ESP_LOGI(TAG, "Node %d: status read failed (likely rebooting — good)", nodeId);
    } else {
        ESP_LOGW(TAG, "Node %d: unexpected status 0x%02x", nodeId, status);
    }

    return true;
}

uint8_t CanOpenOTAStreaming::readSlaveStatus(uint8_t nodeId) {
    uint8_t status = 0xFF;
    size_t readSize = 0;
    if (CANopenModule::readSDO(nodeId, UC2_OD::OTA_STATUS, 0,
                                &status, sizeof(status), &readSize)) {
        return status;
    }
    return 0xFF;
}

int CanOpenOTAStreaming::actFromJson(cJSON* doc) {
    cJSON* otaObj = cJSON_GetObjectItem(doc, "ota");
    if (!otaObj) {
        ESP_LOGE(TAG, "Missing 'ota' object");
        return -1;
    }

    cJSON* nodeIdItem = cJSON_GetObjectItem(otaObj, "nodeId");
    cJSON* sizeItem   = cJSON_GetObjectItem(otaObj, "size");
    cJSON* crc32Item  = cJSON_GetObjectItem(otaObj, "crc32");

    if (!nodeIdItem || !sizeItem) {
        ESP_LOGE(TAG, "Missing 'nodeId' or 'size' in ota object");
        return -1;
    }

    uint8_t  nodeId = (uint8_t)nodeIdItem->valueint;
    uint32_t size   = (uint32_t)sizeItem->valuedouble;

    uint32_t crc32 = 0;
    if (crc32Item && cJSON_IsString(crc32Item)) {
        crc32 = (uint32_t)strtoul(crc32Item->valuestring, NULL, 16);
    } else if (crc32Item && cJSON_IsNumber(crc32Item)) {
        crc32 = (uint32_t)crc32Item->valuedouble;
    }

    ESP_LOGI(TAG, "OTA command: node=%d, size=%lu, crc32=0x%08lx",
             nodeId, (unsigned long)size, (unsigned long)crc32);

    // For now, log that we need data from serial.
    // The actual binary receive + flash is handled by the serial binary mode
    // which calls flashSlave() once the data is buffered.
    // TODO: Implement serial binary receive mode for large firmware images
    // For small test images or SD card, caller can use flashSlave() directly.
    ESP_LOGI(TAG, "OTA start acknowledged — awaiting firmware data");

    return 0;
}

#endif // CAN_CONTROLLER_CANOPEN && !CANOPEN_OTA_NO_SENDER


// ==========================================================================
// T4.4 step 3: merged from OtaBinaryReceive.cpp - master-side serial to CAN OTA binary bridge (4 KB streaming)
// (behavior-identical; per-file preprocessor guards preserved)
// ==========================================================================
/**
 * @file OtaBinaryReceive.cpp
 * @brief Master-side serial → CAN OTA binary receive bridge (streaming).
 *
 * Receives a firmware blob from the host over Serial and forwards it to a
 * remote slave over CANopen SDO without ever buffering the full image.
 * The image is consumed in 4 KB chunks; each chunk is pushed to the slave
 * via CANopenModule::sdoDownloadChunk while the next chunk is read from
 * Serial. Memory cost stays constant (~4 KB) regardless of firmware size.
 */
#include "OtaBinaryReceive.h"
#include "PinConfig.h"

#if defined(CAN_CONTROLLER_CANOPEN) && !defined(CANOPEN_OTA_NO_SENDER)

#include "CANopenModule.h"
#include "UC2_OD_Indices.h"
#include <uc2_hal.h>
#include <esp_log.h>
#include <rom/crc.h>

#undef TAG
#define TAG "OTA_RX"

namespace {

// Streaming chunk buffer in internal DRAM. 4 KB matches the ESP32 flash
// page size and is small enough to fit on any ESP32 build (no PSRAM
// required). The total firmware size is irrelevant — we never hold more
// than one chunk in RAM.
constexpr size_t   CHUNK_SIZE          = 4096;
constexpr uint32_t RX_TIMEOUT_MS       = 30000;

uint8_t  s_chunk[CHUNK_SIZE];
size_t   s_chunkPos        = 0;

bool     s_active          = false;
bool     s_streamOpen      = false;
uint32_t s_totalSize       = 0;
uint32_t s_expectedCrc32   = 0;
uint32_t s_runningCrc32    = 0;
uint8_t  s_nodeId          = 0;
uint32_t s_bytesReceived   = 0;
uint32_t s_lastByteMillis  = 0;

void emitJson(const char* msg) {
    Serial.write(msg);
    Serial.write('\n');
}

void cleanup() {
    if (s_streamOpen) {
        CANopenModule::sdoDownloadAbort();
        s_streamOpen = false;
    }
    s_active         = false;
    s_totalSize      = 0;
    s_expectedCrc32  = 0;
    s_runningCrc32   = 0;
    s_nodeId         = 0;
    s_bytesReceived  = 0;
    s_chunkPos       = 0;
}

// Push the buffered chunk to the slave. On the first call also primes the
// slave with CRC32 + size and opens the streaming SDO session for
// OTA_FIRMWARE_DATA. Returns false on error (and emits an error JSON).
bool flushChunk() {
    if (s_chunkPos == 0) {
        log_i("flushChunk called with empty chunk, ignoring");
        return true;
    }

    if (!s_streamOpen) {
        // 1) Tell the slave the expected CRC32 (so it can verify the image).
        if (!CANopenModule::writeSDO_u32(s_nodeId, UC2_OD::OTA_FIRMWARE_CRC32,
                                         0, s_expectedCrc32)) {
            emitJson("{\"ota_status\":\"error\",\"error\":\"crc_write_failed\"}");
            return false;
        } // TODO: Under which circumstances can this fail? If the slave is reachable, shouldn't this always succeed?
          //       If it can fail, do we want to retry a few times before giving up?
          //       If it fails, the host sees crc_write_failed which is a bit misleading if the real issue is connectivity. Maybe we want a separate error for this case?

        // 2) Tell the slave the firmware size — this triggers esp_ota_begin
        //    on the slave so the next OTA_FIRMWARE_DATA bytes can be flashed.
        if (!CANopenModule::writeSDO_u32(s_nodeId, UC2_OD::OTA_FIRMWARE_SIZE,
                                         0, s_totalSize)) {
            emitJson("{\"ota_status\":\"error\",\"error\":\"size_write_failed\"}");
            return false;
        }
        // Give the slave time to complete esp_ota_begin (flash erase can
        // take 200-500ms depending on partition state) and for the CAN
        // stack to recover from any missed frames during the erase.
        vTaskDelay(pdMS_TO_TICKS(500));

        // 3) Open the streaming SDO session for the firmware blob.
        if (!CANopenModule::sdoDownloadBegin(s_nodeId,
                UC2_OD::OTA_FIRMWARE_DATA, 0, s_totalSize)) {
            emitJson("{\"ota_status\":\"error\",\"error\":\"sdo_begin_failed\"}");
            return false;
        }
        s_streamOpen = true;
    }

    //CANopenModule::sdoDownloadChunk(s_chunk, s_chunkPos);
    if (!CANopenModule::sdoDownloadChunk(s_chunk, s_chunkPos)) { // TODO: What exactly is this doing? Does it push the chunk to the slave?
        emitJson("{\"ota_status\":\"error\",\"error\":\"sdo_chunk_failed\"}");
        s_streamOpen = false;  // sdoDownloadChunk already aborted on failure
        return false;
    }

    // Update running CRC32 over the bytes actually pushed downstream.
    s_runningCrc32 = crc32_le(s_runningCrc32, s_chunk, s_chunkPos);
    s_chunkPos = 0;
    return true;
}

}  // namespace

namespace OtaBinaryReceive {

cJSON* begin(uint8_t nodeId, uint32_t size, uint32_t crc32) {
    // This is most likiley fired from the master
    log_i("OTA begin: nodeId=%u size=%u crc32=0x%08lX",
          nodeId, (unsigned)size, (unsigned long)crc32);
    cJSON* resp = cJSON_CreateObject();
    if (!resp) return nullptr;

    if (s_active) {
        // A previous run is wedged (e.g. host died mid-transfer and we
        // never saw a final byte; or the master rebooted before cleanup
        // ran). Reset to a clean state so we don't reject this request.
        log_i("OTA: previous session still marked active, forcing cleanup");
        cleanup();
    }
    if (CANopenModule::sdoDownloadActive()) {
        // SDO mutex still held from a wedged previous stream — release it
        // so sdoDownloadBegin() below can take the mutex.
        log_i("OTA: stale SDO stream detected, aborting it");
        CANopenModule::sdoDownloadAbort();
    }
    if (size == 0 || size > 8 * 1024 * 1024) {
        log_e("Invalid OTA size: %u", (unsigned)size);
        cJSON_AddStringToObject(resp, "ota_status", "error");
        cJSON_AddStringToObject(resp, "error", "invalid_size");
        return resp;
    }

    // Confirm the slave is reachable BEFORE telling the host "ready".
    // Otherwise the host streams the first 4 KB into the master while the
    // master can't yet reach the slave — by the time flushChunk runs the
    // host has already disconnected on size_write_failed.
    if (!CANopenModule::waitForNodeReachable(nodeId, 10000)) {
        log_e("OTA: slave 0x%02X not reachable after 10s", nodeId);
        cJSON_AddStringToObject(resp, "ota_status", "error");
        cJSON_AddStringToObject(resp, "error", "slave_unreachable");
        return resp;
    }

    // Streaming session — no large allocation. The 4 KB s_chunk buffer
    // lives in BSS  and is reused for every transfer.
    s_totalSize       = size;
    s_expectedCrc32   = crc32;
    s_runningCrc32    = 0;
    s_nodeId          = nodeId;
    s_bytesReceived   = 0;
    s_lastByteMillis  = millis();
    s_chunkPos        = 0;
    s_streamOpen      = false;
    s_active          = true;

    log_i("OTA session ready (streaming, %u-byte chunks)", (unsigned)CHUNK_SIZE);

    cJSON_AddStringToObject(resp, "ota_status", "ready");
    cJSON_AddNumberToObject(resp, "size",      (double)size);
    cJSON_AddNumberToObject(resp, "nodeId",    nodeId);
    cJSON_AddNumberToObject(resp, "chunkSize", (double)CHUNK_SIZE);
    return resp;
}

bool isActive() { return s_active; }

void processBytes() {
    if (!s_active) return;

    // Drain as much as possible this call - NOT just one FIFO-worth.
    // The host writes 4096 B at once; if we only drain ~256 B per main-loop
    // iteration, the UART ring buffer overflows and bytes are lost silently
    // (firmware ends up corrupt, only caught by the slave-side CRC).
    bool didRead = false;
    while (Serial.available() > 0 && s_bytesReceived < s_totalSize) {
        uint32_t remaining = s_totalSize - s_bytesReceived;
        size_t   space     = CHUNK_SIZE - s_chunkPos;
        size_t   avail     = (size_t)Serial.available();
        size_t   toRead    = avail;
        if (toRead > remaining) toRead = remaining;
        if (toRead > space)     toRead = space;
        if (toRead == 0) break;  // chunk is full, must flush before reading more

        size_t nRead = Serial.readBytes(reinterpret_cast<char*>(s_chunk) + s_chunkPos, toRead);
        if (nRead == 0) break;
        s_chunkPos      += nRead;
        s_bytesReceived += nRead;
        s_lastByteMillis = millis();
        didRead = true;

        // Flush as soon as the chunk is full or the last firmware byte landed.
        bool isLast = (s_bytesReceived >= s_totalSize);
        if (s_chunkPos >= CHUNK_SIZE || (isLast && s_chunkPos > 0)) {
            // ACK *before* pushing to CAN. The host sends the next chunk as
            // soon as it sees the ACK, so it lands in the 8 KB UART ring
            // while this one is on the bus — the serial hop is hidden
            // behind the CAN hop instead of added to it. The host keeps
            // exactly one chunk in flight, so the ring cannot overflow.
            char ack[48];
            snprintf(ack, sizeof(ack), "{\"ota_rx\":%lu}",
                     (unsigned long)s_bytesReceived);
            emitJson(ack);

            if (!flushChunk()) {
                log_i("Flushing chunk failed, aborting OTA session");
                // The next chunk is probably already in flight (we ACKed
                // above). Swallow it so it isn't parsed as JSON commands.
                vTaskDelay(pdMS_TO_TICKS(100));
                while (Serial.available() > 0) Serial.read();
                cleanup();
                return;
            }
            // Return once per chunk: the caller (loopTask) feeds the task
            // watchdog and polls the E-stop between calls.
            break;
        }
    }

    if (!didRead) {
        // Watchdog: abort if the host stalls mid-transfer.
        if ((millis() - s_lastByteMillis) > RX_TIMEOUT_MS) {
            ESP_LOGE(TAG, "OTA receive timeout after %u/%u bytes",
                     (unsigned)s_bytesReceived, (unsigned)s_totalSize);
            emitJson("{\"ota_status\":\"error\",\"error\":\"rx_timeout\"}");
            cleanup();
            return;
        }
    }

    if (s_bytesReceived < s_totalSize) return;

    // All bytes received and queued. Verify the master-side CRC32 (catches
    // host→master serial corruption before we waste minutes verifying on
    // the slave).
    if (s_expectedCrc32 != 0 && s_runningCrc32 != s_expectedCrc32) {
        char err[160];
        snprintf(err, sizeof(err),
                 "{\"ota_status\":\"error\",\"error\":\"crc_mismatch\","
                 "\"computed\":\"0x%08lX\",\"expected\":\"0x%08lX\","
                 "\"comment\":\"Host→master serial corruption?\"}",
                 (unsigned long)s_runningCrc32,
                 (unsigned long)s_expectedCrc32);
        emitJson(err);
        cleanup();
        return;
    }

    emitJson("{\"ota_status\":\"flashing\"}");

    // Finalise the SDO session — the slave now runs its own CRC check,
    // calls esp_ota_end + set_boot_partition + reboot.
    bool ok = CANopenModule::sdoDownloadEnd();
    s_streamOpen = false;

    if (ok) {
        emitJson("{\"ota_status\":\"success\"}");
    } else {
        emitJson("{\"ota_status\":\"error\",\"error\":\"sdo_end_failed\"}");
    }

    cleanup();
}

}  // namespace OtaBinaryReceive

// T4.2: C shims for the pure-C SerialProcessIn.c (binary OTA mode check +
// byte drain). Defined inside the same guard as the namespace above.
extern "C" bool ota_isActive(void)
{ return OtaBinaryReceive::isActive(); }

extern "C" void ota_processBytes(void)
{ OtaBinaryReceive::processBytes(); }

#endif  // CAN_CONTROLLER_CANOPEN && !CANOPEN_OTA_NO_SENDER

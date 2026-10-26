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

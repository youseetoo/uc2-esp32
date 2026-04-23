/**
 * CANopenModule.cpp
 *
 * CANopen stack integration for UC2.
 * Adapted from the MWE's uc2_canopen.cpp (proven working code) with UC2-specific
 * integration into the existing codebase:
 *   - TWAI driver init + FreeRTOS task structure from MWE
 *   - SDO client helpers from MWE
 *   - OD <-> Module sync stubs for future full integration
 *
 * Pin configuration comes from pinConfig (CAN_TX, CAN_RX) at runtime.
 * Node ID comes from runtimeConfig.canNodeId (NVS-backed).
 */
#include "CANopenModule.h"


#include "PinConfig.h"
#ifdef CAN_CONTROLLER_CANOPEN

#include "../config/RuntimeConfig.h"
#include "../config/NVSConfig.h"
#include "RoutingTable.h"
// Named OD constants — generated from tools/canopen/uc2_canopen_registry.yaml.
// Constants are declared here for compile-time verification; not yet used in logic.
#include "UC2_OD_Indices.h"

#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
#include "../serial/SerialProcess.h"
#include "../qid/QidRegistry.h"
#endif
#ifdef HOME_MOTOR
#include "../home/HomeMotor.h"
#endif
#ifdef LASER_CONTROLLER
#include "../laser/LaserController.h"
#endif
#ifdef LED_CONTROLLER
#include "../led/LedController.h"
#endif
#ifdef GALVO_CONTROLLER
#include "../scanner/GalvoController.h"
#endif

#include "CanOpenOTA.h"

extern "C" {
#include <CANopen.h>
#include "OD.h"
}

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/twai.h"

// ============================================================================
// Constants
// ============================================================================
#define TAG_CAN    "UC2_CAN"
#define TAG_CO     "UC2_CO"

// Task priorities (same scheme as MWE trainer)
#define CAN_CTRL_TASK_PRIO          1
#define CANOPEN_TASK_PRIO           3
#define CANOPEN_TMR_TASK_PRIO       4
#define CANOPEN_INT_TASK_PRIO       2

// NMT control flags
#define NMT_CONTROL \
    CO_NMT_STARTUP_TO_OPERATIONAL \
    | CO_NMT_ERR_ON_ERR_REG \
    | CO_ERR_REG_GENERIC_ERR \
    | CO_ERR_REG_COMMUNICATION
#define FIRST_HB_TIME           500
#define SDO_SRV_TIMEOUT_TIME   1000
#define SDO_CLI_TIMEOUT_TIME   1000
#define SDO_CLI_BLOCK          false
#define OD_STATUS_BITS         NULL

// ============================================================================
// TWAI configuration — built at runtime from pinConfig
// ============================================================================
static const twai_filter_config_t  f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
static const twai_timing_config_t  t_config = TWAI_TIMING_CONFIG_500KBITS();

// g_config is built in CAN_ctrl_task because CAN_TX/RX come from pinConfig
// (not available as constexpr).

// ============================================================================
// Globals (same pattern as MWE)
// ============================================================================
typedef struct { twai_message_t message; } CANMessages;

static SemaphoreHandle_t CAN_ctrl_task_sem;
static SemaphoreHandle_t s_sdoMutex = NULL;

// These queues are extern'd by CO_driver_ESP32.c in the library
QueueHandle_t CAN_TX_queue;
QueueHandle_t CAN_RX_queue;

CO_t* CO = NULL;

// ============================================================================
// CAN controller task — manages TWAI driver, TX/RX queues, error recovery
// (Preserved from MWE with pinConfig-based GPIO)
// ============================================================================
void CANopenModule::CAN_ctrl_task(void* arg)
{
    twai_message_t rx_message;
    CANMessages tx_msg;

    xSemaphoreTake(CAN_ctrl_task_sem, portMAX_DELAY);

    // Build general config at runtime from pinConfig pins.
    // tx_queue_len = 0: disable TWAI driver's internal TX queue entirely.
    // We use our own CAN_TX_queue and feed twai_transmit() one frame at a
    // time.  The driver-internal queue causes the tx_msg_count race
    // (assert twai.c:183 tx_msg_count >= 0) during bus-off / recovery
    // transitions because the ISR can decrement a counter that was already
    // cleared.  With tx_queue_len=0 the driver writes directly to the HW
    // TX buffer and tx_msg_count never exceeds 1.
    twai_general_config_t g_config = {
        .mode           = TWAI_MODE_NORMAL,
        .tx_io          = (gpio_num_t)pinConfig.CAN_TX,
        .rx_io          = (gpio_num_t)pinConfig.CAN_RX,
        .clkout_io      = TWAI_IO_UNUSED,
        .bus_off_io     = TWAI_IO_UNUSED,
        .tx_queue_len   = 0,
        .rx_queue_len   = 20,
        .alerts_enabled = TWAI_ALERT_NONE,
        .clkout_divider = 0,
        .intr_flags     = ESP_INTR_FLAG_LEVEL1
    };

    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_LOGI(TAG_CAN, "TWAI driver installed (TX=%d, RX=%d)",
             pinConfig.CAN_TX, pinConfig.CAN_RX);
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG_CAN, "TWAI driver started");

    twai_reconfigure_alerts(TWAI_ALERT_ALL, NULL);

    // Flush old messages
    uint32_t alerts;
    unsigned long flushStart = millis();
    while ((millis() - flushStart) < 100) {
        twai_read_alerts(&alerts, 0);
        if (alerts & TWAI_ALERT_RX_DATA) twai_receive(&rx_message, 0);
        vTaskDelay(1);
    }

    // Main CAN I/O loop
    while (true) {
        twai_read_alerts(&alerts, 0);

        // RX: drain TWAI FIFO into CAN_RX_queue (for CANopenNode to process)
        if (alerts & TWAI_ALERT_RX_DATA) {
            while (twai_receive(&rx_message, 0) == ESP_OK) {
                xQueueSend(CAN_RX_queue, (void*)&rx_message, 10);
            }
        }

        // TX: drain as many queued frames as the HW can accept per iteration.
        // Only attempt TX when the driver is actually RUNNING and the HW TX
        // buffer is free (msgs_to_tx == 0).  With tx_queue_len=0 the driver
        // never queues internally, so twai_transmit returns ESP_FAIL if the
        // buffer is busy — no risk of the tx_msg_count race.
        {
            twai_status_info_t st;
            while (twai_get_status_info(&st) == ESP_OK &&
                   st.state == TWAI_STATE_RUNNING &&
                   st.msgs_to_tx == 0)
            {
                if (xQueueReceive(CAN_TX_queue, (void*)&tx_msg, 0) != pdTRUE) break;
                esp_err_t err = twai_transmit(&tx_msg.message, 0);
                if (err != ESP_OK) {
                    // HW buffer unexpectedly busy — put the frame back
                    xQueueSendToFront(CAN_TX_queue, (void*)&tx_msg, 0);
                    break;
                }
            }
        }

        // Bus-off: flush our TX queue (frames are now stale) then
        // trigger hardware recovery.
        if (alerts & TWAI_ALERT_BUS_OFF) {
            ESP_LOGW(TAG_CAN, "Bus-off — flushing TX queue and initiating recovery...");
            while (xQueueReceive(CAN_TX_queue, (void*)&tx_msg, 0) == pdTRUE) {}
            twai_initiate_recovery();
        }

        // Recovery completed — the driver is now stopped; restart it.
        if (alerts & TWAI_ALERT_BUS_RECOVERED) {
            ESP_LOGI(TAG_CAN, "Bus recovered");
            twai_start();
            twai_reconfigure_alerts(TWAI_ALERT_ALL, NULL);
        }

        // Transient bus frame errors (e.g. no ACK when master absent) — hardware
        // handles these automatically; just count quietly to avoid log spam.
        if (alerts & TWAI_ALERT_BUS_ERROR) {
            static uint32_t busErrCount = 0;
            if ((++busErrCount % 100) == 1) {
                ESP_LOGW(TAG_CAN, "CAN bus errors: %u (normal when master absent)", (unsigned)busErrCount);
            }
        }

        vTaskDelay(1);
    }
}

// ============================================================================
// CANopenNode timer task — processes SYNC, RPDOs, TPDOs at 1ms
// ============================================================================
void CANopenModule::CO_tmr_task(void* arg)
{
    for (;;) {
        if (CO != NULL && !CO->nodeIdUnconfigured && CO->CANmodule->CANnormal) {
            uint32_t timeDifference_us = 1000;
            bool_t syncWas = false;

#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE
            syncWas = CO_process_SYNC(CO, timeDifference_us, NULL);
#endif
#if (CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE
            CO_process_RPDO(CO, syncWas, timeDifference_us, NULL);
#endif

            // Sync RPDO data from OD into Module commands (slave only)
            syncRpdoToModules();

            // Sync Module state back into OD for TPDOs (slave only)
            syncModulesToTpdo();

#if (CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE
            CO_process_TPDO(CO, syncWas, timeDifference_us, NULL);
#endif
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ============================================================================
// CANopenNode interrupt task — polls CAN_RX_queue for CANopen frame matching
// ============================================================================
void CANopenModule::CO_interrupt_task(void* arg)
{
    /*
    This function handles CANopen interrupts by polling the CAN_RX_queue for incoming CANopen frames and processing them accordingly.
    */
    for (;;) {
        if (CO != NULL) {
            // Drain all pending RX messages each tick for responsive SDO
            while (uxQueueMessagesWaiting(CAN_RX_queue) > 0) {
                CO_CANinterrupt(CO->CANmodule);
            }
        }
        vTaskDelay(1);
    }
}

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
            ESP_LOGE("SDO", "Read abort: %d", SDO_ret);
            return abortCode;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (SDO_ret > 0);

    *readSize = CO_SDOclientUploadBufRead(SDO_C, buf, bufSize);
    CO_SDOclientClose(SDO_C);
    return CO_SDO_AB_NONE;
}

static CO_SDO_abortCode_t _write_SDO(CO_SDOclient_t* SDO_C, uint8_t nodeId,
    uint16_t index, uint8_t subIndex,
    uint8_t* data, size_t dataSize,
    uint32_t timeoutMs = 100)
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
            ESP_LOGE("SDO", "Write abort: %d", SDO_ret);
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

// Returns true if the slave at nodeId has pushed a TPDO within the last 5 seconds.
// Used to fast-fail SDO writes against absent slaves, preventing TWAI retransmission
// storms that could starve the WDT. Allows one probe per second for discovery.
static bool isNodeReachable(uint8_t nodeId)
{
    // Check motor routes first — they have TPDO-based liveness tracking
    for (uint8_t logicalAx = 0; logicalAx < 4; logicalAx++) {
        const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::MOTOR, logicalAx);
        if (!route || route->where != UC2::RouteEntry::REMOTE) continue;
        if (route->nodeId != nodeId) continue;

        const auto& slave = CANopenModule::s_remoteSlaves[route->subAxis];
        if (!slave.seen) {
            log_e("Node 0x%02X not seen, skipping SDO operation", nodeId);
            return false;
        }
        if ((millis() - slave.lastUpdateMs) > 5000) {
            log_w("Node 0x%02X last update too old, skipping SDO operation", nodeId);
            return false;
        }
        return true;
    }

    // Non-motor peripherals (LED, LASER, GALVO, etc.) don't have TPDO feedback
    // yet.  If there is a remote route to this nodeId, allow the SDO attempt —
    // the 100 ms SDO timeout handles unresponsive nodes gracefully.
    static const UC2::RouteEntry::Type nonMotorTypes[] = {
        UC2::RouteEntry::LASER, UC2::RouteEntry::LED,
        UC2::RouteEntry::GALVO, UC2::RouteEntry::HOME,
    };
    for (auto t : nonMotorTypes) {
        for (uint8_t id = 0; id < 4; id++) {
            const auto* route = UC2::RoutingTable::find(t, id);
            if (route && route->where == UC2::RouteEntry::REMOTE && route->nodeId == nodeId)
                return true;
        }
    }

    return false;  // no route to this nodeId
}

bool CANopenModule::writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                             uint8_t* data, size_t dataSize)
{
    if (CO == NULL || CO->SDOclient == NULL) {
        log_e("SDO client not initialized");
        return false;
    }   
    // Fast-fail when slave hasn't been heard from (no recent TPDO).
    // Discovery happens via the slave's heartbeat toggle in its TPDO.
    if (!isNodeReachable(nodeId)) return false;
    if (s_sdoMutex && xSemaphoreTake(s_sdoMutex, pdMS_TO_TICKS(200)) != pdTRUE) return false;
    CO_SDO_abortCode_t ret = _write_SDO(CO->SDOclient, nodeId,
        index, subIndex, data, dataSize);
    if (s_sdoMutex) xSemaphoreGive(s_sdoMutex);
    return (ret == CO_SDO_AB_NONE);
}

bool CANopenModule::readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                            uint8_t* buf, size_t bufSize, size_t* readSize)
{
    if (CO == NULL || CO->SDOclient == NULL) return false;
    if (!isNodeReachable(nodeId)) return false;
    if (s_sdoMutex && xSemaphoreTake(s_sdoMutex, pdMS_TO_TICKS(200)) != pdTRUE) return false;
    CO_SDO_abortCode_t ret = _read_SDO(CO->SDOclient, nodeId,
        index, subIndex, buf, bufSize, readSize);
    if (s_sdoMutex) xSemaphoreGive(s_sdoMutex);
    return (ret == CO_SDO_AB_NONE);
}

bool CANopenModule::isOperational()
{
    return (CO != NULL && CO->NMT != NULL &&
            CO->NMT->operatingState == CO_NMT_OPERATIONAL);
}

// Typed SDO write helpers
bool CANopenModule::writeSDO_u8(uint8_t nodeId, uint16_t idx, uint8_t sub, uint8_t v) {
    return writeSDO(nodeId, idx, sub, &v, sizeof(v));
}
bool CANopenModule::writeSDO_u16(uint8_t nodeId, uint16_t idx, uint8_t sub, uint16_t v) {
    return writeSDO(nodeId, idx, sub, (uint8_t*)&v, sizeof(v));
}
bool CANopenModule::writeSDO_u32(uint8_t nodeId, uint16_t idx, uint8_t sub, uint32_t v) {
    return writeSDO(nodeId, idx, sub, (uint8_t*)&v, sizeof(v));
}
bool CANopenModule::writeSDO_i32(uint8_t nodeId, uint16_t idx, uint8_t sub, int32_t v) {
    return writeSDO(nodeId, idx, sub, (uint8_t*)&v, sizeof(v));
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
    if (!isNodeReachable(nodeId)) return false;
    if (s_sdoMutex && xSemaphoreTake(s_sdoMutex, pdMS_TO_TICKS(500)) != pdTRUE) return false;

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

// ============================================================================
// LED pixel data — OD extension for SDO segmented/block transfer (slave side)
// ============================================================================
#ifdef LED_CONTROLLER
#define LED_PIXEL_BUF_MAX  (3 * 256)  // 256 pixels max (RGB)
static OD_extension_t s_ledPixelExt;
static uint8_t        s_ledPixelBuffer[LED_PIXEL_BUF_MAX];
static size_t         s_ledPixelBytesReceived = 0;

static ODR_t onLedPixelWrite(OD_stream_t* stream, const void* buf,
                             OD_size_t count, OD_size_t* countWritten) {
    if (s_ledPixelBytesReceived + count > sizeof(s_ledPixelBuffer)) {
        s_ledPixelBytesReceived = 0;
        return ODR_OUT_OF_MEM;
    }
    memcpy(s_ledPixelBuffer + s_ledPixelBytesReceived, buf, count);
    s_ledPixelBytesReceived += count;
    *countWritten = count;

    // If this was the last segment, apply to the LED strip
    if (stream->dataOffset + count >= stream->dataLength) {
        uint16_t pixelCount = s_ledPixelBytesReceived / 3;
        LedController::setPixels(s_ledPixelBuffer, pixelCount);
        s_ledPixelBytesReceived = 0;
    }
    return ODR_OK;
}
#endif // LED_CONTROLLER

// ============================================================================
// CO_main task — CANopenNode init + main processing loop
// (Preserved structure from MWE, parametrised by runtimeConfig.canNodeId)
// ============================================================================
void CANopenModule::CO_main_task(void* arg)
{
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
    uint32_t heapMemoryUsed;
    void* CANptr = NULL;
    uint8_t pendingNodeId = runtimeConfig.canNodeId;    // start with the configured node ID, but allow LSS to override if it's already taken on the bus
    uint8_t activeNodeId  = runtimeConfig.canNodeId;    // for logging only — reflects the actual node ID in use, which may be overridden by LSS if there is a conflict
    uint16_t pendingBitRate = 500;

    // Allocate CANopenNode objects
    CO_config_t* config_ptr = NULL;
    CO = CO_new(config_ptr, &heapMemoryUsed);
    if (CO == NULL) {
        ESP_LOGE(TAG_CO, "CO_new failed — out of memory");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG_CO, "Allocated %lu bytes for CANopen objects", (unsigned long)heapMemoryUsed);

    while (reset != CO_RESET_APP) {
        ESP_LOGI(TAG_CO, "Communication reset (node-id=%d)...", activeNodeId);

        CO->CANmodule->CANnormal = false;
        CO_CANsetConfigurationMode((void*)&CANptr);
        CO_CANmodule_disable(CO->CANmodule);

        // Init CAN
        CO_ReturnError_t err = CO_CANinit(CO, CANptr, pendingBitRate);
        if (err != CO_ERROR_NO) {
            ESP_LOGE(TAG_CO, "CO_CANinit failed: %d", err);
            vTaskDelete(NULL);
            return;
        }

        // Init LSS
        CO_LSS_address_t lssAddress = { .identity = {
            .vendorID       = OD_PERSIST_COMM.x1018_identity.vendor_ID,
            .productCode    = OD_PERSIST_COMM.x1018_identity.productCode,
            .revisionNumber = OD_PERSIST_COMM.x1018_identity.revisionNumber,
            .serialNumber   = OD_PERSIST_COMM.x1018_identity.serialNumber
        }};
        err = CO_LSSinit(CO, &lssAddress, &pendingNodeId, &pendingBitRate);
        if (err != CO_ERROR_NO) {
            ESP_LOGE(TAG_CO, "CO_LSSinit failed: %d", err);
            vTaskDelete(NULL);
            return;
        }

        activeNodeId = pendingNodeId;
        // If LSS overrode the configured node-id (bus conflict), persist the
        // new value to runtimeConfig + NVS so subsequent boots keep it.
        if (activeNodeId != runtimeConfig.canNodeId) {
            ESP_LOGW(TAG_CO, "LSS reassigned node-id %u -> %u (saved to NVS)",
                     (unsigned)runtimeConfig.canNodeId, (unsigned)activeNodeId);
            runtimeConfig.canNodeId = activeNodeId;
            NVSConfig::saveConfig();
        }
        uint32_t errInfo = 0;

        // Init CANopen
        err = CO_CANopenInit(CO, NULL, NULL, OD, OD_STATUS_BITS,
            static_cast<CO_NMT_control_t>(NMT_CONTROL),
            FIRST_HB_TIME, SDO_SRV_TIMEOUT_TIME, SDO_CLI_TIMEOUT_TIME,
            SDO_CLI_BLOCK, activeNodeId, &errInfo);
        if (err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) {
            ESP_LOGE(TAG_CO, "CO_CANopenInit failed: %d (OD entry 0x%lX)",
                     err, (unsigned long)errInfo);
            vTaskDelete(NULL);
            return;
        }

        // Init PDOs
        err = CO_CANopenInitPDO(CO, CO->em, OD, activeNodeId, &errInfo);
        if (err != CO_ERROR_NO) {
            ESP_LOGE(TAG_CO, "PDO init failed: %d (OD entry 0x%lX)",
                     err, (unsigned long)errInfo);
            vTaskDelete(NULL);
            return;
        }

        // Register OD extensions for domain entries (slave side)
#ifdef LED_CONTROLLER
        {
            OD_entry_t* entry = OD_find(OD, UC2_OD::LED_PIXEL_DATA);
            if (entry) {
                s_ledPixelExt.object = nullptr;
                s_ledPixelExt.read   = nullptr;
                s_ledPixelExt.write  = onLedPixelWrite;
                OD_extension_init(entry, &s_ledPixelExt);
                ESP_LOGI(TAG_CO, "LED pixel data OD extension registered (0x2210)");
            }
        }
#endif

        // Register OTA OD extensions (all slave builds)
        CanOpenOTA::registerOdExtensions();

        // Start processing tasks
        xTaskCreatePinnedToCore(CO_tmr_task,       "CO_TMR", 4096, NULL,
                                CANOPEN_TMR_TASK_PRIO, NULL, tskNO_AFFINITY);
        xTaskCreatePinnedToCore(CO_interrupt_task,  "CO_INT", 4096, NULL,
                                CANOPEN_INT_TASK_PRIO, NULL, tskNO_AFFINITY);

        // Enter normal mode
        CO_CANsetNormalMode(CO->CANmodule);
        reset = CO_RESET_NOT;

        ESP_LOGI(TAG_CO, "*** CANopenNode running — node-id %d ***", activeNodeId);

        while (reset == CO_RESET_NOT) {
            reset = CO_process(CO, false, 1000, NULL);
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    CO_CANsetConfigurationMode((void*)&CANptr);
    CO_delete(CO);
    ESP_LOGI(TAG_CO, "CANopenNode finished");
    vTaskDelete(NULL);
}

// ============================================================================
// /can_get — returns node ID, NMT state, TWAI health
// ============================================================================
cJSON* CANopenModule::get(cJSON* /*doc*/)
{
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "nodeId", runtimeConfig.canNodeId);
    cJSON_AddNumberToObject(resp, "canRole", (int)runtimeConfig.canRole);

    if (CO != nullptr && CO->NMT != nullptr) {
        uint8_t nmtState = CO->NMT->operatingState;
        cJSON_AddNumberToObject(resp, "nmtState", nmtState);
        const char* stateStr = "unknown";
        switch (nmtState) {
            case CO_NMT_INITIALIZING:    stateStr = "INITIALIZING"; break;
            case CO_NMT_PRE_OPERATIONAL: stateStr = "PRE-OPERATIONAL"; break;
            case CO_NMT_OPERATIONAL:     stateStr = "OPERATIONAL"; break;
            case CO_NMT_STOPPED:         stateStr = "STOPPED"; break;
        }
        cJSON_AddStringToObject(resp, "nmtStateStr", stateStr);
    }

    // TWAI bus health
    twai_status_info_t status;
    if (twai_get_status_info(&status) == ESP_OK) {
        cJSON* bus = cJSON_CreateObject();
        cJSON_AddNumberToObject(bus, "txErr", status.tx_error_counter);
        cJSON_AddNumberToObject(bus, "rxErr", status.rx_error_counter);
        cJSON_AddNumberToObject(bus, "txFailed", status.tx_failed_count);
        cJSON_AddNumberToObject(bus, "busOffCount", status.bus_error_count);
        const char* stateStr = "unknown";
        switch (status.state) {
            case TWAI_STATE_STOPPED:   stateStr = "stopped"; break;
            case TWAI_STATE_RUNNING:   stateStr = "running"; break;
            case TWAI_STATE_BUS_OFF:   stateStr = "bus_off"; break;
            case TWAI_STATE_RECOVERING: stateStr = "recovering"; break;
        }
        cJSON_AddStringToObject(bus, "state", stateStr);
        cJSON_AddItemToObject(resp, "bus", bus);
    }
    return resp;
}

// ============================================================================
// /can_act — set node ID, persists to NVS
// {"task":"/can_act", "nodeId": 10}
// ============================================================================
cJSON* CANopenModule::act(cJSON* doc)
{
    cJSON* resp = cJSON_CreateObject();
    bool changed = false;
    // {"task":"/can_act","nodeId":11,"canMotorAxis":1}
    cJSON* nodeIdItem = cJSON_GetObjectItem(doc, "nodeId");
    if (nodeIdItem && cJSON_IsNumber(nodeIdItem) &&
        nodeIdItem->valueint >= 1 && nodeIdItem->valueint <= 127)
    {
        runtimeConfig.canNodeId = (uint8_t)nodeIdItem->valueint;
        changed = true;
    }

    // canMotorAxis: which FocusMotor index the slave dispatches CANopen commands to.
    // The slave's physical stepper is typically at index 1, not 0.
    // Example: {"task":"/can_act","canMotorAxis":1}
    cJSON* axisItem = cJSON_GetObjectItem(doc, "canMotorAxis");
    if (axisItem && cJSON_IsNumber(axisItem) &&
        axisItem->valueint >= 0 && axisItem->valueint <= 3)
    {
        runtimeConfig.canMotorAxis = (uint8_t)axisItem->valueint;
        changed = true;
    }

    if (changed) {
        NVSConfig::saveConfig();
        cJSON_AddStringToObject(resp, "status", "saved");
        cJSON_AddNumberToObject(resp, "nodeId",       runtimeConfig.canNodeId);
        cJSON_AddNumberToObject(resp, "canMotorAxis", runtimeConfig.canMotorAxis);
        cJSON_AddBoolToObject(resp, "rebooting", true);
        // Delay slightly so the HTTP response can be sent before the reboot.
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_restart();
    } else {
        cJSON_AddStringToObject(resp, "status", "ok");
        cJSON_AddNumberToObject(resp, "nodeId",       runtimeConfig.canNodeId);
        cJSON_AddNumberToObject(resp, "canMotorAxis", runtimeConfig.canMotorAxis);
    }
    return resp;
}

// ============================================================================
// Pending commands — posted by CO_tmr_task, consumed by loop() (main context).
// Per-axis arrays for motors; separate structs for homing and lasers.
// Writing 'pending = true' last acts as a commit flag (single-byte assignment is
// atomic on Xtensa, so no dedicated mutex is needed for this simple producer-
// consumer pattern).
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
static PendingAxisCmd s_axisCmds[4];

struct PendingHomingCmd {
    volatile bool pending  = false;
    int32_t speed          = 0;
    int8_t  direction      = 1;
    int32_t timeout        = 5000;
    int32_t endstopRelease = 0;
    uint8_t polarity       = 0;
};
static PendingHomingCmd s_homingCmds[4];

struct PendingLaserCmd {
    volatile bool pending = false;
    uint16_t      value   = 0;
};
static PendingLaserCmd s_laserCmds[4];

// ============================================================================
// OD <-> Module sync (called from CO_tmr_task)
// Using native UC2 OD entries (0x2000+)
// We poll this every 1ms from the timer task for simplicity and responsiveness; 
// no interrupts or mutexes needed for this one-way communication from OD to Module.
// ============================================================================
void CANopenModule::syncRpdoToModules()
{
    if (runtimeConfig.isSlave()) {
        syncRpdoToModules_slave();
    } else {
        syncRpdoToModules_master();
    }
}

// Slave: consume RPDO data from OD_RAM and dispatch to local modules
void CANopenModule::syncRpdoToModules_slave()
{
#ifdef MOTOR_CONTROLLER
    uint8_t cmdWord = OD_RAM.x2003_motor_command_word;
    if (cmdWord) {
        for (int ax = 0; ax < 4; ax++) {
            if (cmdWord & (1u << ax)) {
                // Move command for axis ax
                s_axisCmds[ax].pos   = OD_RAM.x2000_motor_target_position[ax];
                s_axisCmds[ax].speed = (int32_t)OD_RAM.x2002_motor_speed[ax];
                s_axisCmds[ax].accel = (int32_t)OD_RAM.x2006_motor_acceleration[ax];
                s_axisCmds[ax].isAbs     = OD_RAM.x2007_motor_is_absolute[ax] != 0;
                s_axisCmds[ax].isForever = OD_RAM.x200B_motor_is_forever[ax] != 0;
                s_axisCmds[ax].isStop    = false;
                s_axisCmds[ax].pending   = true;  // commit last
            }
            if (cmdWord & (1u << (ax + 4))) {
                // Stop command for axis ax
                s_axisCmds[ax].isStop  = true;
                s_axisCmds[ax].pending = true;
            }
        }
        OD_RAM.x2003_motor_command_word = 0;
    }
#endif

#ifdef HOME_MOTOR
    for (int ax = 0; ax < 4; ax++) {
        if (OD_RAM.x2010_homing_command[ax]) {
            s_homingCmds[ax].speed          = (int32_t)OD_RAM.x2011_homing_speed[ax];
            s_homingCmds[ax].direction      = OD_RAM.x2012_homing_direction[ax];
            s_homingCmds[ax].timeout        = (int32_t)OD_RAM.x2013_homing_timeout[ax];
            s_homingCmds[ax].endstopRelease = OD_RAM.x2014_homing_endstop_release[ax];
            s_homingCmds[ax].polarity       = OD_RAM.x2015_homing_endstop_polarity[ax];
            s_homingCmds[ax].pending        = true;
            OD_RAM.x2010_homing_command[ax] = 0;
        }
    }
#endif

#ifdef LASER_CONTROLLER
    for (int ch = 0; ch < 4; ch++) {
        uint16_t v = OD_RAM.x2100_laser_pwm_value[ch];
        // Only arm pending when the OD value actually changed from the last dispatched value.
        // Using OR (!pending || value!=v) caused an infinite loop: loop() clears pending,
        // syncRpdoToModules() immediately re-arms it with the same value, ad infinitum.
        if (s_laserCmds[ch].value != v) {
            s_laserCmds[ch].value   = v;
            s_laserCmds[ch].pending = true;
        }
    }
#endif

#ifdef LED_CONTROLLER
    // LED mode: detect changes to mode/brightness/colour and dispatch
    {
        static uint8_t  lastLedMode   = 0xFF;
        static uint8_t  lastLedBright = 0xFF;
        static uint32_t lastLedColour = 0xFFFFFFFF;
        uint8_t  curMode   = OD_RAM.x2200_led_array_mode;
        uint8_t  curBright = OD_RAM.x2201_led_brightness;
        uint32_t curColour = OD_RAM.x2202_led_uniform_colour;
        if (curMode != lastLedMode || curBright != lastLedBright || curColour != lastLedColour) {
            lastLedMode   = curMode;
            lastLedBright = curBright;
            lastLedColour = curColour;
            LedController::setMode(curMode, curBright, curColour);
        }
    }
    // LED pattern: detect changes to pattern id/speed
    {
        static uint8_t  lastPatternId    = 0xFF;
        static uint16_t lastPatternSpeed = 0xFFFF;
        uint8_t  curPat   = OD_RAM.x2220_led_pattern_id;
        uint16_t curSpeed = OD_RAM.x2221_led_pattern_speed;
        if (curPat != lastPatternId || curSpeed != lastPatternSpeed) {
            lastPatternId    = curPat;
            lastPatternSpeed = curSpeed;
            LedController::setPattern(curPat, curSpeed);
        }
    }
#endif

#ifdef GALVO_CONTROLLER
    // Galvo: detect changes to command word and scan parameters, dispatch to GalvoController
    {
        static uint8_t lastGalvoCmd = 0;
        uint8_t cmd = OD_RAM.x2602_galvo_command_word;
        if (cmd != lastGalvoCmd) {
            lastGalvoCmd = cmd;
            switch (cmd) {
                case 0: // Stop
                    GalvoController::stop();
                    break;
                case 1: { // Goto XY — set target position via raster config
                    ScanConfig cfg = GalvoController::getCurrentConfig();
                    cfg.x_min = (uint16_t)OD_RAM.x2600_galvo_target_position[0];
                    cfg.x_max = cfg.x_min;
                    cfg.y_min = (uint16_t)OD_RAM.x2600_galvo_target_position[1];
                    cfg.y_max = cfg.y_min;
                    cfg.nx = 1;
                    cfg.ny = 1;
                    cfg.frame_count = 1;
                    GalvoController::setConfig(cfg);
                    GalvoController::start();
                    break;
                }
                case 2: // Line scan
                case 3: { // Raster scan
                    ScanConfig cfg;
                    cfg.x_min = (uint16_t)OD_RAM.x260B_galvo_x_start;
                    cfg.y_min = (uint16_t)OD_RAM.x260C_galvo_y_start;
                    cfg.nx = OD_RAM.x2605_galvo_n_steps_line;
                    cfg.ny = (cmd == 2) ? 1 : OD_RAM.x2606_galvo_n_steps_pixel;
                    int32_t xStep = OD_RAM.x260D_galvo_x_step;
                    int32_t yStep = OD_RAM.x260E_galvo_y_step;
                    cfg.x_max = (uint16_t)(cfg.x_min + cfg.nx * xStep);
                    cfg.y_max = (uint16_t)(cfg.y_min + cfg.ny * yStep);
                    cfg.sample_period_us = (uint16_t)OD_RAM.x2604_galvo_scan_speed;
                    cfg.pre_samples = OD_RAM.x2609_galvo_t_pre_us;
                    cfg.fly_samples = OD_RAM.x260A_galvo_t_post_us;
                    cfg.frame_count = 0; // Infinite until stopped
                    cfg.enable_trigger = OD_RAM.x260F_galvo_camera_trigger_mode;
                    GalvoController::setConfig(cfg);
                    GalvoController::start();
                    break;
                }
                case 5: // Emergency stop
                    GalvoController::stop();
                    break;
                default:
                    break;
            }
            OD_RAM.x2602_galvo_command_word = 0;
        }
    }
#endif
}

// Master: drain RPDO-written OD_RAM entries into the remote slave cache.
// Called every 1 ms from CO_tmr_task. Detects running->stopped transitions
// and emits async serial notifications.
RemoteSlaveState CANopenModule::s_remoteSlaves[REMOTE_SLAVE_SLOTS] = {};

void CANopenModule::syncRpdoToModules_master()
{
#ifdef MOTOR_CONTROLLER
    for (uint8_t slot = 0; slot < REMOTE_SLAVE_SLOTS; slot++) {
        int32_t newPos    = OD_RAM.x2001_motor_actual_position[slot];
        uint8_t newStatus = OD_RAM.x2004_motor_status_word[slot];

        RemoteSlaveState& slave = s_remoteSlaves[slot];

        // Skip if nothing changed AND we've already initialised
        if (slave.seen
            && newPos    == slave.motorPosition
            && newStatus == slave.motorStatus) continue;

        bool wasRunning = slave.seen && (slave.motorStatus & 0x01) != 0;
        bool nowRunning = (newStatus & 0x01) != 0;
        bool firstSeen  = !slave.seen;

        slave.motorPosition = newPos;
        slave.motorStatus   = newStatus;
        slave.lastUpdateMs  = millis();
        slave.seen          = true;

        // Rate-limit log to once per second (keepalive toggles arrive every 2 s)
        static uint32_t s_lastRpdoLogMs = 0;
        uint32_t nowLog = millis();
        if ((nowLog - s_lastRpdoLogMs) >= 1000) {
            s_lastRpdoLogMs = nowLog;
            log_i("Remote slave slot %d update: pos=%ld status=0x%02X running=%d",
                  slot, (long)newPos, newStatus, (int)nowRunning);
        }

        // Mirror into FocusMotor data so /motor_get keeps working
        for (uint8_t logicalAx = 0; logicalAx < 4; logicalAx++) { // TODO: We should consider having more axis 
            const auto* route = UC2::RoutingTable::find(
                UC2::RouteEntry::MOTOR, logicalAx);
            if (!route || route->where != UC2::RouteEntry::REMOTE) continue;
            if (route->subAxis != slot) continue;

            MotorData* md = FocusMotor::getData()[logicalAx];
            if (md) {
                md->currentPosition = newPos;
                md->stopped         = !nowRunning;
            }

            // Detect running -> stopped transition -> report completion via QidRegistry
            if (wasRunning && !nowRunning) {
                ESP_LOGI(TAG_CO, "Slave slot %d (motor %d) DONE pos=%ld",
                         slot, logicalAx, (long)newPos);
                if (md && md->qid > 0) {
                    QidRegistry::reportActionDone(md->qid);
                }
            }

            if (firstSeen) {
                ESP_LOGI(TAG_CO, "Slave slot %d (motor %d) FIRST SEEN pos=%ld running=%d",
                         slot, logicalAx, (long)newPos, (int)nowRunning);
            }
        }
    }
#endif
}

void CANopenModule::syncModulesToTpdo()
{
    /*
    This function pushes the current module state into the TPDOs.
    It is called every 1 ms from the CO_tmr_task, but only updates 
    the TPDO data when there are changes to report (e.g. motor position/status changes).

    */
    // Always update system stats (useful for all roles)
    OD_RAM.x2503_uptime_seconds = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
    OD_RAM.x2504_free_heap_bytes = (uint32_t)esp_get_free_heap_size();

    // Only slaves need to push module state into OD for TPDOs
    if (!runtimeConfig.isSlave()) return;

#ifdef MOTOR_CONTROLLER
    int localAxis = (int)pinConfig.REMOTE_MOTOR_AXIS_ID;
    FocusMotor::updateData(localAxis);
    MotorData* md = FocusMotor::getData()[localAxis];
    if (md == nullptr) return;

    bool isRunning = FocusMotor::isRunning(localAxis);
    int32_t newPos = md->currentPosition;
    uint8_t newStatus = isRunning ? 0x01u : 0x00u;

    // Heartbeat toggle: flip bit 1 of statusWord every 1 s so that the
    // master's RPDO consumer always sees a data change and refreshes
    // lastUpdateMs, even when the motor is idle. Bit 0 = running (semantic),
    // bit 1 = heartbeat toggle (ignored by master when evaluating running).
    static uint8_t  s_heartbeatToggle = 0;
    static uint32_t s_lastKeepaliveMs = 0;
    uint32_t nowMs = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if ((nowMs - s_lastKeepaliveMs) >= 1000) {
        s_heartbeatToggle ^= 0x02;  // toggle bit 1
        s_lastKeepaliveMs  = nowMs;
    }
    newStatus |= s_heartbeatToggle;

    static int32_t s_lastPos    = INT32_MIN;
    static uint8_t s_lastStatus = 0xFF;

    if (newPos != s_lastPos || newStatus != s_lastStatus) {
        for (int ax = 0; ax < 4; ax++) {
            OD_RAM.x2001_motor_actual_position[ax] = newPos;
            OD_RAM.x2004_motor_status_word[ax]     = newStatus;
        }
        s_lastPos    = newPos;
        s_lastStatus = newStatus;

        // Request TPDO1 transmission on next CO_process_TPDO cycle
        if (CO != NULL && CO->TPDO != NULL) {
            CO_TPDOsendRequest(&CO->TPDO[0]);
        }

        static uint32_t s_lastSyncLogMs = 0;
        if (nowMs - s_lastSyncLogMs >= 1000) {
            s_lastSyncLogMs = nowMs;
            ESP_LOGI(TAG_CO, "syncModulesToTpdo: local-ax%d pos=%ld running=%d -> TPDO1",
                     localAxis, (long)newPos, (int)isRunning);
        }
    }
#endif

#ifdef GALVO_CONTROLLER
    // Galvo state: push status word into OD for TPDO
    {
        uint8_t galvoStatus = 0;
        if (GalvoController::isRunning()) galvoStatus |= 0x01;  // bit 0: moving
        if (GalvoController::isRunning()) galvoStatus |= 0x02;  // bit 1: scan active

        static uint8_t s_lastGalvoStatus = 0xFF;
        if (galvoStatus != s_lastGalvoStatus) {
            OD_RAM.x2603_galvo_status_word = galvoStatus;
            s_lastGalvoStatus = galvoStatus;
        }
    }
#endif
}

// ============================================================================
// Public API
// ============================================================================
void CANopenModule::setup()
{
    ESP_LOGI(TAG_CO, "Starting CANopen stack (node-id=%d, TX=%d, RX=%d)...",
             runtimeConfig.canNodeId, pinConfig.CAN_TX, pinConfig.CAN_RX);

    // The CANopenNode library logs every CAN frame at INFO level under these tags.
    // Reduce to WARN to prevent serial interleaving with FocusMotor/Arduino logs.
    esp_log_level_set("CO_INT",         ESP_LOG_WARN);
    esp_log_level_set("CO_INT_PROCESS", ESP_LOG_WARN);

    CAN_ctrl_task_sem = xSemaphoreCreateBinary();
    s_sdoMutex = xSemaphoreCreateMutex();
    CAN_TX_queue = xQueueCreate(32, sizeof(CANMessages));
    CAN_RX_queue = xQueueCreate(32, sizeof(CANMessages));

    // Start TWAI controller task
    xSemaphoreGive(CAN_ctrl_task_sem);
    xTaskCreatePinnedToCore(CAN_ctrl_task, "TWAI_ctrl", 4096, NULL,
                            CAN_CTRL_TASK_PRIO, NULL, tskNO_AFFINITY);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Start CANopen main task
    xTaskCreatePinnedToCore(CO_main_task, "CO_MAIN", 6000, NULL,
                            CANOPEN_TASK_PRIO, NULL, tskNO_AFFINITY);
    vTaskDelay(pdMS_TO_TICKS(100));
}

void CANopenModule::loop()
{
    // NMT state monitoring
    if (CO != NULL && CO->NMT != NULL) {
        static uint8_t prevState = 0xFF;
        uint8_t curState = CO->NMT->operatingState;
        if (curState != prevState) {
            prevState = curState;
            const char* stateStr = "unknown";
            switch (curState) {
                case CO_NMT_INITIALIZING:    stateStr = "INITIALIZING"; break;
                case CO_NMT_PRE_OPERATIONAL: stateStr = "PRE-OPERATIONAL"; break;
                case CO_NMT_OPERATIONAL:     stateStr = "OPERATIONAL"; break;
                case CO_NMT_STOPPED:         stateStr = "STOPPED"; break;
            }
            ESP_LOGI(TAG_CO, "NMT state changed -> %s (%d)", stateStr, curState);
        }
    }

    // Master: slave state is pushed via TPDO -> RPDO (see syncRpdoToModules_master).
    // No SDO polling needed. /motor_get reads from s_remoteSlaves cache.

#ifdef MOTOR_CONTROLLER
    // Dispatch pending motor move/stop commands (per OD axis).
    // runtimeConfig.canMotorAxis remaps the OD axis index (as written by the master) to
    // the local FocusMotor axis that has the physical stepper wired up.
    // Example: master always writes OD-axis 0; slave motor is at FocusMotor index 1 →
    //          canMotorAxis = 1.
    for (int ax = 0; ax < 4; ax++) {
        if (!s_axisCmds[ax].pending) continue;
        s_axisCmds[ax].pending = false;
        // pinConfig.REMOTE_MOTOR_AXIS_ID is the compile-time authoritative axis index for
        // the physical stepper on this slave board — always correct, no runtime config needed.
        int localAxis = (int)pinConfig.REMOTE_MOTOR_AXIS_ID;
        if (FocusMotor::getData()[localAxis] == nullptr) {
            ESP_LOGW(TAG_CO, "Motor dispatch: OD-ax%d -> local-ax%d is null; check REMOTE_MOTOR_AXIS_ID", ax, localAxis);
            continue;
        }
        MotorData* m = FocusMotor::getData()[localAxis];
        if (s_axisCmds[ax].isStop) {
            m->isStop    = true;
            m->isforever = false;
            log_i("Dispatch motor STOP OD-ax%d -> local-ax%d", ax, localAxis);
            FocusMotor::startStepper(localAxis, 0);
        } else {
            m->targetPosition   = s_axisCmds[ax].pos;
            m->speed            = s_axisCmds[ax].speed;
            m->absolutePosition = s_axisCmds[ax].isAbs;
            m->isforever        = s_axisCmds[ax].isForever;
            m->isStop           = false;
            m->stopped          = false;
            // Use received acceleration; keep existing value if non-zero; fall back to 40000
            if (s_axisCmds[ax].accel > 0)
                m->acceleration = s_axisCmds[ax].accel;
            else if (m->acceleration <= 0)
                m->acceleration = 40000;
            ESP_LOGI(TAG_CO, "Dispatch motor OD-ax%d -> local-ax%d: pos=%ld spd=%ld accel=%ld abs=%d",
                     ax, localAxis, (long)m->targetPosition, (long)m->speed,
                     (long)m->acceleration, m->absolutePosition);
            FocusMotor::startStepper(localAxis, 0);
        }
    }
#endif

#ifdef HOME_MOTOR
    // Dispatch pending homing commands
    for (int ax = 0; ax < 4; ax++) {
        if (!s_homingCmds[ax].pending) continue;
        s_homingCmds[ax].pending = false;
        int maxSpeed = s_homingCmds[ax].speed > 0 ? s_homingCmds[ax].speed * 2 : 1000;
        HomeMotor::startHome(ax,
            (int)s_homingCmds[ax].timeout,
            (int)s_homingCmds[ax].speed,
            maxSpeed,
            (int)s_homingCmds[ax].direction,
            (int)s_homingCmds[ax].polarity,
            (int)s_homingCmds[ax].endstopRelease,
            0  /* qid */); // TODO: we need to keep track of the qid to now if its still alive/busy
            // TODO: How about the stop command? We currently ignore it and just let the homing run until completion or timeout. We could add a "isStop" flag to PendingHomingCmd and check it here to allow stopping an ongoing homing operation.
    }
#endif

#ifdef LASER_CONTROLLER
    // Dispatch pending laser value updates (only fires on value change, see syncRpdoToModules)
    for (int ch = 0; ch < 4; ch++) {
        if (!s_laserCmds[ch].pending) continue;
        s_laserCmds[ch].pending = false;
        ESP_LOGI(TAG_CO, "Dispatch laser %d: PWM=%d", ch, (int)s_laserCmds[ch].value);
        LaserController::setLaserVal(ch, (int)s_laserCmds[ch].value);
    }
#endif
}

#endif // CAN_CONTROLLER_CANOPEN

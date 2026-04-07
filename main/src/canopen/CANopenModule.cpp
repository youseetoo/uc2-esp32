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

#ifdef CAN_CONTROLLER_CANOPEN

#include "PinConfig.h"
#include "../config/RuntimeConfig.h"
#include "../config/NVSConfig.h"

#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
#endif

extern "C" {
#include <CANopen.h>
#include "OD.h"
}

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
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

    // Build general config at runtime from pinConfig pins
    twai_general_config_t g_config = {
        .mode           = TWAI_MODE_NORMAL,
        .tx_io          = (gpio_num_t)pinConfig.CAN_TX,
        .rx_io          = (gpio_num_t)pinConfig.CAN_RX,
        .clkout_io      = TWAI_IO_UNUSED,
        .bus_off_io     = TWAI_IO_UNUSED,
        .tx_queue_len   = 20,
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

        // TX: drain queued frames from CANopenNode
        while (xQueueReceive(CAN_TX_queue, (void*)&tx_msg, 0) == pdTRUE) {
            if (twai_transmit(&tx_msg.message, 0) != ESP_OK) {
                ESP_LOGE(TAG_CAN, "TX failed");
                break;
            }
        }

        // Error recovery
        if (alerts & (TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_ERROR)) {
            ESP_LOGW(TAG_CAN, "Bus error — recovering...");
            twai_initiate_recovery();
            twai_stop();
            twai_driver_uninstall();
            vTaskDelay(pdMS_TO_TICKS(100));
            twai_driver_install(&g_config, &t_config, &f_config);
            twai_start();
            twai_reconfigure_alerts(TWAI_ALERT_ALL, NULL);
            ESP_LOGI(TAG_CAN, "Bus recovered");
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
    uint8_t* buf, size_t bufSize, size_t* readSize)
{
    CO_SDO_return_t SDO_ret;

    SDO_ret = CO_SDOclient_setup(SDO_C,
        CO_CAN_ID_SDO_CLI + nodeId,
        CO_CAN_ID_SDO_SRV + nodeId, nodeId);
    if (SDO_ret != CO_SDO_RT_ok_communicationEnd) return CO_SDO_AB_GENERAL;

    SDO_ret = CO_SDOclientUploadInitiate(SDO_C, index, subIndex, 1000, false);
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
    uint8_t* data, size_t dataSize)
{
    CO_SDO_return_t SDO_ret;

    SDO_ret = CO_SDOclient_setup(SDO_C,
        CO_CAN_ID_SDO_CLI + nodeId,
        CO_CAN_ID_SDO_SRV + nodeId, nodeId);
    if (SDO_ret != CO_SDO_RT_ok_communicationEnd) return CO_SDO_AB_NO_DATA;

    SDO_ret = CO_SDOclientDownloadInitiate(SDO_C, index, subIndex, dataSize, 1000, false);
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
        vTaskDelay(pdMS_TO_TICKS(1));
    } while (SDO_ret > 0);

    CO_SDOclientClose(SDO_C);
    return CO_SDO_AB_NONE;
}

// ============================================================================
// Public SDO API
// ============================================================================
bool CANopenModule::writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                             uint8_t* data, size_t dataSize)
{
    if (CO == NULL || CO->SDOclient == NULL) return false;
    CO_SDO_abortCode_t ret = _write_SDO(CO->SDOclient, nodeId,
        index, subIndex, data, dataSize);
    return (ret == CO_SDO_AB_NONE);
}

bool CANopenModule::readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                            uint8_t* buf, size_t bufSize, size_t* readSize)
{
    if (CO == NULL || CO->SDOclient == NULL) return false;
    CO_SDO_abortCode_t ret = _read_SDO(CO->SDOclient, nodeId,
        index, subIndex, buf, bufSize, readSize);
    return (ret == CO_SDO_AB_NONE);
}

bool CANopenModule::isOperational()
{
    return (CO != NULL && CO->NMT != NULL &&
            CO->NMT->operatingState == CO_NMT_OPERATIONAL);
}

// ============================================================================
// CO_main task — CANopenNode init + main processing loop
// (Preserved structure from MWE, parametrised by runtimeConfig.canNodeId)
// ============================================================================
void CANopenModule::CO_main_task(void* arg)
{
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
    uint32_t heapMemoryUsed;
    void* CANptr = NULL;
    uint8_t pendingNodeId = runtimeConfig.canNodeId;
    uint8_t activeNodeId  = runtimeConfig.canNodeId;
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
    cJSON* nodeIdItem = cJSON_GetObjectItem(doc, "nodeId");
    if (nodeIdItem && nodeIdItem->valueint >= 1 && nodeIdItem->valueint <= 127) {
        runtimeConfig.canNodeId = (uint8_t)nodeIdItem->valueint;
        NVSConfig::saveConfig();
        cJSON_AddNumberToObject(resp, "nodeId", runtimeConfig.canNodeId);
        cJSON_AddStringToObject(resp, "status", "saved — reboot to apply");
    } else {
        cJSON_AddNumberToObject(resp, "nodeId", runtimeConfig.canNodeId);
        cJSON_AddStringToObject(resp, "status", "ok");
    }
    return resp;
}

// ============================================================================
// OD <-> Module sync (called from CO_tmr_task)
// Using trainer OD entries (0x6000, 0x6001, 0x6200, 0x6401)
// ============================================================================
void CANopenModule::syncRpdoToModules()
{
    // Called every 1ms from CO_tmr_task AFTER CO_process_RPDO().
    // Protocol (matches DeviceRouter on master side):
    //   0x6401:01 = position high word (bits 31-16)
    //   0x6401:02 = position low  word (bits 15-0)
    //   0x6401:03 = speed (uint16)
    //   0x6401:04 = control flags (bit0=isAbs, bit1=isStop)
    //   0x6200:01 = trigger byte (bit0=1 starts move, cleared here after dispatch)

    static uint8_t prevTrigger = 0xFF;
    uint8_t trigger = OD_RAM.x6200_writeOutput8Bit[0];

    if (trigger != prevTrigger) {
        prevTrigger = trigger;
        ESP_LOGI(TAG_CO, "RPDO trigger byte: 0x%02X", trigger);

        if (trigger & 0x01) {
#ifdef MOTOR_CONTROLLER
            // Reassemble int32 position from two uint16 words
            uint32_t hi = OD_RAM.x6401_readAnalogueInput16Bit[0];
            uint32_t lo = OD_RAM.x6401_readAnalogueInput16Bit[1];
            int32_t targetPos = (int32_t)((hi << 16) | lo);
            uint16_t speedVal  = OD_RAM.x6401_readAnalogueInput16Bit[2];
            uint16_t ctrl      = OD_RAM.x6401_readAnalogueInput16Bit[3];
            bool isAbs         = (ctrl & 0x01) != 0;
            bool isStop        = (ctrl & 0x02) != 0;

            // Derive axis from node ID: A=10→0, X=11→1, Y=12→2, Z=13→3
            int axis = (int)runtimeConfig.canNodeId - 10;
            if (axis < 0 || axis > 3) axis = 0;

            ESP_LOGI(TAG_CO, "Motor cmd: axis=%d pos=%ld speed=%u isAbs=%d isStop=%d",
                     axis, (long)targetPos, speedVal, isAbs ? 1 : 0, isStop ? 1 : 0);

            if (isStop) {
                FocusMotor::getData()[axis]->isStop = true;
                FocusMotor::startStepper(axis, 0);
            } else {
                MotorData* m = FocusMotor::getData()[axis];
                m->targetPosition  = targetPos;
                m->speed           = (int32_t)speedVal;
                m->absolutePosition = isAbs;
                m->isforever       = false;
                m->isStop          = false;
                m->stopped         = false;
                FocusMotor::startStepper(axis, 0);
            }
#endif
            // Clear trigger bit so we don't re-fire, preserve other bits
            OD_RAM.x6200_writeOutput8Bit[0] &= ~0x01;
            prevTrigger = OD_RAM.x6200_writeOutput8Bit[0];
        }
    }
}

void CANopenModule::syncModulesToTpdo()
{
    // Called every 1ms from CO_tmr_task BEFORE CO_process_TPDO().
    // Write module state into OD_RAM so TPDOs broadcast it and master can SDO-read it.

    static uint32_t counter = 0;
    OD_RAM.x6001_counter = counter++;

#ifdef MOTOR_CONTROLLER
    int axis = (int)runtimeConfig.canNodeId - 10;
    if (axis >= 0 && axis <= 3) {
        // check for nullptr just in case
        if (FocusMotor::getData()[axis] == nullptr) {
            ESP_LOGW(TAG_CO, "Motor data for axis %d is null", axis);
            return;
        }
        int32_t pos = FocusMotor::getData()[axis]->currentPosition;
        bool running = FocusMotor::isRunning(axis);

        // Store actual position split into two uint16 words (mirrors DeviceRouter decode)
        OD_RAM.x6401_readAnalogueInput16Bit[0] = (uint16_t)((uint32_t)pos >> 16);
        OD_RAM.x6401_readAnalogueInput16Bit[1] = (uint16_t)((uint32_t)pos & 0xFFFF);

        // Store isRunning in x6000 input byte
        OD_RAM.x6000_readInput8Bit[0] = running ? 1 : 0;
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

    CAN_ctrl_task_sem = xSemaphoreCreateBinary();
    CAN_TX_queue = xQueueCreate(10, sizeof(CANMessages));
    CAN_RX_queue = xQueueCreate(10, sizeof(CANMessages));

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
}

#endif // CAN_CONTROLLER_CANOPEN

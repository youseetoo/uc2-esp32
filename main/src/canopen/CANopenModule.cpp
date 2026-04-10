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
// Named OD constants — generated from tools/canopen/uc2_canopen_registry.yaml.
// Constants are declared here for compile-time verification; not yet used in logic.
#include "UC2_OD_Indices.h"

#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
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

        // Bus-off: trigger hardware recovery — wait for 128 x 11 recessive bits.
        // Do NOT uninstall/reinstall here: repeated twai_driver_install() calls cause
        // ESP_ERR_NOT_FOUND in esp_intr_alloc on ESP32-S3 (interrupt already allocated).
        if (alerts & TWAI_ALERT_BUS_OFF) {
            ESP_LOGW(TAG_CAN, "Bus-off — initiating recovery...");
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
    uint8_t* buf, size_t bufSize, size_t* readSize)
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
bool CANopenModule::writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                             uint8_t* data, size_t dataSize)
{
    /*
    This function is a helper for writing SDOs from outside the CANopen stack (e.g. from the master's serial->CAN bridge).
    It uses the global CO->SDOclient object to perform a blocking SDO write.
    nodeId: target node ID
    index: SDO index => CANopen Object Dictionary entry (e.g. 0x2000 for motor position)
    subIndex: SDO subindex => specific parameter within the OD entry (e.g. 0x01 for current position)
    data: pointer to the data buffer to write (e.g. 4 bytes for int32 position)
    dataSize: size of the data buffer in bytes
    */
    if (CO == NULL || CO->SDOclient == NULL) return false;
    CO_SDO_abortCode_t ret = _write_SDO(CO->SDOclient, nodeId,
        index, subIndex, data, dataSize);
    return (ret == CO_SDO_AB_NONE);
}

bool CANopenModule::readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                            uint8_t* buf, size_t bufSize, size_t* readSize)
{
    /*
    This function is a helper for reading SDOs from outside the CANopen stack (e.g. from the master's serial->CAN bridge).
    It uses the global CO->SDOclient object to perform a blocking SDO read.
    nodeId: target node ID
    index: SDO index => CANopen Object Dictionary entry (e.g. 0x2000 for motor position)
    subIndex: SDO subindex => specific parameter within the OD entry (e.g. 0x01 for current position)
    buf: pointer to the buffer to store the read data
    bufSize: size of the buffer in bytes
    readSize: pointer to a variable to store the number of bytes actually read
    */
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
    bool changed = false;

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
    } else {
        cJSON_AddStringToObject(resp, "status", "ok");
    }
    cJSON_AddNumberToObject(resp, "nodeId",       runtimeConfig.canNodeId);
    cJSON_AddNumberToObject(resp, "canMotorAxis", runtimeConfig.canMotorAxis);
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
    bool    isAbs  = false;
    bool    isStop = false;
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
    // Called every 1ms from CO_tmr_task AFTER CO_process_RPDO().
    //
    // Motor command protocol (UC2 native):
    //   x2003_motor_command_word  — bitmask: bit i=start axis i, bit(4+i)=stop axis i
    //   x2000_motor_target_position[i] — int32 target steps
    //   x2002_motor_speed[i]          — uint32 steps/s
    //   x2006_motor_acceleration[i]   — uint32 steps/s^2
    //   x2007_motor_is_absolute[i]    — uint8 0=relative, 1=absolute
    //
    // Homing protocol:
    //   x2010_homing_command[i] != 0  — start homing for axis i (cleared after dispatch)
    //
    // Laser protocol:
    //   x2100_laser_pwm_value[ch] — direct PWM value; posted on every change

#ifdef MOTOR_CONTROLLER
    uint8_t cmdWord = OD_RAM.x2003_motor_command_word;
    if (cmdWord) {
        for (int ax = 0; ax < 4; ax++) {
            if (cmdWord & (1u << ax)) {
                // Move command for axis ax
                s_axisCmds[ax].pos   = OD_RAM.x2000_motor_target_position[ax];
                s_axisCmds[ax].speed = (int32_t)OD_RAM.x2002_motor_speed[ax];
                s_axisCmds[ax].accel = (int32_t)OD_RAM.x2006_motor_acceleration[ax];
                s_axisCmds[ax].isAbs = OD_RAM.x2007_motor_is_absolute[ax] != 0;
                s_axisCmds[ax].isStop = false;
                s_axisCmds[ax].pending = true;  // commit last
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
}

void CANopenModule::syncModulesToTpdo()
{
    // Called every 1ms from CO_tmr_task BEFORE CO_process_TPDO().
    // Write module state into OD_RAM so TPDOs broadcast it and master can SDO-read it.

    // Always update system stats
    OD_RAM.x2503_uptime_seconds = (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
    OD_RAM.x2504_free_heap_bytes = (uint32_t)esp_get_free_heap_size();

#ifdef MOTOR_CONTROLLER
    for (int ax = 0; ax < 4; ax++) {
        if (FocusMotor::getData()[ax] == nullptr) continue;
        OD_RAM.x2001_motor_actual_position[ax] = FocusMotor::getData()[ax]->currentPosition;
        OD_RAM.x2004_motor_status_word[ax]     = FocusMotor::isRunning(ax) ? 0x01u : 0x00u;
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

#ifdef MOTOR_CONTROLLER
    // Dispatch pending motor move/stop commands (per axis)
    for (int ax = 0; ax < 4; ax++) {
        if (!s_axisCmds[ax].pending) continue;
        s_axisCmds[ax].pending = false;
        if (FocusMotor::getData()[ax] == nullptr) continue;
        MotorData* m = FocusMotor::getData()[ax];
        if (s_axisCmds[ax].isStop) {
            m->isStop = true;
            FocusMotor::startStepper(ax, 0);
        } else {
            m->targetPosition   = s_axisCmds[ax].pos;
            m->speed            = s_axisCmds[ax].speed;
            m->absolutePosition = s_axisCmds[ax].isAbs;
            m->isforever        = false; //TODO: we need to provide acceleration, isforever, etc. too
            m->isStop           = false;
            m->stopped          = false;
            if (s_axisCmds[ax].accel > 0) m->acceleration = s_axisCmds[ax].accel;
            else if (m->acceleration <= 0) m->acceleration = 40000;
            ESP_LOGI(TAG_CO, "Dispatch motor %d: pos=%ld spd=%ld abs=%d",
                     ax, (long)m->targetPosition, (long)m->speed, m->absolutePosition);
            FocusMotor::startStepper(ax, 0);
        }
    } // TODO: How about stopStepper?
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

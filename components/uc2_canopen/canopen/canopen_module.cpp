/**
 * canopen_module.cpp
 * T4.4: split from CANopenModule.cpp (behavior-identical). Core: TWAI I/O task, CANopenNode tasks, OD extensions, get/act, setup/loop.
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
// T4.4: shared across the split TUs — extern in canopen_module_priv.h.
SemaphoreHandle_t s_sdoMutex = NULL;

// Updated whenever the TWAI driver reports a BUS_ERROR alert (e.g. missing
// ACK because no other node is on the bus).  Used by isNodeReachable() to
// fast-fail SDO writes when the bus is unhealthy — avoids burning ~500 ms
// per write timeout when the cable is unplugged.
volatile uint32_t s_lastBusErrorMs = 0;

// Per-node liveness timestamp, indexed by CANopen nodeId (1..127).
// Updated from the TWAI RX drain on any frame whose source nodeId matches
// a known function code (TPDO/SDO-resp/heartbeat). Used by isNodeReachable
// to fast-fail SDO writes against absent slaves uniformly across all
// peripheral types (motor, laser, LED, galvo, ...).
volatile uint32_t s_nodeLastSeenMs[128] = {0};

// These queues are extern'd by CO_driver_ESP32.c in the library
QueueHandle_t CAN_TX_queue;
QueueHandle_t CAN_RX_queue;

CO_t* CO = NULL;

// Set to CO_RESET_COMM from any context to trigger a live node-ID reload
// without esp_restart(). CO_main_task checks this in its inner loop.
volatile CO_NMT_reset_cmd_t s_requestedReset = CO_RESET_NOT;

#include "canopen_module_priv.h"


// ============================================================================
// GPIO-slave TPDO2 forwarder (master only)
// ----------------------------------------------------------------------------
// Decode the 8-byte payload pushed by the GPIO slave and emit a serial JSON
// frame that downstream tools (ImSwitch, the PS4 bridge, …) already know how
// to consume. The format intentionally mirrors what the slave would print
// locally if /gpio_get were polled — only the transport differs.
// ============================================================================
static void forwardGpioSlaveTpdo(uint8_t nodeId, const uint8_t* d)
{
    if (!d) return;

    uint8_t  estop  = d[0];
    uint8_t  in2    = d[1];
    uint8_t  in3    = d[2];
    uint8_t  flags  = d[3];
    uint16_t adcF   = (uint16_t)d[4] | ((uint16_t)d[5] << 8);
    uint16_t adcR   = (uint16_t)d[6] | ((uint16_t)d[7] << 8);

    bool trip  = (flags & 0x01) != 0;
    bool estopFlag = (flags & 0x02) != 0;

    // Deduplicate so we only emit serial JSON on a *digital edge*.
    //
    // The slave's TPDO2 has a 1 s event-timer heartbeat (OD x1801 sub 5),
    // and OD_RAM.x2310 is rewritten with the latest filtered ADC every loop
    // tick. If we included `adcF` in the predicate, every heartbeat with the
    // same digital state but a slightly different ADC reading would
    // re-emit — which is exactly the "periodic updates we don't want"
    // symptom the bench test showed.
    //
    // Dedup keys are the digital inputs and the flags byte only. The ADC
    // values still ride in the JSON payload so downstream consumers see the
    // ADC level at the moment of the edge, but a heartbeat that brings only
    // a drifted ADC value goes silently into the bit bucket here.
    static uint8_t s_lastEstop = 0xFF, s_lastIn2 = 0xFF, s_lastIn3 = 0xFF;
    static uint8_t s_lastFlags = 0xFF;
    if (estop == s_lastEstop && in2 == s_lastIn2 && in3 == s_lastIn3 &&
        flags == s_lastFlags) {
        // Optional: heartbeat-rate log so we can confirm the slave is alive
        // without flooding serial. Once every 10 s is plenty for diagnostics.
        static uint32_t s_lastHbLogMs = 0;
        uint32_t nowMs = millis();
        if ((nowMs - s_lastHbLogMs) >= 10000) {
            s_lastHbLogMs = nowMs;
            log_d("gpio slave heartbeat  node=%u flags=0x%02X adc=%u (no edge, suppressed)",
                  nodeId, (unsigned)flags, (unsigned)adcF);
        }
        return;
    }
    s_lastEstop = estop; s_lastIn2 = in2; s_lastIn3 = in3;
    s_lastFlags = flags;
    log_i("gpio slave EDGE  node=%u estop=%u in2=%u in3=%u flags=0x%02X adc=%u",
          nodeId, (unsigned)estop, (unsigned)in2, (unsigned)in3,
          (unsigned)flags, (unsigned)adcF);

    cJSON* root = cJSON_CreateObject();
    if (!root) return;
    cJSON* gpio = cJSON_AddObjectToObject(root, "gpio");
    if (gpio) {
        // "event":1 marks this as an asynchronously PUSHED edge (TPDO2), as
        // opposed to a /gpio_get response which carries the same "gpio" key.
        // UC2-REST uses this to fire collision callbacks only on real events.
        cJSON_AddNumberToObject(gpio, "event",    1);
        cJSON_AddNumberToObject(gpio, "node",     nodeId);
        cJSON_AddNumberToObject(gpio, "estop",    estopFlag ? 1 : 0);
        cJSON_AddNumberToObject(gpio, "estopPin", estop);
        cJSON_AddNumberToObject(gpio, "in2",      in2);
        cJSON_AddNumberToObject(gpio, "in3",      in3);
        cJSON_AddNumberToObject(gpio, "flags",    flags);
        cJSON_AddNumberToObject(gpio, "trip",     trip ? 1 : 0);
        cJSON_AddNumberToObject(gpio, "filtered", adcF);
        cJSON_AddNumberToObject(gpio, "raw",      adcR);
        char* s = cJSON_PrintUnformatted(root);
        if (s) {
            serial_out_safe_send_json_string(s);
            free(s);
        }
    }
    cJSON_Delete(root);
}

// ============================================================================
// PTZ-bridge TPDO2 forwarder (master only)
// ----------------------------------------------------------------------------
// The PTZ keyboard bridge (UC2_canopen_bridge_ptz) pushes discrete key events
// on its TPDO2 using the shared x1A01 mapping. Payload (see PtzRouter.cpp,
// emitEventTpdo):
//   d[0] = event type (1=preset call, 2=preset set, 3=preset clear,
//          4=aux on, 5=aux off, 6=iris open, 7=iris close,
//          8=camera on/off key, 9=autoscan key)
//   d[1] = argument (preset / aux number)
//   d[2] = sequence number — dedup key across the 1 s TPDO heartbeat
//   d[3] = 0x80 magic marker ("this is a PTZ event, not GPIO state")
// Downstream (UC2-REST / ImSwitch) maps these freely, e.g. preset call 1 →
// snap image, AUX 1 on/off → laser, iris open/close → LED brightness.
// ============================================================================
static void forwardPtzSlaveTpdo(uint8_t nodeId, const uint8_t* d)
{
    if (!d || d[3] != 0x80) return; // not a PTZ event payload

    static uint8_t s_lastSeq = 0xFF;
    if (d[2] == s_lastSeq) return;  // TPDO heartbeat repeat, not a new event
    s_lastSeq = d[2];

    static const char* kTypeNames[] = {
        "none", "preset_call", "preset_set", "preset_clear",
        "aux_on", "aux_off", "iris_open", "iris_close",
        "camera_onoff", "autoscan"
    };
    const uint8_t type = d[0];
    const char* name = (type < sizeof(kTypeNames) / sizeof(kTypeNames[0]))
                           ? kTypeNames[type] : "unknown";
    log_i("ptz bridge EVENT  node=%u type=%s arg=%u seq=%u",
          nodeId, name, (unsigned)d[1], (unsigned)d[2]);

    cJSON* root = cJSON_CreateObject();
    if (!root) return;
    cJSON* ptz = cJSON_AddObjectToObject(root, "ptz");
    if (ptz) {
        cJSON_AddNumberToObject(ptz, "event", 1);
        cJSON_AddNumberToObject(ptz, "node",  nodeId);
        cJSON_AddNumberToObject(ptz, "type",  type);
        cJSON_AddStringToObject(ptz, "name",  name);
        cJSON_AddNumberToObject(ptz, "arg",   d[1]);
        cJSON_AddNumberToObject(ptz, "seq",   d[2]);
        char* s = cJSON_PrintUnformatted(root);
        if (s) {
            serial_out_safe_send_json_string(s);
            free(s);
        }
    }
    cJSON_Delete(root);
}

// Remote-set helper for GPIO1 / GPIO4 on the slave. id ∈ {1, 2} maps to the
// slave's DIGITAL_OUT_1 / DIGITAL_OUT_2. Pass val = -1 to fire a brief pulse
// (the slave treats 0xFF as the pulse sentinel).
bool CANopenModule_setRemoteGpio(uint8_t nodeId, uint8_t id, int val)
{
    if (id < 1 || id > 2) return false;
    uint8_t v = (val < 0) ? 0xFF : (uint8_t)(val ? 1 : 0);
    return CANopenModule::writeSDO_u8(nodeId, 0x2301, id, v);
}

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
        .controller_id  = 0,   // T1.3: IDF 6.1 twai_general_config_t (first field)
        .mode           = TWAI_MODE_NORMAL,
        .tx_io          = (gpio_num_t)pinConfig.CAN_TX,
        .rx_io          = (gpio_num_t)pinConfig.CAN_RX,
        .clkout_io      = TWAI_IO_UNUSED,
        .bus_off_io     = TWAI_IO_UNUSED,
        .tx_queue_len   = 0,
        .rx_queue_len   = 64,   // bumped from 20: a 256-B SDO chunk = ~37
                                // CAN frames hit the slave back-to-back; a
                                // small queue + 1 ms task tick can drop
                                // frames and trigger SDO server timeout
                                // (abort 0x05040000) on the OTA stream.
        .alerts_enabled = TWAI_ALERT_NONE,
        .clkout_divider = 0,
        .intr_flags     = ESP_INTR_FLAG_LEVEL1,
        .general_flags  = {0}  // T1.3: IDF 6.1 twai_general_config_t (last field)
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
        // Block until the driver raises an alert (TX done, RX frame, bus
        // state change) so queued frames go out back-to-back at wire rate
        // instead of one per 1 ms tick — the old polling capped SDO block
        // download at ~7 KB/s. The 1-tick timeout is the fallback for frames
        // another task queued while the bus was idle; on timeout the driver
        // sets alerts = 0 so the checks below are still valid.
        twai_read_alerts(&alerts, pdMS_TO_TICKS(1));

        // RX: drain TWAI FIFO into CAN_RX_queue (for CANopenNode to process)
        if (alerts & TWAI_ALERT_RX_DATA) {
            while (twai_receive(&rx_message, 0) == ESP_OK) {
                // Universal node liveness: any CANopen frame whose source
                // nodeId is 1..127 marks that node as alive. COB-ID layout:
                //   bits 10..7 = function code, bits 6..0 = source nodeId.
                // We accept TPDO1..4 (0x180/0x280/0x380/0x480), SDO server
                // response (0x580), and heartbeat (0x700). Note: this is
                // master-side bookkeeping — on a slave the table simply
                // reflects whatever other nodes happen to chatter.
                if (!(rx_message.flags & TWAI_MSG_FLAG_EXTD)) {
                    uint16_t cobid = rx_message.identifier & 0x7FF;
                    uint16_t fc    = cobid & 0x780;
                    uint8_t  src   = cobid & 0x7F;
                    if (src >= 1 && src <= 127 &&
                        (fc == 0x180 || fc == 0x280 || fc == 0x380 ||
                         fc == 0x480 || fc == 0x580 || fc == 0x700)) {
                        // One-time log the first time a node is heard from, so the
                        // cold-start path is observable. fc==0x700 == heartbeat/
                        // bootup, i.e. the slave announcing itself (no polling).
                        if (s_nodeLastSeenMs[src] == 0)
                            log_i("CAN node 0x%02X first seen (fc=0x%03X)", src, fc);
                        s_nodeLastSeenMs[src] = millis();
                    }

                    // ── GPIO-slave TPDO2 sniffer (master only) ───────────
                    // The master's four RPDOs are saturated by motor slots,
                    // so the GPIO slave's TPDO2 is consumed by a direct
                    // TWAI hook instead. Payload layout matches OD x1A01
                    // (4x u8 digital + 2x u16 analog).
                    if (runtime_config_is_master(&runtimeConfig) &&
                        pinConfig.MASTER_GPIO_SLAVE_NODE_ID > 0 &&
                        fc == 0x280 &&
                        src == (uint8_t)pinConfig.MASTER_GPIO_SLAVE_NODE_ID &&
                        rx_message.data_length_code >= 8)
                    {
                        forwardGpioSlaveTpdo(src, rx_message.data);
                    }

                    // ── PTZ-bridge TPDO2 sniffer (master only) ───────────
                    // Same transport as the GPIO slave, PTZ key-event
                    // payload (0x80 marker + seq dedup inside).
                    if (runtime_config_is_master(&runtimeConfig) &&
                        pinConfig.MASTER_PTZ_NODE_ID > 0 &&
                        fc == 0x280 &&
                        src == (uint8_t)pinConfig.MASTER_PTZ_NODE_ID &&
                        rx_message.data_length_code >= 8)
                    {
                        forwardPtzSlaveTpdo(src, rx_message.data);
                    }
                }
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

        // Re-queue any frames CO_CANsend had to defer (bufferFull) because
        // CAN_TX_queue was full. This is the TX-complete stand-in the driver
        // documents but nothing was calling — without it a single queue-full
        // episode leaves CANtxCount>0 and CO_CANsend wedged at TX_OVERFLOW.
        // Cheap: returns immediately when nothing is pending.
        if (CO != NULL && CO->CANmodule != NULL)
            CO_CANtx_retryQueued(CO->CANmodule);

        // Bus-off: flush our TX queue (frames are now stale) then
        // trigger hardware recovery.
        if (alerts & TWAI_ALERT_BUS_OFF) {
            log_i("Bus-off — flushing TX queue and initiating recovery...");
            while (xQueueReceive(CAN_TX_queue, (void*)&tx_msg, 0) == pdTRUE) {}
            // Reconcile CANopenNode's TX bookkeeping with the now-empty queue:
            // the flushed frames left buffers stuck bufferFull=true with
            // CANtxCount>0. Without this reset, CO_CANsend returns TX_OVERFLOW
            // forever after recovery and every SDO write hangs — the exact
            // "must reboot the master after a bus-off" failure.
            if (CO != NULL && CO->CANmodule != NULL)
                CO_CANclearPendingTx(CO->CANmodule);
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
            s_lastBusErrorMs = millis();
            static uint32_t busErrCount = 0;
            if ((++busErrCount % 10000) == 1) {
                log_i("CAN bus errors: %u (normal when master absent)", (unsigned)busErrCount);
            }
        }
        // No vTaskDelay here: twai_read_alerts() at the top of the loop is
        // the blocking point (1 tick max), so this task never spins.
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
        if (CO != NULL && CO->CANmodule != NULL && CO->CANmodule->CANnormal) {
            // Drain all pending RX messages each tick for responsive SDO
            while (uxQueueMessagesWaiting(CAN_RX_queue) > 0) {
                CO_CANinterrupt(CO->CANmodule);
            }
        }
        vTaskDelay(1);
    }
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
        led_set_pixels(s_ledPixelBuffer, pixelCount);
        s_ledPixelBytesReceived = 0;
    }
    return ODR_OK;
}

// 0x2211 — LED_SINGLE_PIXEL: 5-byte payload {u16 index, u8 r, u8 g, u8 b}.
// Each complete write drives one LED at the given index, leaving the rest
// of the strip untouched.  Atomic small write — no buffering needed.
static OD_extension_t s_ledSinglePixelExt;
static uint8_t        s_ledSinglePixelBuf[5];
static size_t         s_ledSinglePixelBytes = 0;

static ODR_t onLedSinglePixelWrite(OD_stream_t* stream, const void* buf,
                                   OD_size_t count, OD_size_t* countWritten) {
    if (s_ledSinglePixelBytes + count > sizeof(s_ledSinglePixelBuf)) {
        s_ledSinglePixelBytes = 0;
        return ODR_OUT_OF_MEM;
    }
    memcpy(s_ledSinglePixelBuf + s_ledSinglePixelBytes, buf, count);
    s_ledSinglePixelBytes += count;
    *countWritten = count;
    if (stream->dataOffset + count >= stream->dataLength) {
        uint16_t index = (uint16_t)s_ledSinglePixelBuf[0] |
                         ((uint16_t)s_ledSinglePixelBuf[1] << 8);
        uint8_t r = s_ledSinglePixelBuf[2];
        uint8_t g = s_ledSinglePixelBuf[3];
        uint8_t b = s_ledSinglePixelBuf[4];
        led_set_single(index, r, g, b);
        s_ledSinglePixelBytes = 0;
    }
    return ODR_OK;
}

// 0x2212 — LED_SHAPE: 5-byte payload {u8 shape, u8 radius, u8 r, u8 g, u8 b}.
// shape: 0 = rings → led_draw_rings(radius, r, g, b)
//        1 = circle → led_draw_circle(radius, r, g, b)
static OD_extension_t s_ledShapeExt;
static uint8_t        s_ledShapeBuf[5];
static size_t         s_ledShapeBytes = 0;

static ODR_t onLedShapeWrite(OD_stream_t* stream, const void* buf,
                             OD_size_t count, OD_size_t* countWritten) {
    if (s_ledShapeBytes + count > sizeof(s_ledShapeBuf)) {
        s_ledShapeBytes = 0;
        return ODR_OUT_OF_MEM;
    }
    memcpy(s_ledShapeBuf + s_ledShapeBytes, buf, count);
    s_ledShapeBytes += count;
    *countWritten = count;
    if (stream->dataOffset + count >= stream->dataLength) {
        uint8_t shape  = s_ledShapeBuf[0];
        uint8_t radius = s_ledShapeBuf[1];
        uint8_t r      = s_ledShapeBuf[2];
        uint8_t g      = s_ledShapeBuf[3];
        uint8_t b      = s_ledShapeBuf[4];
        if (shape == 1) led_draw_circle(radius, r, g, b);
        else            led_draw_rings(radius, r, g, b);
        s_ledShapeBytes = 0;
    }
    return ODR_OK;
}
#endif // LED_CONTROLLER

// ============================================================================
// Galvo arbitrary points — OD extension for SDO domain transfer (slave side)
// Mirrors the LED pixel pattern: the master streams a packed byte buffer to
// 0x2610 (GALVO_POINTS_DATA); we reassemble it, parse the header + point list,
// and start an arbitrary-point scan. Wire format:
//   GalvoPointsHeader { u8 trigger_mode, u16 point_count }   (packed, 3 bytes)
//   point_count x ArbitraryScanPoint { u16 x, u16 y, u32 dwell_us }  (8 bytes)
// ============================================================================
#ifdef GALVO_CONTROLLER
#define GALVO_POINTS_BUF_MAX (sizeof(GalvoPointsHeader) + \
                              SCANNER_MAX_ARBITRARY_POINTS * sizeof(ArbitraryScanPoint))
static OD_extension_t s_galvoPointsExt;
static uint8_t        s_galvoPointsBuffer[GALVO_POINTS_BUF_MAX];
static size_t         s_galvoPointsBytesReceived = 0;

static ODR_t onGalvoPointsWrite(OD_stream_t* stream, const void* buf,
                                OD_size_t count, OD_size_t* countWritten) {
    if (s_galvoPointsBytesReceived + count > sizeof(s_galvoPointsBuffer)) {
        s_galvoPointsBytesReceived = 0;
        return ODR_OUT_OF_MEM;
    }
    memcpy(s_galvoPointsBuffer + s_galvoPointsBytesReceived, buf, count);
    s_galvoPointsBytesReceived += count;
    *countWritten = count;

    // Not the last segment yet — keep buffering.
    if (stream->dataOffset + count < stream->dataLength) {
        return ODR_OK;
    }

    // Final segment: parse and launch. Reset the accumulator first so a
    // malformed transfer can't wedge the next one.
    size_t total = s_galvoPointsBytesReceived;
    s_galvoPointsBytesReceived = 0;

    if (total < sizeof(GalvoPointsHeader)) {
        log_w("Galvo points: short transfer (%u bytes)", (unsigned)total);
        return ODR_OK;
    }
    GalvoPointsHeader hdr;
    memcpy(&hdr, s_galvoPointsBuffer, sizeof(hdr));
    uint16_t n = hdr.point_count;
    size_t expected = sizeof(GalvoPointsHeader) + (size_t)n * sizeof(ArbitraryScanPoint);
    if (n == 0 || n > SCANNER_MAX_ARBITRARY_POINTS || total < expected) {
        log_w("Galvo points: bad transfer count=%u total=%u expected=%u",
              (unsigned)n, (unsigned)total, (unsigned)expected);
        return ODR_OK;
    }

    ArbitraryScanPoint points[SCANNER_MAX_ARBITRARY_POINTS];
    memcpy(points, s_galvoPointsBuffer + sizeof(GalvoPointsHeader),
           (size_t)n * sizeof(ArbitraryScanPoint));
    log_i("Galvo points received: %u points, trigger_mode=%u",
          (unsigned)n, (unsigned)hdr.trigger_mode);

    if (galvo_set_arbitrary_points(points, n)) {
        galvo_set_scan_mode(SCAN_MODE_ARBITRARY);
        galvo_set_trigger_mode(hdr.trigger_mode);
        galvo_start();
    } else {
        log_w("Galvo points: setArbitraryPoints failed");
    }
    return ODR_OK;
}
#endif // GALVO_CONTROLLER

// ============================================================================
// CO_main task — CANopenNode init + main processing loop
// (Preserved structure from MWE, parametrised by runtimeConfig.canNodeId)
// ============================================================================
// Populate the read-only SYSTEM OD strings (0x2500 fw version, 0x2508 build
// timestamp, 0x2509 MAC) and derive the CANopen identity serial number from the
// factory MAC. Called once at startup before CO_LSSinit() reads x1018. The MAC
// makes x1018:4 (serialNumber) globally unique — a prerequisite for any future
// LSS-based node-id assignment — and lets the master report each node's MAC in
// the bus scan.
static void populateSystemOD()
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    snprintf(OD_RAM.x2500_firmware_version_string,
             sizeof(OD_RAM.x2500_firmware_version_string), "UC2-ESP v2.0"); // TODO: Replace this with versioned firmware string from build system
    snprintf(OD_RAM.x2508_build_timestamp,
             sizeof(OD_RAM.x2508_build_timestamp), "%s %s", __DATE__, __TIME__);
    snprintf(OD_RAM.x2509_mac_address, sizeof(OD_RAM.x2509_mac_address),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Lower 32 bits of the MAC → unique CANopen serial number (x1018:4).
    OD_PERSIST_COMM.x1018_identity.serialNumber =
        ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16) |
        ((uint32_t)mac[4] << 8)  |  (uint32_t)mac[5];

    log_i("System OD: fw='%s' build='%s' mac=%s serial=0x%08lX",
          OD_RAM.x2500_firmware_version_string, OD_RAM.x2508_build_timestamp,
          OD_RAM.x2509_mac_address,
          (unsigned long)OD_PERSIST_COMM.x1018_identity.serialNumber);
}

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
        log_e("CO_new failed — out of memory");
        vTaskDelete(NULL);
        return;
    }
    log_i("Allocated %lu bytes for CANopen objects", (unsigned long)heapMemoryUsed);

    // Fill in fw-version / build-timestamp / MAC OD strings and the MAC-derived
    // identity serial before LSS init (below) reads x1018.
    populateSystemOD();

    // Subtasks are started once here and idle safely via CANnormal guard during
    // any subsequent CO_RESET_COMM re-init cycle.
    xTaskCreatePinnedToCore(CO_tmr_task,      "CO_TMR", 4096, NULL,
                            CANOPEN_TMR_TASK_PRIO, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(CO_interrupt_task, "CO_INT", 4096, NULL,
                            CANOPEN_INT_TASK_PRIO, NULL, tskNO_AFFINITY);

    while (reset != CO_RESET_APP) {
        // Re-read node ID on every comm reset so a live update (via s_requestedReset)
        // takes effect without esp_restart().
        pendingNodeId = runtimeConfig.canNodeId;
        activeNodeId  = pendingNodeId;
        log_i("Communication reset (node-id=%d)...", activeNodeId);

        CO->CANmodule->CANnormal = false;
        CO_CANsetConfigurationMode((void*)&CANptr);
        CO_CANmodule_disable(CO->CANmodule);

        // Init CAN
        CO_ReturnError_t err = CO_CANinit(CO, CANptr, pendingBitRate);
        if (err != CO_ERROR_NO) {
            log_e("CO_CANinit failed: %d", err);
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
            log_e("CO_LSSinit failed: %d", err);
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
            nvsconfig_save();
        }
        uint32_t errInfo = 0;

        // Init CANopen
        err = CO_CANopenInit(CO, NULL, NULL, OD, OD_STATUS_BITS,
            static_cast<CO_NMT_control_t>(NMT_CONTROL),
            FIRST_HB_TIME, SDO_SRV_TIMEOUT_TIME, SDO_CLI_TIMEOUT_TIME,
            SDO_CLI_BLOCK, activeNodeId, &errInfo);
        if (err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) {
            log_e("CO_CANopenInit failed: %d (OD entry 0x%lX)",
                     err, (unsigned long)errInfo);
            vTaskDelete(NULL);
            return;
        }

        // Init PDOs
        err = CO_CANopenInitPDO(CO, CO->em, OD, activeNodeId, &errInfo);
        if (err != CO_ERROR_NO) {
            log_e("PDO init failed: %d (OD entry 0x%lX)",
                     err, (unsigned long)errInfo);
            vTaskDelete(NULL);
            return;
        }

#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
        // Master converts inbound axis-fault EMCY (from motor slaves) into
        // {"axisEvent":...} JSON on the host serial channel (design v2, WP5).
        if (runtime_config_is_master(&runtimeConfig)) {
            CO_EM_initCallbackRx(CO->em, axisEmcyRxCallback);
        }
#endif

        // Register OD extensions for domain entries (slave side)
#ifdef LED_CONTROLLER
        {
            OD_entry_t* entry = OD_find(OD, UC2_OD::LED_PIXEL_DATA);
            if (entry) {
                s_ledPixelExt.object = nullptr;
                s_ledPixelExt.read   = nullptr;
                s_ledPixelExt.write  = onLedPixelWrite;
                OD_extension_init(entry, &s_ledPixelExt);
                log_i("LED pixel data OD extension registered (0x2210)");
            }
            OD_entry_t* singleEntry = OD_find(OD, UC2_OD::LED_SINGLE_PIXEL);
            if (singleEntry) {
                s_ledSinglePixelExt.object = nullptr;
                s_ledSinglePixelExt.read   = nullptr;
                s_ledSinglePixelExt.write  = onLedSinglePixelWrite;
                OD_extension_init(singleEntry, &s_ledSinglePixelExt);
                log_i("LED single pixel OD extension registered (0x2211)");
            }
            OD_entry_t* shapeEntry = OD_find(OD, UC2_OD::LED_SHAPE);
            if (shapeEntry) {
                s_ledShapeExt.object = nullptr;
                s_ledShapeExt.read   = nullptr;
                s_ledShapeExt.write  = onLedShapeWrite;
                OD_extension_init(shapeEntry, &s_ledShapeExt);
                log_i("LED shape OD extension registered (0x2212)");
            }
        }
#endif

#ifdef GALVO_CONTROLLER
        {
            OD_entry_t* gpEntry = OD_find(OD, UC2_OD::GALVO_POINTS_DATA);
            if (gpEntry) {
                s_galvoPointsExt.object = nullptr;
                s_galvoPointsExt.read   = nullptr;
                s_galvoPointsExt.write  = onGalvoPointsWrite;
                OD_extension_init(gpEntry, &s_galvoPointsExt);
                log_i("Galvo points OD extension registered (0x2610)");
            }
        }
#endif

        // Register OTA OD extensions (slave receiver — opt out via CANOPEN_OTA_NO_RECEIVER)
#ifndef CANOPEN_OTA_NO_RECEIVER
        CanOpenOTA::registerOdExtensions();
#endif

        // Enter normal mode — subtasks resume processing via CANnormal guard
        CO_CANsetNormalMode(CO->CANmodule);
        reset = CO_RESET_NOT;

        log_i("*** CANopenNode running - node-id %d ***", activeNodeId);
        // One-shot dump of the effective SDO configuration so we can verify
        // at boot whether BLOCK transfer is actually compiled in (a common
        // failure mode is editing CO_driver_target.h but the library .c
        // files not getting recompiled by PlatformIO incremental builds).
        log_i("SDO cfg: CLI=0x%04X (BLOCK=%d) CLI_BUF=%d  "
            "SRV=0x%04X (BLOCK=%d) SRV_BUF=%d  CRC16=0x%02X",
            (unsigned)CO_CONFIG_SDO_CLI,
            ((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_BLOCK) ? 1 : 0,
            (int)CO_CONFIG_SDO_CLI_BUFFER_SIZE,
            (unsigned)CO_CONFIG_SDO_SRV,
            ((CO_CONFIG_SDO_SRV) & CO_CONFIG_SDO_SRV_BLOCK) ? 1 : 0,
            (int)CO_CONFIG_SDO_SRV_BUFFER_SIZE,
            (unsigned)CO_CONFIG_CRC16);

        while (reset == CO_RESET_NOT) {
            if (s_requestedReset != CO_RESET_NOT) {
                reset = s_requestedReset;
                s_requestedReset = CO_RESET_NOT;
                break;
            }
            reset = CO_process(CO, false, 1000, NULL);
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    CO_CANsetConfigurationMode((void*)&CANptr);
    CO_delete(CO);
    log_i("CANopenNode finished");
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

// Read a VISIBLE_STRING OD entry (sub 0) from a remote node into a NUL-terminated
// buffer. Returns true on success. Used by the bus scan to fetch each slave's
// build timestamp (0x2508), firmware version (0x2500) and MAC (0x2509).
static bool readNodeString(uint8_t nodeId, uint16_t index, char* out, size_t outSize)
{
    if (!out || outSize == 0) return false;
    out[0] = '\0';
    uint8_t buf[40];
    size_t  readSize = 0;
    if (!CANopenModule::readSDO(nodeId, index, 0x00, buf, sizeof(buf), &readSize))
        return false;
    if (readSize >= outSize) readSize = outSize - 1;
    memcpy(out, buf, readSize);
    out[readSize] = '\0';
    return true;
}

// ============================================================================
// /can_act — set node ID, persists to NVS; also handles bus scan + remote
// reboot for backward compatibility with the pre-CANopen Python client.
// ----------------------------------------------------------------------------
//   {"task":"/can_act", "nodeId": 10, "canMotorAxis": 1}  — local config
//   {"task":"/can_act", "scan": true, "qid": <n>}          — enumerate REMOTE
//                                                            routes + probe
//                                                            (reports build/mac)
//   {"task":"/can_act", "restart": <nodeId>}               — 0 = self,
//                                                            else SDO 0x2507=1
//   {"task":"/can_act", "setRemoteNodeId": <newId>,        — reassign a remote
//                       "target": <curId>, "expectMac": "AA:.."}  node's CAN id
//   {"task":"/can_act", "setRemoteNodeId": <newId>,        — reassign by MAC
//                       "byMac": "AA:BB:CC:DD:EE:FF"}          (master finds it)
// ============================================================================
cJSON* CANopenModule::act(cJSON* doc)
{
    cJSON* resp = cJSON_CreateObject();

    // ----- generic SDO read/write bridge (master) -----
    // Lets the host read/write any slave OD entry by index/sub. Used by the
    // UC2-REST axis feedback layer (design v2, WP9) to reach the AXIS block
    // (0x2040+), but works for any manufacturer OD entry.
    //   write: {"task":"/can_act","sdo":{"node":11,"index":8258,"sub":1,
    //           "op":"w","type":"i32","value":123}}
    //   read : {"task":"/can_act","sdo":{"node":11,"index":8256,"sub":1,
    //           "op":"r","type":"i32"}}  -> {"status":"ok","value":..,"node":11}
    // type ∈ {u8,u16,u32,i32}. index/sub/value are decimal ints on the wire.
    cJSON* sdoItem = cJSON_GetObjectItem(doc, "sdo");
    if (sdoItem && cJSON_IsObject(sdoItem)) {
        cJSON* jNode  = cJSON_GetObjectItem(sdoItem, "node");
        cJSON* jIndex = cJSON_GetObjectItem(sdoItem, "index");
        cJSON* jSub   = cJSON_GetObjectItem(sdoItem, "sub");
        cJSON* jOp    = cJSON_GetObjectItem(sdoItem, "op");
        cJSON* jType  = cJSON_GetObjectItem(sdoItem, "type");
        if (!cJSON_IsNumber(jNode) || !cJSON_IsNumber(jIndex) || !cJSON_IsNumber(jSub)) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "sdo needs node/index/sub");
            return resp;
        }
        uint8_t  node = (uint8_t)jNode->valueint;
        uint16_t idx  = (uint16_t)jIndex->valueint;
        uint8_t  sub  = (uint8_t)jSub->valueint;
        const char* type = (jType && jType->valuestring) ? jType->valuestring : "i32";
        bool isWrite = (jOp && jOp->valuestring && jOp->valuestring[0] == 'w');

        if (isWrite) {
            cJSON* jVal = cJSON_GetObjectItem(sdoItem, "value");
            long v = cJSON_IsNumber(jVal) ? (long)jVal->valuedouble : 0;
            bool ok;
            if      (!strcmp(type, "u8"))  ok = writeSDO_u8 (node, idx, sub, (uint8_t)v);
            else if (!strcmp(type, "u16")) ok = writeSDO_u16(node, idx, sub, (uint16_t)v);
            else if (!strcmp(type, "u32")) ok = writeSDO_u32(node, idx, sub, (uint32_t)v);
            else                           ok = writeSDO_i32(node, idx, sub, (int32_t)v);
            cJSON_AddStringToObject(resp, "status", ok ? "ok" : "error");
        } else {
            uint8_t buf[8] = {0};
            size_t rd = 0;
            bool ok = readSDO(node, idx, sub, buf, sizeof(buf), &rd);
            if (ok) {
                int32_t val = 0;
                memcpy(&val, buf, rd < 4 ? rd : 4);
                cJSON_AddNumberToObject(resp, "value", val);
            }
            cJSON_AddStringToObject(resp, "status", ok ? "ok" : "error");
        }
        cJSON_AddNumberToObject(resp, "node", node);
        cJSON_AddNumberToObject(resp, "index", idx);
        cJSON_AddNumberToObject(resp, "sub", sub);
        return resp;
    }

    // ----- scan: enumerate REMOTE routes and probe each unique nodeId -----
    cJSON* scanItem = cJSON_GetObjectItem(doc, "scan");
    if (scanItem && (cJSON_IsTrue(scanItem) ||
                     (cJSON_IsNumber(scanItem) && scanItem->valueint != 0)))
    {
        cJSON* arr = cJSON_CreateArray();
        // Dedupe nodeIds; remember the first route-type we saw per node.
        struct Entry { uint8_t nodeId; UC2::RouteEntry::Type type; };
        Entry seen[16]; uint8_t nSeen = 0;
        static const UC2::RouteEntry::Type types[] = {
            UC2::RouteEntry::MOTOR, UC2::RouteEntry::LASER,
            UC2::RouteEntry::LED,   UC2::RouteEntry::GALVO,
            UC2::RouteEntry::HOME,  UC2::RouteEntry::DIN,
        };
        for (auto t : types) {
            for (uint8_t lid = 0; lid < 4; lid++) {
                const auto* r = UC2::RoutingTable::find(t, lid);
                if (!r || r->where != UC2::RouteEntry::REMOTE) continue;
                bool dup = false;
                for (uint8_t i = 0; i < nSeen; i++) {
                    if (seen[i].nodeId == r->nodeId) { dup = true; break; }
                }
                if (dup) continue;
                if (nSeen < (uint8_t)(sizeof(seen)/sizeof(seen[0]))) {
                    seen[nSeen++] = { r->nodeId, t };
                }
            }
        }
        // Always add the GPIO slave if its node-id is configured, even when
        // no DIN route is in the routing table (e.g. on masters that don't
        // expose digital_in routing yet).
        if (pinConfig.MASTER_GPIO_SLAVE_NODE_ID > 0) {
            uint8_t gpioNid = (uint8_t)pinConfig.MASTER_GPIO_SLAVE_NODE_ID;
            bool dup = false;
            for (uint8_t i = 0; i < nSeen; i++) {
                if (seen[i].nodeId == gpioNid) { dup = true; break; }
            }
            if (!dup && nSeen < (uint8_t)(sizeof(seen)/sizeof(seen[0]))) {
                seen[nSeen++] = { gpioNid, UC2::RouteEntry::DIN };
            }
        }
        // The PTZ keyboard bridge (UC2_canopen_bridge_ptz) is a *producer*, not
        // a route target, so it never appears in the routing table. List it
        // explicitly, like the GPIO slave above, so the host can discover it.
        // Its 1 s producer heartbeat (OD 0x1017) keeps it reachable while
        // powered. Tagged with a synthetic type so the switch below prints "ptz".
        static constexpr int8_t kPtzTypeTag = 100;
        if (pinConfig.MASTER_PTZ_NODE_ID > 0) {
            uint8_t ptzNid = (uint8_t)pinConfig.MASTER_PTZ_NODE_ID;
            bool dup = false;
            for (uint8_t i = 0; i < nSeen; i++)
                if (seen[i].nodeId == ptzNid) { dup = true; break; }
            if (!dup && nSeen < (uint8_t)(sizeof(seen)/sizeof(seen[0]))) {
                seen[nSeen++] = { ptzNid, (UC2::RouteEntry::Type)kPtzTypeTag };
            }
        }
        for (uint8_t i = 0; i < nSeen; i++) {
            cJSON* dev = cJSON_CreateObject();
            const char* tStr = "?";
            switch ((int)seen[i].type) {
                case UC2::RouteEntry::MOTOR: tStr = "motor"; break;
                case UC2::RouteEntry::LASER: tStr = "laser"; break;
                case UC2::RouteEntry::LED:   tStr = "led";   break;
                case UC2::RouteEntry::GALVO: tStr = "galvo"; break;
                case UC2::RouteEntry::HOME:  tStr = "home";  break;
                case UC2::RouteEntry::DIN:   tStr = "gpio";  break;
                case kPtzTypeTag:            tStr = "ptz";   break;
                default: break;
            }
            bool reachable = isNodeReachable(seen[i].nodeId);
            cJSON_AddNumberToObject(dev, "canId",         seen[i].nodeId);
            cJSON_AddNumberToObject(dev, "deviceType",    (int)seen[i].type);
            cJSON_AddStringToObject(dev, "deviceTypeStr", tStr);
            cJSON_AddNumberToObject(dev, "status",        reachable ? 0 : 1);
            cJSON_AddStringToObject(dev, "statusStr",
                                    reachable ? "idle" : "unreachable");
            // For reachable nodes, SDO-read their identity strings so the scan
            // reports firmware build date, version and MAC. Each read is guarded
            // by reachability already; absent nodes are skipped (no stall).
            if (reachable) {
                char tmp[40];
                if (readNodeString(seen[i].nodeId, UC2_OD::BUILD_TIMESTAMP, tmp, sizeof(tmp)))
                    cJSON_AddStringToObject(dev, "build", tmp);
                if (readNodeString(seen[i].nodeId, UC2_OD::FIRMWARE_VERSION_STRING, tmp, sizeof(tmp)))
                    cJSON_AddStringToObject(dev, "fwVersion", tmp);
                if (readNodeString(seen[i].nodeId, UC2_OD::MAC_ADDRESS, tmp, sizeof(tmp)))
                    cJSON_AddStringToObject(dev, "mac", tmp);
            }
            cJSON_AddItemToArray(arr, dev);
        }
        // The scan only SDO-probes remotes; report the master's own identity
        // (read locally from its OD) at the top level so the host sees it too.
        cJSON* master = cJSON_CreateObject();
        cJSON_AddNumberToObject(master, "canId",     runtimeConfig.canNodeId);
        cJSON_AddStringToObject(master, "build",     OD_RAM.x2508_build_timestamp);
        cJSON_AddStringToObject(master, "fwVersion", OD_RAM.x2500_firmware_version_string);
        cJSON_AddStringToObject(master, "mac",       OD_RAM.x2509_mac_address);
        cJSON_AddItemToObject(resp, "master", master);

        // Optional discovery probe: SDO-read the MAC from a range of node-ids so
        // brand-new / unrouted nodes can be found for MAC-keyed provisioning.
        // {"scan":true,"probeRange":true[,"from":1,"to":127]}. Absent ids
        // fast-fail (canReachForSDO returns false for nodes that never
        // announced themselves), so this only spends real time on nodes that
        // are physically present on the bus.
        uint8_t nProbed = 0;
        cJSON* probeItem = cJSON_GetObjectItem(doc, "probeRange");
        if (probeItem && (cJSON_IsTrue(probeItem) ||
                          (cJSON_IsNumber(probeItem) && probeItem->valueint != 0))) {
            int from = 1, to = 127;
            cJSON* f = cJSON_GetObjectItem(doc, "from");
            cJSON* t = cJSON_GetObjectItem(doc, "to");
            if (f && cJSON_IsNumber(f)) from = f->valueint;
            if (t && cJSON_IsNumber(t)) to   = t->valueint;
            if (from < 1)   from = 1;
            if (to   > 127) to   = 127;
            for (int nid = from; nid <= to; nid++) {
                if (nid == runtimeConfig.canNodeId) continue;   // skip self
                bool already = false;                            // skip routed nodes
                for (uint8_t i = 0; i < nSeen; i++)
                    if (seen[i].nodeId == (uint8_t)nid) { already = true; break; }
                if (already) continue;
                char macStr[40];
                if (!readNodeString((uint8_t)nid, UC2_OD::MAC_ADDRESS, macStr, sizeof(macStr)))
                    continue;   // no response → not present
                cJSON* dev = cJSON_CreateObject();
                cJSON_AddNumberToObject(dev, "canId",         nid);
                cJSON_AddNumberToObject(dev, "deviceType",    -1);
                cJSON_AddStringToObject(dev, "deviceTypeStr", "unrouted");
                cJSON_AddNumberToObject(dev, "status",        0);
                cJSON_AddStringToObject(dev, "statusStr",     "idle");
                cJSON_AddStringToObject(dev, "mac",           macStr);
                char tmp[40];
                if (readNodeString((uint8_t)nid, UC2_OD::BUILD_TIMESTAMP, tmp, sizeof(tmp)))
                    cJSON_AddStringToObject(dev, "build", tmp);
                if (readNodeString((uint8_t)nid, UC2_OD::FIRMWARE_VERSION_STRING, tmp, sizeof(tmp)))
                    cJSON_AddStringToObject(dev, "fwVersion", tmp);
                cJSON_AddItemToArray(arr, dev);
                nProbed++;
            }
        }

        cJSON_AddItemToObject(resp, "scan", arr);
        cJSON* qidItem = cJSON_GetObjectItem(doc, "qid");
        cJSON_AddNumberToObject(resp, "qid",
            (qidItem && cJSON_IsNumber(qidItem)) ? qidItem->valueint : 0);
        cJSON_AddNumberToObject(resp, "count", nSeen + nProbed);
        return resp;
    }

    // ----- restart: 0 = master self-reboot, else SDO write 1 -> 0x2507 -----
    cJSON* restartItem = cJSON_GetObjectItem(doc, "restart");
    if (restartItem && cJSON_IsNumber(restartItem)) {
        int target = restartItem->valueint;
        if (target == 0) {
            cJSON_AddStringToObject(resp, "status", "rebooting");
            cJSON_AddNumberToObject(resp, "nodeId", 0);
            vTaskDelay(pdMS_TO_TICKS(300));
            esp_restart();
            return resp;  // unreachable
        }
        if (target < 1 || target > 127) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "nodeId out of range");
            return resp;
        }
        bool ok = writeSDO_u8((uint8_t)target,
                              0x2507, 0x00, 1, /*timeoutMs*/ 1000);
        cJSON_AddStringToObject(resp, "status", ok ? "ok" : "error");
        cJSON_AddNumberToObject(resp, "nodeId", target);
        if (!ok) cJSON_AddStringToObject(resp, "error", "SDO write failed");
        return resp;
    }

    // ----- setRemoteNodeId: reassign a remote node's CAN id by SDO (0x250A) ----
    // setRemoteNodeId is always the NEW id. Identify the node EITHER by:
    //   * its current id   — {"setRemoteNodeId":<new>,"target":<cur>,"expectMac":"AA:.."}
    //     (optional expectMac is verified first, so the id only ever binds to
    //      the intended device), OR
    //   * its MAC only     — {"setRemoteNodeId":<new>,"byMac":"AA:BB:CC:DD:EE:FF"}
    //     (the master probes the bus to find which id currently has that MAC —
    //      use this when you don't know / don't care about the current id).
    cJSON* setIdItem  = cJSON_GetObjectItem(doc, "setRemoteNodeId");
    cJSON* targetItem = cJSON_GetObjectItem(doc, "target");
    cJSON* byMacItem  = cJSON_GetObjectItem(doc, "byMac");
    bool   haveTarget = targetItem && cJSON_IsNumber(targetItem);
    bool   haveByMac  = byMacItem  && cJSON_IsString(byMacItem);
    if (setIdItem && cJSON_IsNumber(setIdItem) && (haveTarget || haveByMac)) {
        int newId = setIdItem->valueint;
        if (newId < 1 || newId > 127) {
            cJSON_AddStringToObject(resp, "status", "error");
            cJSON_AddStringToObject(resp, "error", "newId out of range (1..127)");
            return resp;
        }
        int target = -1;
        if (haveByMac) {
            // Probe the bus for the node currently advertising this MAC. Absent
            // ids fast-fail (canReachForSDO), so only present nodes cost time.
            const char* wantMac = byMacItem->valuestring;
            for (int nid = 1; nid <= 127 && target < 0; nid++) {
                if (nid == runtimeConfig.canNodeId) continue;   // skip self
                char macBuf[40];
                if (readNodeString((uint8_t)nid, UC2_OD::MAC_ADDRESS, macBuf, sizeof(macBuf)) &&
                    strcasecmp(macBuf, wantMac) == 0) {
                    target = nid;
                }
            }
            if (target < 0) {
                cJSON_AddStringToObject(resp, "status", "error");
                cJSON_AddStringToObject(resp, "error", "MAC not found on bus");
                cJSON_AddStringToObject(resp, "mac", wantMac);
                return resp;
            }
        } else {
            target = targetItem->valueint;
            if (target < 1 || target > 127) {
                cJSON_AddStringToObject(resp, "status", "error");
                cJSON_AddStringToObject(resp, "error", "target out of range (1..127)");
                return resp;
            }
            cJSON* expectMacItem = cJSON_GetObjectItem(doc, "expectMac");
            if (expectMacItem && cJSON_IsString(expectMacItem)) {
                char macBuf[40];
                if (!readNodeString((uint8_t)target, UC2_OD::MAC_ADDRESS,
                                    macBuf, sizeof(macBuf))) {
                    cJSON_AddStringToObject(resp, "status", "error");
                    cJSON_AddStringToObject(resp, "error", "could not read target MAC");
                    cJSON_AddNumberToObject(resp, "target", target);
                    return resp;
                }
                if (strcasecmp(macBuf, expectMacItem->valuestring) != 0) {
                    cJSON_AddStringToObject(resp, "status", "error");
                    cJSON_AddStringToObject(resp, "error", "MAC mismatch");
                    cJSON_AddStringToObject(resp, "mac", macBuf);
                    cJSON_AddNumberToObject(resp, "target", target);
                    return resp;
                }
            }
        }
        bool ok = writeSDO_u8((uint8_t)target, UC2_OD::COMMANDED_NODE_ID, 0x00,
                              (uint8_t)newId, /*timeoutMs*/ 1000);
        cJSON_AddStringToObject(resp, "status", ok ? "ok" : "error");
        cJSON_AddNumberToObject(resp, "target", target);
        cJSON_AddNumberToObject(resp, "newId",  newId);
        if (haveByMac) cJSON_AddStringToObject(resp, "mac", byMacItem->valuestring);
        if (!ok) cJSON_AddStringToObject(resp, "error", "SDO write failed");
        return resp;
    }

    bool changed = false;
    // Assign CAN OPEN ID: {"task":"/can_act","nodeId":11,"canMotorAxis":1}
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
        nvsconfig_save();
        cJSON_AddStringToObject(resp, "status", "saved");
        cJSON_AddNumberToObject(resp, "nodeId",       runtimeConfig.canNodeId);
        cJSON_AddNumberToObject(resp, "canMotorAxis", runtimeConfig.canMotorAxis);
        cJSON_AddBoolToObject(resp, "resetting", true);
        // Trigger a CANopen communication reset: CO_main_task will re-init the
        // node stack with the new node ID without requiring esp_restart().
        s_requestedReset = CO_RESET_COMM;
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
#ifdef MOTOR_CONTROLLER  // T1.3: only the motor dispatch path touches this
PendingAxisCmd s_axisCmds[4];
#endif

#ifdef HOME_MOTOR  // T1.3: only the homing dispatch path touches this
PendingHomingCmd s_homingCmds[4];
#endif

PendingLaserCmd s_laserCmds[4];

#ifdef MOTOR_CONTROLLER  // T1.3: hard limits belong to the motor path
PendingHardLimitCmd s_hardlimitCmds[4];
#endif

// ============================================================================
// Public API
// ============================================================================
void CANopenModule::setup()
{
    log_i("Starting CANopen stack (node-id=%d, TX=%d, RX=%d)...",
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

    // Node liveness is heartbeat-driven: every node produces a heartbeat at
    // x1017_producerHeartbeatTime (1000 ms) and the RX hook in CAN_ctrl_task
    // stamps s_nodeLastSeenMs from those 0x700 frames, so isNodeReachable()
    // becomes true ~1 s after a slave boots — no active SDO polling needed.
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
            log_i("NMT state changed -> %s (%d)", stateStr, curState);
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
        if (focusmotor_get_data()[localAxis] == nullptr) {
            ESP_LOGW(TAG_CO, "Motor dispatch: OD-ax%d -> local-ax%d is null; check REMOTE_MOTOR_AXIS_ID", ax, localAxis);
            continue;
        }
        MotorData* m = focusmotor_get_data()[localAxis];
#ifdef HOME_MOTOR
        // TODO: Think about this:  Don't let CAN-delivered motor commands (e.g. PS4 joystick PSx packets
        // streamed continuously from the master) preempt an active homing run.
        // Drop them silently so the homing state machine retains exclusive
        // control of the axis.
        HomeData** hdAll = home_get_data();
        if (hdAll && hdAll[localAxis] && hdAll[localAxis]->homeIsActive) {
            // NOTE: we intentionally do NOT drop motor commands during homing.
            // The host must ALWAYS be able to stop the motor (a CAN STOP has to
            // get through), so blocking motor_act here is not acceptable. Moves
            // that arrive mid-home are allowed through as well.
            log_d("Motor cmd for axis %d arrived during homing - passing through (STOP must always work)", localAxis);
        }
#endif
        if (s_axisCmds[ax].isStop) {
            m->isStop    = true;
            m->isforever = false;
            log_i("Dispatch motor STOP OD-ax%d -> local-ax%d", ax, localAxis);
            // Use stopStepper(); calling startStepper() with target=0,isforever=false
            // makes FAS attempt move(0) which never starts -> "motor never got going".
            focusmotor_stop_stepper(localAxis);
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
            /*
            log_i("Dispatch motor OD-ax%d -> local-ax%d: pos=%ld spd=%ld accel=%ld abs=%d",
                     ax, localAxis, (long)m->targetPosition, (long)m->speed,
                     (long)m->acceleration, m->absolutePosition);
            */
#ifdef AXIS_CONTROLLER
            // Closed-loop dispatch (design v2): when the axis is in a feedback
            // mode, hand the bounded move to AxisController (MONITOR divergence
            // check / CORRECT verify+retry / SERVO PID). Forever/jog moves stay
            // on the plain open-loop path.
            if (!m->isforever &&
                axis_get_feedback(localAxis).mode != MODE_OPEN_LOOP) {
                axis_move_to_with_mode(localAxis, m->targetPosition, m->speed,
                                               m->absolutePosition, 0xFF /*axis mode*/);
                continue;
            }
#endif
            focusmotor_start_stepper(localAxis, 0);
            static uint32_t dispatchN[4] = {0,0,0,0};
            if (s_axisCmds[ax].pending) {
                dispatchN[ax]++;
                ESP_LOGW(TAG_CO, "DISPATCH #%u ax=%d -> startStepper(%d)", dispatchN[ax], ax, localAxis);
            }
        }
    }

#endif

#ifdef HOME_MOTOR
    // Dispatch pending homing commands
    for (int ax = 0; ax < 4; ax++) {
        if (!s_homingCmds[ax].pending) continue;
        s_homingCmds[ax].pending = false;

        // Map OD axis to local physical axis (same as motor moves)
        int localAxis = (int)pinConfig.REMOTE_MOTOR_AXIS_ID;
        if (focusmotor_get_data()[localAxis] == nullptr) {
            ESP_LOGW(TAG_CO, "Homing dispatch: OD-ax%d -> local-ax%d is null; check REMOTE_MOTOR_AXIS_ID", ax, localAxis);
            continue;
        }

        int maxSpeed = s_homingCmds[ax].speed > 0 ? s_homingCmds[ax].speed * 2 : 1000;
        home_start(localAxis,
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

#ifdef MOTOR_CONTROLLER
    // Dispatch pending hard-limit config/clear commands.
    // Map OD axis -> local physical axis (same as motor moves at #2357 and homing
    // at #2419): the master addresses this slave by its routing sub-index, but the
    // physical stepper AND its endstop always live at REMOTE_MOTOR_AXIS_ID, which is
    // also the only axis checkHardLimits()/evaluateHardLimitForAxis() enforces.
    // Without this remap, a slave whose routing sub-index != REMOTE_MOTOR_AXIS_ID
    // (e.g. motor Y=2, motor Z=3) applied setHardLimit() to an idle OD axis while
    // the real axis kept enforcing the endstop — so disabling hardlimits silently
    // did nothing and the motor still stopped. Motor X (sub-index 1) worked only by
    // coincidence. clearHardLimitTriggered must hit the same physical axis so a
    // tripped/locked-out axis is actually released.
    {
        const int localAxis = (int)pinConfig.REMOTE_MOTOR_AXIS_ID;
        for (int ax = 0; ax < 4; ax++) {
            if (!s_hardlimitCmds[ax].pending) continue;
            s_hardlimitCmds[ax].pending = false;
            if (s_hardlimitCmds[ax].isCommand) {
                log_i("Hard-limit CLEAR OD-ax%d -> local-ax%d via CAN", ax, localAxis);
                focusmotor_clear_hard_limit_triggered(localAxis);
            }
            if (s_hardlimitCmds[ax].isConfig) {
                log_i("Hard-limit CONFIG OD-ax%d -> local-ax%d: enabled=%d polarity=%d via CAN",
                         ax, localAxis, (int)s_hardlimitCmds[ax].enabled, (int)s_hardlimitCmds[ax].polarity);
                focusmotor_set_hard_limit(localAxis,
                    s_hardlimitCmds[ax].enabled  != 0,
                    s_hardlimitCmds[ax].polarity != 0);
            }
        }
    }
#endif

#ifdef LASER_CONTROLLER
    // Dispatch pending laser value updates (only fires on value change, see syncRpdoToModules)
    for (int ch = 0; ch < 4; ch++) {
        if (!s_laserCmds[ch].pending) continue;
        s_laserCmds[ch].pending = false;
        log_i("Dispatch laser %d: PWM=%d", ch, (int)s_laserCmds[ch].value);
        laser_set_val(ch, (int)s_laserCmds[ch].value);
    }
#endif
}

// T4.2: C shims for the pure-C SerialProcessIn.c (canopen get/act bridge).
extern "C" cJSON *canopen_get(cJSON *doc)
{ return CANopenModule::get(doc); }

extern "C" cJSON *canopen_act(cJSON *doc)
{ return CANopenModule::act(doc); }

#endif // CAN_CONTROLLER_CANOPEN

/**
 * CanOpenStack.cpp
 *
 * UC2 CANopen-Lite stack implementation.
 * Manages the TWAI driver and a FreeRTOS RX task that dispatches frames
 * to PDOEngine, SDOHandler, NMTManager, and HeartbeatManager.
 *
 */
#include "CanOpenStack.h"
#include "PDOEngine.h"
#include "SDOHandler.h"
#include "NMTManager.h"
#include "HeartbeatManager.h"
#include "ObjectDictionary.h"
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/twai.h"
#include "esp_log.h"
#include <string.h>

#define TAG_COS "UC2_COS"

// RX task stack / priority
#define COS_RX_TASK_STACK  4096u
#define COS_RX_TASK_PRIO   5u

namespace CanOpenStack {

// ---------------------------------------------------------------------------
// Module-level state
// ---------------------------------------------------------------------------
static uint8_t          s_nodeId         = 1u;
static bool             s_busOk          = false;
static volatile uint32_t s_txCount       = 0u;
static volatile uint32_t s_rxCount       = 0u;
static volatile uint32_t s_errCount      = 0u;
static TaskHandle_t     s_rxTaskHandle   = nullptr;
static UC2_RPDOCallback s_rpdoCb         = nullptr;
static UC2_TPDOCallback s_tpdoCb         = nullptr;

// ---------------------------------------------------------------------------
// Helper: build TWAI timing config from bitrate enum
// ---------------------------------------------------------------------------
static twai_timing_config_t make_timing(UC2_CAN_Bitrate br)
{
    switch (br) {
    case UC2_CAN_250K: { twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS(); return t; }
    case UC2_CAN_1M:   { twai_timing_config_t t = TWAI_TIMING_CONFIG_1MBITS();   return t; }
    default:           { twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS(); return t; }
    }
}

// ---------------------------------------------------------------------------
// CAN RX task — receives frames and dispatches to subsystems
// ---------------------------------------------------------------------------
static void canRxTask(void* arg)
{
    ESP_LOGI(TAG_COS, "CAN RX task started (node-id=%d)", s_nodeId);

    while (true) {
        twai_message_t frame;
        esp_err_t err = twai_receive(&frame, pdMS_TO_TICKS(100));
        if (err != ESP_OK) {
            // Check for bus-off / error alerts
            uint32_t alerts = 0;
            twai_read_alerts(&alerts, 0);
            if (alerts & (TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_ERROR)) {
                s_busOk = false;
                s_errCount++;
                ESP_LOGW(TAG_COS, "CAN bus error — recovering");
                twai_initiate_recovery();
                vTaskDelay(pdMS_TO_TICKS(200));
                twai_stop();
                twai_start();
                twai_reconfigure_alerts(TWAI_ALERT_ALL, nullptr);
                s_busOk = true;
            }
            continue;
        }

        s_rxCount++;

        // --- NMT frame ---
        if (PDOEngine::isNMT(frame) && frame.data_length_code >= 2) {
            NMTManager::onNMTCommand(frame.data[0], frame.data[1], s_nodeId);
            continue;
        }

        // --- Heartbeat frame ---
        if (PDOEngine::isHeartbeat(frame)) {
            HeartbeatManager::processFrame(frame);
            continue;
        }

        // --- SDO response (master: SDO client waiting in SDOHandler) ---
        // SDO request (slave: our SDO server handles it)
        if (PDOEngine::isSDO(frame)) {
            // Slave-side SDO server
            if (frame.identifier == UC2_COBID_SDO_REQ_BASE + s_nodeId) {
                SDOHandler::sdoServerProcessFrame(frame);
            }
            // Master-side: SDO responses go back to SDOHandler::waitSDOResponse()
            // which polls twai_receive() directly — no dispatch needed here
            // EXCEPT we re-queue it so the polling thread sees it.
            // (For now, the SDO client calls twai_receive() in a blocking loop
            //  and will pick up the frame on the next receive.)
            continue;
        }

        // --- UC2 PDO frames ---
        if (PDOEngine::isUC2PDO(frame)) {
            // ----- Slave: dispatch incoming RPDOs to app callback -----
            {
                UC2_RPDO1_MotorPos rp1 = {};
                if (PDOEngine::decodeMotorPosPDO(frame, s_nodeId, &rp1)) {
                    if (s_rpdoCb) s_rpdoCb(&rp1, nullptr, nullptr);
                    continue;
                }
            }
            {
                UC2_RPDO2_Control rp2 = {};
                if (PDOEngine::decodeControlPDO(frame, s_nodeId, &rp2)) {
                    if (s_rpdoCb) s_rpdoCb(nullptr, &rp2, nullptr);
                    continue;
                }
            }
            {
                UC2_RPDO3_LEDExtra rp3 = {};
                if (PDOEngine::decodeLEDExtraPDO(frame, s_nodeId, &rp3)) {
                    if (s_rpdoCb) s_rpdoCb(nullptr, nullptr, &rp3);
                    continue;
                }
            }

            // ----- Master: dispatch incoming TPDOs to app callback -----
            {
                uint8_t srcNode = 0;
                UC2_TPDO1_MotorStatus tp1 = {};
                if (PDOEngine::decodeMotorStatusPDO(frame, &srcNode, &tp1)) {
                    // Update NMT table with node being alive
                    NMTManager::onHeartbeat(srcNode, UC2_NMT_STATE_OPERATIONAL);
                    if (s_tpdoCb) s_tpdoCb(srcNode, &tp1, nullptr, nullptr);
                    continue;
                }
            }
            {
                uint8_t srcNode = 0;
                UC2_TPDO2_NodeStatus tp2 = {};
                if (PDOEngine::decodeNodeStatusPDO(frame, &srcNode, &tp2)) {
                    NMTManager::onNodeStatusPDO(srcNode, tp2.capabilities);
                    NMTManager::onHeartbeat(srcNode, tp2.nmt_state);
                    if (s_tpdoCb) s_tpdoCb(srcNode, nullptr, &tp2, nullptr);
                    continue;
                }
            }
            {
                uint8_t srcNode = 0;
                UC2_TPDO3_SensorData tp3 = {};
                if (PDOEngine::decodeSensorDataPDO(frame, &srcNode, &tp3)) {
                    if (s_tpdoCb) s_tpdoCb(srcNode, nullptr, nullptr, &tp3);
                    continue;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
bool init(gpio_num_t txGpio, gpio_num_t rxGpio,
          uint8_t ownNodeId,
          UC2_CAN_Bitrate bitrate,
          uint16_t heartbeatPeriodMs)
{
    s_nodeId = ownNodeId;

    // TWAI driver configuration
    twai_general_config_t g_config = {
        .mode           = TWAI_MODE_NORMAL,
        .tx_io          = txGpio,
        .rx_io          = rxGpio,
        .clkout_io      = TWAI_IO_UNUSED,
        .bus_off_io     = TWAI_IO_UNUSED,
        .tx_queue_len   = 32,
        .rx_queue_len   = 32,
        .alerts_enabled = TWAI_ALERT_ALL,
        .clkout_divider = 0,
        .intr_flags     = ESP_INTR_FLAG_LEVEL1
    };
    twai_timing_config_t t_config = make_timing(bitrate);
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_COS, "TWAI install failed: %d", err);
        return false;
    }
    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG_COS, "TWAI start failed: %d", err);
        twai_driver_uninstall();
        return false;
    }

    // Flush stale frames
    vTaskDelay(pdMS_TO_TICKS(50));
    {
        twai_message_t dummy;
        while (twai_receive(&dummy, 0) == ESP_OK) {}
    }

    s_busOk   = true;
    s_txCount = s_rxCount = s_errCount = 0u;

    // Initialize sub-systems
    PDOEngine::init();
    NMTManager::init();
    HeartbeatManager::init(ownNodeId, heartbeatPeriodMs);
    SDOHandler::sdoServerInit(ownNodeId, nullptr, nullptr, nullptr);  // no-op until callbacks set

    // Send NMT boot-up frame (HB with state=0x00 signals boot-up to master)
    {
        twai_message_t boot = {};
        boot.identifier       = UC2_COBID_HEARTBEAT_BASE + ownNodeId;
        boot.extd             = 0;
        boot.rtr              = 0;
        boot.data_length_code = 1;
        boot.data[0]          = UC2_NMT_STATE_INITIALIZING;
        twai_transmit(&boot, pdMS_TO_TICKS(20));
    }

    // Start RX task
    xTaskCreatePinnedToCore(canRxTask, "UC2_CAN_RX",
                            COS_RX_TASK_STACK, nullptr,
                            COS_RX_TASK_PRIO, &s_rxTaskHandle,
                            1 /* core 1 */);

    ESP_LOGI(TAG_COS, "CANopen stack initialized (node-id=%d, HB=%dms)",
             ownNodeId, heartbeatPeriodMs);
    return true;
}

void deinit()
{
    if (s_rxTaskHandle) {
        vTaskDelete(s_rxTaskHandle);
        s_rxTaskHandle = nullptr;
    }
    twai_stop();
    twai_driver_uninstall();
    s_busOk = false;
    ESP_LOGI(TAG_COS, "CANopen stack stopped");
}

void loop()
{
    // Tick heartbeat producer (sends HB frame when period elapses)
    HeartbeatManager::tick();
    // Check heartbeat consumer timeouts
    NMTManager::update();
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void setSdoODCallbacks(SDOHandler::OD_ReadCb readCb,
                       SDOHandler::OD_WriteCb writeCb, void* ctx)
{
    SDOHandler::sdoServerInit(s_nodeId, readCb, writeCb, ctx);
}

void setRPDOCallback(UC2_RPDOCallback cb) { s_rpdoCb = cb; }
void setTPDOCallback(UC2_TPDOCallback cb) { s_tpdoCb = cb; }

void setNodeStatusCallback(NMTManager::NodeStatusCallback cb)
{
    NMTManager::setNodeStatusCallback(cb);
}

// ---------------------------------------------------------------------------
// Master-side transmit helpers
// ---------------------------------------------------------------------------

static bool transmit(const twai_message_t& f)
{
    esp_err_t err = twai_transmit(&f, pdMS_TO_TICKS(20));
    if (err == ESP_OK) { s_txCount++; return true; }
    s_errCount++;
    return false;
}

bool sendMotorMove(uint8_t nodeId, uint8_t axis,
                   int32_t targetPos, int32_t speed, uint8_t cmd, int16_t qid)
{
    // RPDO1: position + speed
    bool ok = transmit(PDOEngine::encodeMotorPosPDO(nodeId, targetPos, speed));
    // RPDO2: axis + command (laser/LED fields unused — axis != 0xFF/0xFE)
    // Include qid in LED-extra RPDO3 so slave can report it in TPDO1
    ok &= transmit(PDOEngine::encodeControlPDO(nodeId,
                   axis, cmd, 0, 0, 0, 0, 0));
    ok &= transmit(PDOEngine::encodeLEDExtraPDO(nodeId, 0, 0, qid));
    return ok;
}

bool sendMotorStop(uint8_t nodeId, uint8_t axis, int16_t qid)
{
    bool ok = transmit(PDOEngine::encodeControlPDO(nodeId,
                       axis, UC2_MOTOR_CMD_STOP, 0, 0, 0, 0, 0));
    ok &= transmit(PDOEngine::encodeLEDExtraPDO(nodeId, 0, 0, qid));
    return ok;
}

bool sendLaserSet(uint8_t nodeId, uint8_t channel, uint16_t pwm, int16_t qid)
{
    // axis=0xFF signals laser-only; no motor trigger
    return transmit(PDOEngine::encodeControlPDO(nodeId,
                    0xFF, 0, channel, pwm, 0, 0, 0));
}

bool sendLEDSet(uint8_t nodeId, uint8_t mode,
                uint8_t r, uint8_t g, uint8_t b, uint16_t ledIndex, int16_t qid)
{
    // axis=0xFE signals LED-only
    bool ok = transmit(PDOEngine::encodeControlPDO(nodeId,
                       0xFE, 0, 0, 0, mode, r, g));
    ok &= transmit(PDOEngine::encodeLEDExtraPDO(nodeId, b, ledIndex, qid));
    return ok;
}

// ---------------------------------------------------------------------------
// Slave-side transmit helpers
// ---------------------------------------------------------------------------

bool publishMotorStatus(uint8_t axis, int32_t actualPos,
                        bool isRunning, int16_t qid)
{
    return transmit(PDOEngine::encodeMotorStatusPDO(s_nodeId,
                    axis, actualPos, isRunning, qid));
}

bool publishNodeStatus(uint8_t caps, uint8_t errReg,
                       int16_t tempX10, int16_t encoderPos)
{
    uint8_t nmtState = (uint8_t)NMTManager::getOwnState();
    return transmit(PDOEngine::encodeNodeStatusPDO(s_nodeId,
                    caps, errReg, tempX10, encoderPos, nmtState));
}

bool publishSensorData(int32_t encoderPos32, uint16_t adcRaw,
                       uint8_t dinState, uint8_t doutState)
{
    return transmit(PDOEngine::encodeSensorDataPDO(s_nodeId,
                    encoderPos32, adcRaw, dinState, doutState));
}

// ---------------------------------------------------------------------------
// NMT helpers
// ---------------------------------------------------------------------------

void startAll()         { NMTManager::sendStart(0); }
void stopAll()          { NMTManager::sendStop(0); }
void resetNode(uint8_t n) { NMTManager::sendReset(n); }

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

uint8_t  ownNodeId()  { return s_nodeId; }
bool     isBusOk()    { return s_busOk; }
uint32_t txCount()    { return s_txCount; }
uint32_t rxCount()    { return s_rxCount; }
uint32_t errorCount() { return s_errCount; }

} // namespace CanOpenStack

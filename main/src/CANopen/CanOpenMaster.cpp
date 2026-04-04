/**
 * CanOpenMaster.cpp
 *
 * Master-side CANopen-Lite initialization and loop.
 *
 * Wires CanOpenStack, NMTManager, and DeviceRouter together.
 * The master discovers slaves via NMT boot-up, queries capabilities,
 * and populates the DeviceRouter route table automatically.
 */
#include "CanOpenMaster.h"
#include "CanOpenStack.h"
#include "NMTManager.h"
#include "ObjectDictionary.h"
#include "../DeviceRouter.h"
#include "esp_log.h"
#include <PinConfig.h>

#ifndef UC2_CAN_TX_PIN
#  define UC2_CAN_TX_PIN  17
#endif
#ifndef UC2_CAN_RX_PIN
#  define UC2_CAN_RX_PIN  18
#endif
#ifndef UC2_HB_PERIOD_MS
#  define UC2_HB_PERIOD_MS 1000u
#endif

// Master always uses node-ID 0x01
#ifndef UC2_MASTER_NODE_ID
#  define UC2_MASTER_NODE_ID 0x01
#endif

#define TAG_CM "UC2_CO_MASTER"

#include "SDOHandler.h"

namespace CanOpenMaster {

// -----------------------------------------------------------------------
// TPDO callback — receives status frames from slaves
// -----------------------------------------------------------------------
static void onTPDO(uint8_t nodeId,
                   const UC2_TPDO1_MotorStatus* tp1,
                   const UC2_TPDO2_NodeStatus*  tp2,
                   const UC2_TPDO3_SensorData*  /*tp3*/)
{
    if (tp1) {
        ESP_LOGD(TAG_CM, "TPDO1 node=0x%02X axis=%d pos=%ld running=%d",
                 nodeId, tp1->axis, (long)tp1->actual_pos, tp1->is_running);
    }
    if (tp2) {
        // First time we see a TPDO2 from a node, treat it as an online event
        ESP_LOGD(TAG_CM, "TPDO2 node=0x%02X caps=0x%02X err=0x%02X",
                 nodeId, tp2->capabilities, tp2->error_reg);
    }
}

// -----------------------------------------------------------------------
// NMT node-status callback — detect boot-up and offline events
// -----------------------------------------------------------------------
static void onNodeStatus(uint8_t nodeId, UC2_NMT_State state, bool online)
{
    ESP_LOGI(TAG_CM, "NMT: node 0x%02X state=%d online=%d",
             nodeId, (int)state, (int)online);

    if (online && state == UC2_NMT_State::INITIALIZING) {
        // Query capabilities and motor/laser count from slave via SDO
        uint8_t caps = 0;
        uint8_t motorCount = 1;
        uint8_t laserCount = 0;
        if (!SDOHandler::nodeGetCaps(nodeId, &caps)) {
            caps = UC2_CAP_MOTOR; // Fallback assumption
        }
        if (caps & UC2_CAP_MOTOR) {
            uint8_t mc = 0;
            if (SDOHandler::readU8(nodeId, UC2_OD_MOTOR_COUNT, 0, &mc) && mc > 0) {
                motorCount = mc;
            }
        }
        if (caps & UC2_CAP_LASER) {
            uint8_t lc = 0;
            if (SDOHandler::readU8(nodeId, UC2_OD_LASER_COUNT, 0, &lc) && lc > 0) {
                laserCount = lc;
            }
        }
        DeviceRouter::onSlaveOnline(nodeId, caps, motorCount, laserCount);

        // Send NMT start so the slave transitions to OPERATIONAL
        NMTManager::sendStart(nodeId);
    }

    if (!online) {
        DeviceRouter::onSlaveOffline(nodeId);
    }
}

// -----------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------
void setup()
{
    ESP_LOGI(TAG_CM, "CANopen master setup: TX=%d RX=%d HB=%dms",
             UC2_CAN_TX_PIN, UC2_CAN_RX_PIN, UC2_HB_PERIOD_MS);

    // Build local route table entries — count only axes with valid (>=0) step pins
    uint8_t localMotors = 0;
    uint8_t localLasers = 0;
    bool    localLED    = false;
#ifdef MOTOR_CONTROLLER
    if (pinConfig.MOTOR_A_STEP >= 0) localMotors++;
    if (pinConfig.MOTOR_X_STEP >= 0) localMotors++;
    if (pinConfig.MOTOR_Y_STEP >= 0) localMotors++;
    if (pinConfig.MOTOR_Z_STEP >= 0) localMotors++;
#endif
#ifdef LASER_CONTROLLER
    // Count configured laser channels (GPIO pin >= 0 means active)
    if (pinConfig.LASER_1 >= 0) localLasers++;
    if (pinConfig.LASER_2 >= 0) localLasers++;
    if (pinConfig.LASER_3 >= 0) localLasers++;;
#endif
#ifdef LED_CONTROLLER
    localLED = (pinConfig.LED_PIN >= 0);
#endif
    DeviceRouter::init(localMotors, localLasers, localLED);

    // Register callbacks before starting the stack
    CanOpenStack::setTPDOCallback(onTPDO);
    CanOpenStack::setNodeStatusCallback(onNodeStatus);

    bool ok = CanOpenStack::init(
        (gpio_num_t)UC2_CAN_TX_PIN,
        (gpio_num_t)UC2_CAN_RX_PIN,
        (uint8_t)UC2_MASTER_NODE_ID,
        UC2_CAN_500K,
        (uint16_t)UC2_HB_PERIOD_MS
    );

    if (!ok) {
        ESP_LOGE(TAG_CM, "CanOpenStack init FAILED — check wiring");
        return;
    }

    // Start all slaves that may already be on the bus
    CanOpenStack::startAll();

    ESP_LOGI(TAG_CM, "CANopen master running.");
}

// -----------------------------------------------------------------------
// loop
// -----------------------------------------------------------------------
void loop()
{
    CanOpenStack::loop();
}

} // namespace CanOpenMaster

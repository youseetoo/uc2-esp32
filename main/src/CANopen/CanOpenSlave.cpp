/**
 * CanOpenSlave.cpp
 *
 * Slave-side CANopen-Lite initialization and loop.
 *
 * Extracted from the former slave/main.cpp so it can be compiled as part
 * of the unified firmware and guarded by UC2_CANOPEN_SLAVE.
 */
#include "CanOpenSlave.h"
#include "CanOpenStack.h"
#include "NMTManager.h"
#include "SlaveController.h"
#include "ObjectDictionary.h"
#include "esp_log.h"

// Build-time configuration with sensible defaults
#ifndef UC2_NODE_ID
#  define UC2_NODE_ID        0x10
#endif
#ifndef UC2_CAN_TX_PIN
#  define UC2_CAN_TX_PIN     3
#endif
#ifndef UC2_CAN_RX_PIN
#  define UC2_CAN_RX_PIN     2
#endif
#ifndef UC2_HB_PERIOD_MS
#  define UC2_HB_PERIOD_MS   1000u
#endif
#ifndef SLAVE_NUM_MOTORS
#  define SLAVE_NUM_MOTORS   1
#endif
#ifndef SLAVE_NUM_LASERS
#  define SLAVE_NUM_LASERS   0
#endif
#ifndef SLAVE_HAS_LED
#  define SLAVE_HAS_LED      0
#endif

// Capability bitmask derived from build flags
static constexpr uint8_t OWN_CAPS =
#if SLAVE_NUM_MOTORS > 0
    UC2_CAP_MOTOR |
#endif
#if SLAVE_NUM_LASERS > 0
    UC2_CAP_LASER |
#endif
#if SLAVE_HAS_LED
    UC2_CAP_LED |
#endif
    0u;

#define TAG_CS "UC2_CO_SLAVE"

namespace CanOpenSlave {

// -----------------------------------------------------------------------
// setup
// -----------------------------------------------------------------------
void setup()
{
    ESP_LOGI(TAG_CS, "CANopen slave setup: nodeId=0x%02X TX=%d RX=%d HB=%dms caps=0x%02X",
             UC2_NODE_ID, UC2_CAN_TX_PIN, UC2_CAN_RX_PIN, UC2_HB_PERIOD_MS, OWN_CAPS);

    // Initialize SlaveController state
    SlaveController::init(
        OWN_CAPS,
        (uint8_t)UC2_NODE_ID,
        (uint8_t)SLAVE_NUM_MOTORS,
        (uint8_t)SLAVE_NUM_LASERS,
        (bool)SLAVE_HAS_LED
    );

    // Wire up callbacks BEFORE starting the CAN stack
    CanOpenStack::setSdoODCallbacks(SlaveController::sdoRead,
                                    SlaveController::sdoWrite,
                                    nullptr);
    CanOpenStack::setRPDOCallback(SlaveController::onRPDO);

    NMTManager::setNodeStatusCallback(
        [](uint8_t nodeId, UC2_NMT_State newState, bool online) {
            ESP_LOGI(TAG_CS, "NMT: node 0x%02X state=%d online=%d",
                     nodeId, (int)newState, (int)online);
        }
    );

    // Start CAN stack
    bool ok = CanOpenStack::init(
        (gpio_num_t)UC2_CAN_TX_PIN,
        (gpio_num_t)UC2_CAN_RX_PIN,
        (uint8_t)UC2_NODE_ID,
        UC2_CAN_500K,
        (uint16_t)UC2_HB_PERIOD_MS
    );

    if (!ok) {
        ESP_LOGE(TAG_CS, "CanOpenStack init FAILED — check wiring");
        return;
    }

    ESP_LOGI(TAG_CS, "CANopen slave running. Waiting for NMT START from master.");
}

// -----------------------------------------------------------------------
// loop
// -----------------------------------------------------------------------
void loop()
{
    CanOpenStack::loop();

    // Only run device-level logic when the master has put us in OPERATIONAL
    if (NMTManager::getOwnState() == UC2_NMT_State::OPERATIONAL) {
        SlaveController::loop();
    }
}

} // namespace CanOpenSlave

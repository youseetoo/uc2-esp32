/**
 * slave/main.cpp
 *
 * UC2 CAN Satellite Firmware — entry point.
 *
 * This firmware runs on a Seeed XIAO ESP32-S3 (or any ESP32 variant)
 * connected to the UC2 CAN bus as a CANopen-Lite slave node.
 *
 * Build configuration (set via platformio.ini build_flags):
 *   -DUC2_NODE_ID=0x10          CAN node-ID (default 0x10 = first satellite)
 *   -DUC2_CAN_TX_PIN=3          TWAI TX GPIO
 *   -DUC2_CAN_RX_PIN=2          TWAI RX GPIO
 *   -DUC2_HB_PERIOD_MS=1000     Heartbeat period in ms
 *   -DMOTOR_CONTROLLER          Include FocusMotor support
 *   -DLASER_CONTROLLER          Include LaserController support
 *   -DLED_CONTROLLER            Include LedController support
 *   -DSLAVE_NUM_MOTORS=1        Number of motor axes
 *   -DSLAVE_NUM_LASERS=2        Number of laser channels
 *   -DSLAVE_HAS_LED=1           LED array present
 *
 * Boot sequence:
 *   1. Initialize NVS and any persistent config
 *   2. Initialize device controllers (motor, laser, LED)
 *   3. Initialize CanOpenStack → sends CANopen boot-up frame automatically
 *   4. Register RPDO + SDO callbacks from SlaveController
 *   5. Wait for NMT START from master → enter OPERATIONAL
 *   6. Loop: process devices, publish TPDOs on events
 */
#include <Arduino.h>
#include "nvs_flash.h"
#include "esp_log.h"

#include "CANopen/CanOpenStack.h"
#include "CANopen/NMTManager.h"
#include "SlaveController.h"

// ---------------------------------------------------------------------------
// Build-time configuration with sensible defaults
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Capability bitmask derived from build flags
// ---------------------------------------------------------------------------
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

#define TAG_SLAVE "UC2_SLAVE"

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
static void initDeviceControllers();

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup()
{
    // Basic Arduino serial (debug only — not used for JSON protocol on slaves)
    Serial.begin(115200);
    delay(200);
    ESP_LOGI(TAG_SLAVE, "UC2 CAN Slave starting. nodeId=0x%02X", UC2_NODE_ID);

    // Initialize NVS (required by some ESP-IDF components)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG_SLAVE, "NVS partition invalid, erasing...");
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Initialize local device controllers before connecting to the bus
    initDeviceControllers();

    // Initialize SlaveController (holds the runtime OD state)
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

    // NMT state-change callback (optional — useful for debug)
    NMTManager::setNodeStatusCallback(
        [](uint8_t nodeId, UC2_NMT_State newState, bool online) {
            ESP_LOGI(TAG_SLAVE, "NMT: node 0x%02X state=%d online=%d",
                     nodeId, (int)newState, (int)online);
        }
    );

    // Start CAN stack:
    //   - installs TWAI driver
    //   - starts background RX task
    //   - sends CANopen boot-up frame (node starts in PRE_OPERATIONAL)
    bool ok = CanOpenStack::init(
        (gpio_num_t)UC2_CAN_TX_PIN,
        (gpio_num_t)UC2_CAN_RX_PIN,
        (uint8_t)UC2_NODE_ID,
        UC2_CAN_500K,
        (uint16_t)UC2_HB_PERIOD_MS
    );

    if (!ok) {
        ESP_LOGE(TAG_SLAVE, "CanOpenStack init failed — check TX/RX pins and bus wiring");
        // Continue without CAN; device isn't useful but doesn't hard-fault
        return;
    }

    ESP_LOGI(TAG_SLAVE, "CAN stack running. Waiting for NMT START from master.");
    ESP_LOGI(TAG_SLAVE, "  TX=%d RX=%d HB=%dms caps=0x%02X",
             UC2_CAN_TX_PIN, UC2_CAN_RX_PIN, UC2_HB_PERIOD_MS, OWN_CAPS);
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop()
{
    // Tick the CAN stack (sends heartbeat, checks slave timeouts on master)
    CanOpenStack::loop();

    // Only run device-level logic when the master has put us in OPERATIONAL
    if (NMTManager::getOwnState() == UC2_NMT_State::OPERATIONAL) {
        SlaveController::loop();
    }

    // Yield to FreeRTOS — the CAN RX runs in a background task so this delay
    // just prevents the idle watchdog from triggering
    delay(1);
}

// ---------------------------------------------------------------------------
// Device controller initialization
// ---------------------------------------------------------------------------
static void initDeviceControllers()
{
#ifdef MOTOR_CONTROLLER
    // FocusMotor reads its pin config from RuntimeConfig / PinConfig.
    // On the slave, a minimal RuntimeConfig must be provided via build flags
    // or hard-coded defaults in PinConfig.h.
    FocusMotor::setup();
    ESP_LOGI(TAG_SLAVE, "FocusMotor initialized (%d axes)", SLAVE_NUM_MOTORS);
#endif

#ifdef LASER_CONTROLLER
    LaserController::setup();
    ESP_LOGI(TAG_SLAVE, "LaserController initialized (%d channels)", SLAVE_NUM_LASERS);
#endif

#ifdef LED_CONTROLLER
    LedController::setup();
    ESP_LOGI(TAG_SLAVE, "LedController initialized");
#endif
}

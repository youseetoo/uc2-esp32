#include <PinConfig.h>
#include "MotorBackendFactory.h"
#include "IMotorBackend.h"
#include "backends/LocalMotorBackend.h"
#include "../config/RuntimeConfig.h"
#include "esp_log.h"

#ifdef CAN_CONTROLLER_CANOPEN
#include "backends/CANopenMotorBackend.h"
#include "backends/HybridMotorBackend.h"
#endif

static const char* TAG = "MotorBEFactory";

// Helper: returns true if at least one motor axis has a valid step pin
static bool hasLocalMotorPins()
{
    return (pinConfig.MOTOR_A_STEP >= 0 ||
            pinConfig.MOTOR_X_STEP >= 0 ||
            pinConfig.MOTOR_Y_STEP >= 0 ||
            pinConfig.MOTOR_Z_STEP >= 0);
}

// Helper: returns the step pin for the given axis
static int getMotorStepPin(int axis)
{
    switch (axis) {
        case 0: return pinConfig.MOTOR_A_STEP;
        case 1: return pinConfig.MOTOR_X_STEP;
        case 2: return pinConfig.MOTOR_Y_STEP;
        case 3: return pinConfig.MOTOR_Z_STEP;
        default: return -1;
    }
}

IMotorBackend* createMotorBackend()
{
#ifdef  
    if (runtimeConfig.canRole == NodeRole::CAN_MASTER && !hasLocalMotorPins()) {
        // Pure master: forward everything via CANopen SDO
        uint8_t nodeIds[4] = {
            pinConfig.CAN_ID_MOT_A,
            pinConfig.CAN_ID_MOT_X,
            pinConfig.CAN_ID_MOT_Y,
            pinConfig.CAN_ID_MOT_Z
        };
        ESP_LOGI(TAG, "Creating CANopenMotorBackend (pure master)");
        return new CANopenMotorBackend(nodeIds);
    }

    if (runtimeConfig.canRole == NodeRole::CAN_MASTER && hasLocalMotorPins()) {
        // Hybrid master: some local, some remote
        ESP_LOGI(TAG, "Creating HybridMotorBackend (master + local pins)");
        auto* hybrid = new HybridMotorBackend();

        uint8_t nodeIds[4] = {
            pinConfig.CAN_ID_MOT_A,
            pinConfig.CAN_ID_MOT_X,
            pinConfig.CAN_ID_MOT_Y,
            pinConfig.CAN_ID_MOT_Z
        };

        for (int ax = 0; ax < 4; ax++) {
            if (getMotorStepPin(ax) >= 0) {
                hybrid->setAxisBackend(ax, new LocalMotorBackend());
            } else {
                // Single-axis CANopen backend
                uint8_t singleNode[4] = {0, 0, 0, 0};
                singleNode[ax] = nodeIds[ax];
                hybrid->setAxisBackend(ax, new CANopenMotorBackend(singleNode));
            }
        }
        return hybrid;
    }

    if (runtimeConfig.canRole == NodeRole::CAN_SLAVE) {
        // Slave: always local, CANopenModule sync calls our backend
        ESP_LOGI(TAG, "Creating LocalMotorBackend (CAN slave)");
        return new LocalMotorBackend();
    }
#endif

    // STANDALONE (no CAN) or legacy CAN (non-CANopen)
    ESP_LOGI(TAG, "Creating LocalMotorBackend (standalone)");
    return new LocalMotorBackend();
}

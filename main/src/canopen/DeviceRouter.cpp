// DeviceRouter.cpp — Serial-to-CANopen bridge implementation.
// Translates JSON commands into SDO writes/reads to CAN slave nodes.
// Only compiled on master builds (CAN_CONTROLLER_CANOPEN + CAN_SEND_COMMANDS).
#include "DeviceRouter.h"

#if defined(CAN_CONTROLLER_CANOPEN) && defined(CAN_SEND_COMMANDS)

#include "CANopenModule.h"
#include "../wifi/Endpoints.h"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "UC2_DR";

// ============================================================================
// Node-ID mapping (matches existing CAN address scheme)
// ============================================================================
uint8_t DeviceRouter::stepperIdToNodeId(int stepperid) {
    switch (stepperid) {
        case 0: return 14;  // A axis (was CAN addr 272)
        case 1: return 11;  // X axis (was CAN addr 273)
        case 2: return 12;  // Y axis (was CAN addr 274)
        case 3: return 13;  // Z axis (was CAN addr 275)
        default: return 10; // fallback
    }
}

// ============================================================================
// Route a task to the correct handler
// ============================================================================
cJSON* DeviceRouter::routeCommand(const char* task, cJSON* doc) {
    if (task == nullptr || doc == nullptr) return nullptr;

    if (strcmp(task, motor_act_endpoint) == 0)
        return handleMotorAct(doc);
    else if (strcmp(task, motor_get_endpoint) == 0)
        return handleMotorGet(doc);
    else if (strcmp(task, laser_act_endpoint) == 0)
        return handleLaserAct(doc);
    else if (strcmp(task, home_act_endpoint) == 0)
        return handleHomeAct(doc);

    return nullptr; // not a CAN-routed command
}

// ============================================================================
// Motor act — write target position via SDO to slave OD 0x6200:01
// (Using trainer OD indices for now; will change to 0x2000 with full UC2 OD)
// ============================================================================
cJSON* DeviceRouter::handleMotorAct(cJSON* doc) {
    cJSON* motor = cJSON_GetObjectItem(doc, "motor");
    if (!motor) return nullptr;
    cJSON* steppers = cJSON_GetObjectItem(motor, "steppers");
    if (!steppers || !cJSON_IsArray(steppers)) return nullptr;

    cJSON* respSteppers = cJSON_CreateArray();
    int arraySize = cJSON_GetArraySize(steppers);

    for (int i = 0; i < arraySize; i++) {
        cJSON* stepper = cJSON_GetArrayItem(steppers, i);
        cJSON* idItem = cJSON_GetObjectItem(stepper, "stepperid");
        if (!idItem) continue;

        int stepperid = idItem->valueint;
        uint8_t nodeId = stepperIdToNodeId(stepperid);

        // Write target position via SDO to OD 0x6200:01
        cJSON* pos = cJSON_GetObjectItem(stepper, "position");
        if (pos) {
            int32_t posVal = pos->valueint;
            uint8_t data[4];
            memcpy(data, &posVal, 4);  // little-endian, matches ESP32
            bool ok = CANopenModule::writeSDO(nodeId, 0x6200, 0x01, data, 4);
            if (!ok) {
                ESP_LOGW(TAG, "SDO write failed: node=0x%02X pos=%ld", nodeId, (long)posVal);
            }
        }

        // Write speed if provided
        cJSON* speed = cJSON_GetObjectItem(stepper, "speed");
        if (speed) {
            int32_t speedVal = speed->valueint;
            uint8_t data[4];
            memcpy(data, &speedVal, 4);
            CANopenModule::writeSDO(nodeId, 0x6200, 0x02, data, 4);
        }

        // Write isEnable if provided
        cJSON* isEnable = cJSON_GetObjectItem(stepper, "isEnable");
        if (isEnable) {
            uint8_t en = cJSON_IsTrue(isEnable) ? 1 : 0;
            CANopenModule::writeSDO(nodeId, 0x6200, 0x03, &en, 1);
        }

        // Build per-stepper response
        cJSON* respStepper = cJSON_CreateObject();
        cJSON_AddNumberToObject(respStepper, "stepperid", stepperid);

        // Read back current position to confirm the node is alive
        uint8_t buf[4];
        size_t readSize = 0;
        if (CANopenModule::readSDO(nodeId, 0x6001, 0x00, buf, sizeof(buf), &readSize)) {
            int32_t currentPos;
            memcpy(&currentPos, buf, 4);
            cJSON_AddNumberToObject(respStepper, "position", currentPos);
        }
        cJSON_AddNumberToObject(respStepper, "isDone", 0);
        cJSON_AddItemToArray(respSteppers, respStepper);
    }

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "steppers", respSteppers);
    return resp;
}

// ============================================================================
// Motor get — read current position from slave OD
// ============================================================================
cJSON* DeviceRouter::handleMotorGet(cJSON* doc) {
    cJSON* motor = cJSON_GetObjectItem(doc, "motor");
    if (!motor) return nullptr;
    cJSON* steppers = cJSON_GetObjectItem(motor, "steppers");
    if (!steppers || !cJSON_IsArray(steppers)) return nullptr;

    cJSON* respSteppers = cJSON_CreateArray();
    int arraySize = cJSON_GetArraySize(steppers);

    for (int i = 0; i < arraySize; i++) {
        cJSON* stepper = cJSON_GetArrayItem(steppers, i);
        cJSON* idItem = cJSON_GetObjectItem(stepper, "stepperid");
        if (!idItem) continue;

        int stepperid = idItem->valueint;
        uint8_t nodeId = stepperIdToNodeId(stepperid);

        cJSON* respStepper = cJSON_CreateObject();
        cJSON_AddNumberToObject(respStepper, "stepperid", stepperid);

        // Read current position from OD 0x6001:00
        uint8_t buf[4];
        size_t readSize = 0;
        if (CANopenModule::readSDO(nodeId, 0x6001, 0x00, buf, sizeof(buf), &readSize)) {
            int32_t currentPos;
            memcpy(&currentPos, buf, 4);
            cJSON_AddNumberToObject(respStepper, "position", currentPos);
            cJSON_AddNumberToObject(respStepper, "isDone", 1);
        } else {
            cJSON_AddNumberToObject(respStepper, "position", 0);
            cJSON_AddNumberToObject(respStepper, "isDone", -1); // error
            ESP_LOGW(TAG, "SDO read failed: node=0x%02X", nodeId);
        }
        cJSON_AddItemToArray(respSteppers, respStepper);
    }

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "steppers", respSteppers);
    return resp;
}

// ============================================================================
// Laser act — write laser value via SDO
// ============================================================================
cJSON* DeviceRouter::handleLaserAct(cJSON* doc) {
    cJSON* laser = cJSON_GetObjectItem(doc, "laser");
    if (!laser) return nullptr;

    // Support single laser or array
    cJSON* laserid_item = cJSON_GetObjectItem(laser, "laserid");
    cJSON* val_item = cJSON_GetObjectItem(laser, "val");
    if (!laserid_item || !val_item) return nullptr;

    int laserid = laserid_item->valueint;
    int32_t val = val_item->valueint;

    // Map laser to node — for now, laser 0→node11, laser 1→node12, laser 2→node13
    uint8_t nodeId = 11 + (uint8_t)laserid;

    uint8_t data[4];
    memcpy(data, &val, 4);
    bool ok = CANopenModule::writeSDO(nodeId, 0x6200, 0x04, data, 4);

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "return", ok ? 1 : 0);
    return resp;
}

// ============================================================================
// Home act — trigger homing on slave
// ============================================================================
cJSON* DeviceRouter::handleHomeAct(cJSON* doc) {
    cJSON* home = cJSON_GetObjectItem(doc, "home");
    if (!home) return nullptr;

    cJSON* stepperid_item = cJSON_GetObjectItem(home, "stepperid");
    if (!stepperid_item) return nullptr;

    int stepperid = stepperid_item->valueint;
    uint8_t nodeId = stepperIdToNodeId(stepperid);

    // Write homing trigger to slave OD 0x6200:05
    uint8_t trigger = 1;
    bool ok = CANopenModule::writeSDO(nodeId, 0x6200, 0x05, &trigger, 1);

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "return", ok ? 1 : 0);
    return resp;
}

// ============================================================================
// State get — read operational state from a slave node
// ============================================================================
cJSON* DeviceRouter::handleStateGet(uint8_t nodeId) {
    cJSON* resp = cJSON_CreateObject();

    // Read heartbeat counter from OD 0x6001:00 as a liveness check
    uint8_t buf[4];
    size_t readSize = 0;
    if (CANopenModule::readSDO(nodeId, 0x6001, 0x00, buf, sizeof(buf), &readSize)) {
        uint32_t counter;
        memcpy(&counter, buf, 4);
        cJSON_AddNumberToObject(resp, "counter", counter);
        cJSON_AddNumberToObject(resp, "online", 1);
    } else {
        cJSON_AddNumberToObject(resp, "online", 0);
    }

    return resp;
}

#endif // CAN_CONTROLLER_CANOPEN && CAN_SEND_COMMANDS

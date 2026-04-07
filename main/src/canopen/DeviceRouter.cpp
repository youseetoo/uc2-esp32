// DeviceRouter.cpp — Serial-to-CANopen bridge implementation.
// Translates JSON commands into SDO writes/reads to CAN slave nodes.
// Only compiled on master builds (CAN_CONTROLLER_CANOPEN + CAN_SEND_COMMANDS).
#include "DeviceRouter.h"

#if defined(CAN_CONTROLLER_CANOPEN) && defined(CAN_SEND_COMMANDS)

#include "CANopenModule.h"
#include "../wifi/Endpoints.h"
#include "esp_log.h"
#include <cstring>
#include <algorithm>  // std::min

static const char* TAG = "UC2_DR";

// ============================================================================
// Node-ID mapping — must match the slave's runtimeConfig.canNodeId (default=10)
// Mirrors old CAN address scheme: A=10, X=11, Y=12, Z=13
// ============================================================================
uint8_t DeviceRouter::stepperIdToNodeId(int stepperid) {
    switch (stepperid) {
        case 0: return 10;  // A axis (CAN_ID_MOT_A)
        case 1: return 11;  // X axis (CAN_ID_MOT_X)
        case 2: return 12;  // Y axis (CAN_ID_MOT_Y)
        case 3: return 13;  // Z axis (CAN_ID_MOT_Z)
        default: return 10;
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
// Motor act — send motor command to slave using trainer OD entries:
//
//   0x6401:01  = target position high word (bits 31-16)  uint16
//   0x6401:02  = target position low  word (bits 15-0)   uint16
//   0x6401:03  = speed (clamped to uint16, 0-65535)      uint16
//   0x6401:04  = control flags (bit0=isAbs, bit1=isStop) uint16
//   0x6200:01  = trigger byte   (bit0=1 to start move)   uint8
//
// Slave's syncRpdoToModules() detects the trigger, reassembles position,
// calls FocusMotor::startStepper(), then clears bit0.
//
// Note: This uses the MWE trainer OD (uint16 split encoding). The full UC2 OD
// will use native int32 entries at 0x2000+ — remove split encoding at that point.
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

        // --- Write position (int32 split into two uint16 words) ---
        cJSON* pos = cJSON_GetObjectItem(stepper, "position");
        int32_t posVal = pos ? pos->valueint : 0;
        uint16_t posHi = (uint16_t)((uint32_t)posVal >> 16);
        uint16_t posLo = (uint16_t)((uint32_t)posVal & 0xFFFF);
        CANopenModule::writeSDO(nodeId, 0x6401, 0x01, (uint8_t*)&posHi, 2);
        CANopenModule::writeSDO(nodeId, 0x6401, 0x02, (uint8_t*)&posLo, 2);

        // --- Write speed (clamped to uint16) ---
        cJSON* speed = cJSON_GetObjectItem(stepper, "speed");
        uint16_t speedVal = speed ? (uint16_t)std::min(std::abs(speed->valueint), 65535) : 1000;
        CANopenModule::writeSDO(nodeId, 0x6401, 0x03, (uint8_t*)&speedVal, 2);

        // --- Write control flags ---
        cJSON* isAbsItem = cJSON_GetObjectItem(stepper, "isabs");
        cJSON* isStopItem = cJSON_GetObjectItem(stepper, "isStop");
        uint16_t ctrl = 0;
        if (isAbsItem && isAbsItem->valueint) ctrl |= 0x01;  // bit0 = absolute
        if (isStopItem && cJSON_IsTrue(isStopItem)) ctrl |= 0x02; // bit1 = stop
        CANopenModule::writeSDO(nodeId, 0x6401, 0x04, (uint8_t*)&ctrl, 2);

        // --- Trigger the move ---
        uint8_t trigger = 0x01;
        bool ok = CANopenModule::writeSDO(nodeId, 0x6200, 0x01, &trigger, 1);
        if (!ok) {
            ESP_LOGW(TAG, "Trigger SDO failed: node=0x%02X", nodeId);
        }
        log_i("Motor cmd → node 0x%02X: pos=%ld speed=%u isAbs=%d",
              nodeId, (long)posVal, speedVal, (ctrl & 0x01) ? 1 : 0);

        // Build response
        cJSON* respStepper = cJSON_CreateObject();
        cJSON_AddNumberToObject(respStepper, "stepperid", stepperid);
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

        // Read position: two uint16 words at 0x6401:01 and 0x6401:02
        uint8_t buf[4] = {};
        size_t readSize = 0;
        uint16_t hiWord = 0, loWord = 0;
        bool ok = true;
        ok &= CANopenModule::readSDO(nodeId, 0x6401, 0x01, (uint8_t*)&hiWord, 2, &readSize);
        ok &= CANopenModule::readSDO(nodeId, 0x6401, 0x02, (uint8_t*)&loWord, 2, &readSize);
        int32_t currentPos = (int32_t)(((uint32_t)hiWord << 16) | (uint32_t)loWord);

        // Read isRunning from 0x6000:01
        uint8_t isRunning = 0;
        CANopenModule::readSDO(nodeId, 0x6000, 0x01, &isRunning, 1, &readSize);

        if (ok) {
            cJSON_AddNumberToObject(respStepper, "position", currentPos);
            cJSON_AddNumberToObject(respStepper, "isRunning", isRunning);
            cJSON_AddNumberToObject(respStepper, "isDone", isRunning ? 0 : 1);
        } else {
            ESP_LOGW(TAG, "SDO read failed: node=0x%02X", nodeId);
            cJSON_AddNumberToObject(respStepper, "isDone", -1);
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

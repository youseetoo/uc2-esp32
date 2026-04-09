// DeviceRouter.cpp — Serial-to-CANopen bridge implementation.
// Translates JSON commands into SDO writes/reads to CAN slave nodes.
// Only compiled on master builds (CAN_CONTROLLER_CANOPEN + CAN_SEND_COMMANDS).
#include "DeviceRouter.h"

#if defined(CAN_CONTROLLER_CANOPEN) && defined(CAN_SEND_COMMANDS)
#include <PinConfig.h>
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
        case 0: return pinConfig.CAN_ID_MOT_A;
        case 1: return pinConfig.CAN_ID_MOT_X;
        case 2: return pinConfig.CAN_ID_MOT_Y;
        case 3: return pinConfig.CAN_ID_MOT_Z;
        default: return pinConfig.CAN_ID_MOT_X;
    }
}

// Each node currently has one motor driver; axis index on node is always 0.
// Extend here if a future node hosts multiple motor drivers.
uint8_t DeviceRouter::stepperIdToAxisOnNode(int /*stepperid*/) {
    return 0;
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
// Motor act — send motor command to slave using native UC2 OD entries:
//
//   0x2000:sub  = target position (int32)
//   0x2002:sub  = speed (uint32)
//   0x2006:sub  = acceleration (uint32, optional)
//   0x2007:sub  = isAbsolute flag (uint8)
//   0x2003:0x00 = command word (uint8 bitmask: bit[axis]=start, bit[axis+4]=stop)
//
// sub = axis + 1 (CANopenNode uses 1-based sub-indexes for arrays).
// Slave's syncRpdoToModules() detects the command word, starts FocusMotor,
// then clears the processed bits.
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
        uint8_t axis   = stepperIdToAxisOnNode(stepperid);
        uint8_t sub    = axis + 1;

        // Write target position (int32)
        cJSON* posItem = cJSON_GetObjectItem(stepper, "position");
        int32_t posVal = posItem ? posItem->valueint : 0;
        CANopenModule::writeSDO(nodeId, 0x2000, sub, (uint8_t*)&posVal, 4);

        // Write speed (uint32)
        cJSON* speedItem = cJSON_GetObjectItem(stepper, "speed");
        uint32_t speedVal = speedItem ? (uint32_t)std::abs(speedItem->valueint) : 1000;
        CANopenModule::writeSDO(nodeId, 0x2002, sub, (uint8_t*)&speedVal, 4);

        // Write acceleration (uint32), if provided
        cJSON* accelItem = cJSON_GetObjectItem(stepper, "acceleration");
        if (accelItem) {
            uint32_t accelVal = (uint32_t)std::abs(accelItem->valueint);
            CANopenModule::writeSDO(nodeId, 0x2006, sub, (uint8_t*)&accelVal, 4);
        }

        // Write isAbsolute flag (uint8)
        cJSON* isAbsItem = cJSON_GetObjectItem(stepper, "isabs");
        uint8_t isAbs = (isAbsItem && isAbsItem->valueint) ? 1 : 0;
        CANopenModule::writeSDO(nodeId, 0x2007, sub, &isAbs, 1);

        // Trigger: set stop bit (axis+4) or start bit (axis) in command word
        cJSON* isStopItem = cJSON_GetObjectItem(stepper, "isStop");
        bool isStop = isStopItem && cJSON_IsTrue(isStopItem);
        uint8_t cmdWord = isStop ? (uint8_t)(1u << (axis + 4)) : (uint8_t)(1u << axis);
        bool ok = CANopenModule::writeSDO(nodeId, 0x2003, 0x00, &cmdWord, 1);
        if (!ok) {
            ESP_LOGW(TAG, "Motor trigger SDO failed: node=0x%02X", nodeId);
        }
        log_i("Motor cmd → node 0x%02X axis %u: pos=%ld speed=%u isAbs=%d isStop=%d",
              nodeId, axis, (long)posVal, speedVal, isAbs, isStop ? 1 : 0);

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
// Motor get — read position and running state from slave UC2 OD:
//
//   0x2001:sub  = actual position (int32)
//   0x2004:sub  = status word (uint8, bit0=isRunning)
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
        uint8_t axis   = stepperIdToAxisOnNode(stepperid);
        uint8_t sub    = axis + 1;

        cJSON* respStepper = cJSON_CreateObject();
        cJSON_AddNumberToObject(respStepper, "stepperid", stepperid);

        // Read actual position (int32)
        int32_t currentPos = 0;
        size_t readSize = 0;
        bool ok = CANopenModule::readSDO(nodeId, 0x2001, sub, (uint8_t*)&currentPos, 4, &readSize);

        // Read status word (uint8, bit0 = isRunning)
        uint8_t statusWord = 0;
        CANopenModule::readSDO(nodeId, 0x2004, sub, &statusWord, 1, &readSize);

        if (ok) {
            cJSON_AddNumberToObject(respStepper, "position",  currentPos);
            cJSON_AddNumberToObject(respStepper, "isRunning", (statusWord & 0x01) ? 1 : 0);
            cJSON_AddNumberToObject(respStepper, "isDone",    (statusWord & 0x01) ? 0 : 1);
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
// Laser act — write laser PWM value via UC2 OD:
//
//   0x2100:sub  = laser PWM value (uint16)
//
// sub = 1 (channel 0 on the laser node, 1-based sub-index).
// Node mapping: laserid 0 → node 11, 1 → node 12, 2 → node 13.
// ============================================================================
cJSON* DeviceRouter::handleLaserAct(cJSON* doc) {
    cJSON* laser = cJSON_GetObjectItem(doc, "laser");
    if (!laser) return nullptr;

    cJSON* laserid_item = cJSON_GetObjectItem(laser, "laserid");
    cJSON* val_item     = cJSON_GetObjectItem(laser, "val");
    if (!laserid_item || !val_item) return nullptr;

    int laserid = laserid_item->valueint;
    int rawVal  = val_item->valueint;
    uint16_t pwmVal = (uint16_t)std::min(std::max(rawVal, 0), 65535);

    // Map laser id to node (laserid 0→node11, 1→node12, 2→node13)
    uint8_t nodeId = 11 + (uint8_t)laserid;
    uint8_t sub    = 0x01;  // channel 0 on that node

    bool ok = CANopenModule::writeSDO(nodeId, 0x2100, sub, (uint8_t*)&pwmVal, 2);

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "return", ok ? 1 : 0);
    return resp;
}

// ============================================================================
// Home act — trigger homing on slave via UC2 OD:
//
//   0x2011:sub  = homing speed (uint32)
//   0x2012:sub  = homing direction (int8,  -1 or +1)
//   0x2013:sub  = homing timeout ms (uint32)
//   0x2014:sub  = endstop release steps (int32)
//   0x2015:sub  = endstop polarity (uint8, 0=NC 1=NO)
//   0x2010:sub  = homing command trigger (uint8, write 1 to arm)
// ============================================================================
cJSON* DeviceRouter::handleHomeAct(cJSON* doc) {
    cJSON* home = cJSON_GetObjectItem(doc, "home");
    if (!home) return nullptr;

    cJSON* stepperid_item = cJSON_GetObjectItem(home, "stepperid");
    if (!stepperid_item) return nullptr;

    int stepperid = stepperid_item->valueint;
    uint8_t nodeId = stepperIdToNodeId(stepperid);
    uint8_t axis   = stepperIdToAxisOnNode(stepperid);
    uint8_t sub    = axis + 1;

    // Extract homing parameters with sensible defaults
    cJSON* speedItem    = cJSON_GetObjectItem(home, "speed");
    cJSON* dirItem      = cJSON_GetObjectItem(home, "direction");
    cJSON* timeoutItem  = cJSON_GetObjectItem(home, "timeout");
    cJSON* releaseItem  = cJSON_GetObjectItem(home, "endstopRelease");
    cJSON* polarityItem = cJSON_GetObjectItem(home, "endstopPolarity");

    uint32_t speed    = speedItem    ? (uint32_t)std::abs(speedItem->valueint)   : 1000u;
    int8_t   dir      = dirItem      ? (int8_t)dirItem->valueint                 : -1;
    uint32_t timeout  = timeoutItem  ? (uint32_t)timeoutItem->valueint           : 10000u;
    int32_t  release  = releaseItem  ? (int32_t)releaseItem->valueint            : 0;
    uint8_t  polarity = polarityItem ? (uint8_t)polarityItem->valueint           : 0u;

    CANopenModule::writeSDO(nodeId, 0x2011, sub, (uint8_t*)&speed,    4);
    CANopenModule::writeSDO(nodeId, 0x2012, sub, (uint8_t*)&dir,      1);
    CANopenModule::writeSDO(nodeId, 0x2013, sub, (uint8_t*)&timeout,  4);
    CANopenModule::writeSDO(nodeId, 0x2014, sub, (uint8_t*)&release,  4);
    CANopenModule::writeSDO(nodeId, 0x2015, sub, (uint8_t*)&polarity, 1);

    uint8_t trigger = 1;
    bool ok = CANopenModule::writeSDO(nodeId, 0x2010, sub, &trigger, 1);

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "return", ok ? 1 : 0);
    return resp;
}

// ============================================================================
// State get — read uptime from slave UC2 OD (x2503 = uptime_seconds, VAR)
// ============================================================================
cJSON* DeviceRouter::handleStateGet(uint8_t nodeId) {
    cJSON* resp = cJSON_CreateObject();

    uint32_t uptime = 0;
    size_t readSize = 0;
    if (CANopenModule::readSDO(nodeId, 0x2503, 0x00, (uint8_t*)&uptime, 4, &readSize)) {
        cJSON_AddNumberToObject(resp, "uptime",  uptime);
        cJSON_AddNumberToObject(resp, "online",  1);
    } else {
        cJSON_AddNumberToObject(resp, "online",  0);
    }

    return resp;
}

#endif // CAN_CONTROLLER_CANOPEN && CAN_SEND_COMMANDS

// DeviceRouter.cpp — Routing-table-driven command dispatcher.
// Consults RoutingTable to decide LOCAL vs REMOTE for each logical device.
// Active on ALL builds. REMOTE dispatch requires CAN_CONTROLLER_CANOPEN.
#include "DeviceRouter.h"
#include <PinConfig.h>

#include "RoutingTable.h"
#include "../wifi/Endpoints.h"
#include "esp_log.h"
#include <cstring>
#include <algorithm>

#ifdef CAN_CONTROLLER_CANOPEN
#include "CANopenModule.h"
#include "UC2_OD_Indices.h"
#endif

#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
#include "../motor/MotorJsonParser.h"
#include "../qid/QidRegistry.h"
#endif
#ifdef LASER_CONTROLLER
#include "../laser/LaserController.h"
#endif
#ifdef LED_CONTROLLER
#include "../led/LedController.h"
#endif
#ifdef HOME_MOTOR
#include "../home/HomeMotor.h"
#endif
#ifdef GALVO_CONTROLLER
#include "../scanner/GalvoController.h"
#endif
#ifdef TMC_CONTROLLER
#include "../tmc/TMCController.h"
#endif


static const char* TAG = "UC2_DR";


// ============================================================================
// Route a task to the correct handler via the routing table
// ============================================================================
cJSON* DeviceRouter::routeCommand(const char* task, cJSON* doc) {
    if (task == nullptr || doc == nullptr) return nullptr;
    log_i("DeviceRouter received task: %s", task);
#ifdef MOTOR_CONTROLLER
    if (strcmp(task, motor_act_endpoint) == 0)
        return handleMotorAct(doc);
    if (strcmp(task, motor_get_endpoint) == 0)
        return handleMotorGet(doc);
#endif
#ifdef LASER_CONTROLLER
    if (strcmp(task, laser_act_endpoint) == 0)
        return handleLaserAct(doc);
    if (strcmp(task, laser_get_endpoint) == 0)
        return handleLaserGet(doc);
#endif
#ifdef HOME_MOTOR
    if (strcmp(task, home_act_endpoint) == 0)
        return handleHomeAct(doc);
    if (strcmp(task, home_get_endpoint) == 0)
        return handleHomeGet(doc);
#endif
#ifdef LED_CONTROLLER
    if (strcmp(task, ledarr_act_endpoint) == 0)
        return handleLedAct(doc);
    if (strcmp(task, ledarr_get_endpoint) == 0)
        return handleLedGet(doc);
#endif
#ifdef GALVO_CONTROLLER
    if (strcmp(task, galvo_act_endpoint) == 0)
        return handleGalvoAct(doc);
    if (strcmp(task, galvo_get_endpoint) == 0)
        return handleGalvoGet(doc);
#endif
#ifdef TMC_CONTROLLER
    if (strcmp(task, tmc_act_endpoint) == 0)
        return handleTmcAct(doc);
    if (strcmp(task, tmc_get_endpoint) == 0)
        return handleTmcGet(doc);
#endif

    return nullptr; // not a routed command
}

// ============================================================================
// Motor act — routing-table dispatch
// ============================================================================
cJSON* DeviceRouter::handleMotorAct(cJSON* doc) {
    // TODO: We should rather use the MotorJsonParser here instead of duplicating the parsing logic, but that would require refactoring MotorJsonParser to separate parsing from acting, and also to allow passing through the "isStop" flag which is currently only handled in SerialProcess. For now, we duplicate the parsing logic here for simplicity.?
#ifdef MOTOR_CONTROLLER
    log_i("Handling motor_act via DeviceRouter");
    cJSON* motor = cJSON_GetObjectItem(doc, "motor");
    if (!motor) return nullptr;
    cJSON* steppers = cJSON_GetObjectItem(motor, "steppers");
    if (!steppers || !cJSON_IsArray(steppers)) return nullptr;

    cJSON* respSteppers = cJSON_CreateArray();
    int n = cJSON_GetArraySize(steppers);

    // Extract QID so TPDO-driven completion (syncRpdoToModules_master) can report it
#ifdef CAN_CONTROLLER_CANOPEN
    int motorQid = 0;
    {
        cJSON* qidItem = cJSON_GetObjectItem(doc, "qid");
        if (qidItem && cJSON_IsNumber(qidItem)) motorQid = qidItem->valueint;
    }
    int remoteStepperCount = 0;
#endif

    for (int i = 0; i < n; i++) {
        cJSON* s = cJSON_GetArrayItem(steppers, i);
        cJSON* idItem = cJSON_GetObjectItem(s, "stepperid");
        if (!idItem) continue;

        int stepperid = idItem->valueint;
        const auto* route = UC2::RoutingTable::find(
            UC2::RouteEntry::MOTOR, (uint8_t)stepperid);

        if (!route || route->where == UC2::RouteEntry::OFF) {
            ESP_LOGW(TAG, "motor %d has no route", stepperid);
            continue;
        }

        cJSON* posItem     = cJSON_GetObjectItem(s, "position");
        cJSON* speedItem   = cJSON_GetObjectItem(s, "speed");
        cJSON* accelItem   = cJSON_GetObjectItem(s, "acceleration");
        cJSON* isAbsItem   = cJSON_GetObjectItem(s, "isabs");
        cJSON* isStopItem  = cJSON_GetObjectItem(s, "isStop");
        cJSON* foreverItem = cJSON_GetObjectItem(s, "isforever");

        int32_t  pos   = posItem   ? posItem->valueint   : 0;
        uint32_t speed = speedItem ? (uint32_t)std::abs(speedItem->valueint) : 1000;
        uint32_t accel = accelItem ? (uint32_t)std::abs(accelItem->valueint) : 0;
        // cJSON_IsTrue only handles cJSON bool, not numeric 1/0 — handle both
        bool     isAbs     = isAbsItem && (cJSON_IsTrue(isAbsItem) || (cJSON_IsNumber(isAbsItem) && isAbsItem->valueint != 0));
        bool     isStop    = isStopItem && (cJSON_IsTrue(isStopItem) || (cJSON_IsNumber(isStopItem) && isStopItem->valueint != 0));
        bool     isForever = foreverItem && (cJSON_IsTrue(foreverItem) || (cJSON_IsNumber(foreverItem) && foreverItem->valueint != 0));

        if (route->where == UC2::RouteEntry::LOCAL) {
            // Direct call into FocusMotor — no CAN involved
            log_i("Routing motor_act to LOCAL stepper %d: pos=%ld speed=%u accel=%u abs=%d stop=%d",
                     stepperid, (long)pos, speed, accel, isAbs, isStop);
            if (isStop) {
                if (FocusMotor::getData()[stepperid]) {
                    FocusMotor::getData()[stepperid]->isStop = true;
                    FocusMotor::getData()[stepperid]->isforever = false;
                    FocusMotor::startStepper(stepperid, 0);
                }
            } else {
                MotorData* d = FocusMotor::getData()[stepperid];
                if (d) {
                    d->targetPosition   = pos;
                    d->speed            = speed;
                    d->absolutePosition = isAbs;
                    d->isforever        = isForever;
                    d->isStop           = false;
                    d->stopped          = false;
                    if (accel > 0) d->acceleration = accel;
                    else if (d->acceleration <= 0) d->acceleration = 40000;
                    FocusMotor::startStepper(stepperid, 0); // TODO: Shouldn't we use stopstepper instead?
                }
            }
        } else { // REMOTE
#ifdef CAN_CONTROLLER_CANOPEN
            uint8_t nodeId = route->nodeId;
            uint8_t sub    = route->subAxis + 1;

            log_i("Routing motor_act to REMOTE node 0x%02X axis %u: pos=%ld speed=%u accel=%u abs=%d stop=%d",
                     nodeId, route->subAxis, (long)pos, speed, accel, isAbs, isStop);
            CANopenModule::writeSDO_i32(nodeId, UC2_OD::MOTOR_TARGET_POSITION, sub, pos);
            CANopenModule::writeSDO_u32(nodeId, UC2_OD::MOTOR_SPEED, sub, speed);
            if (accel > 0)
                CANopenModule::writeSDO_u32(nodeId, UC2_OD::MOTOR_ACCELERATION, sub, accel);
            CANopenModule::writeSDO_u8(nodeId, UC2_OD::MOTOR_IS_ABSOLUTE, sub, isAbs ? 1 : 0);
            CANopenModule::writeSDO_u8(nodeId, UC2_OD::MOTOR_IS_FOREVER, sub, isForever ? 1 : 0);

            uint8_t cmdWord = isStop
                ? (uint8_t)(1u << (route->subAxis + 4))
                : (uint8_t)(1u << route->subAxis);
            bool ok = CANopenModule::writeSDO_u8(nodeId, UC2_OD::MOTOR_COMMAND_WORD, 0x00, cmdWord);
            if (!ok) log_w("Motor SDO failed: node 0x%02X", nodeId);

            log_i("Motor cmd -> node 0x%02X axis %u: pos=%ld speed=%u abs=%d stop=%d",
                     nodeId, route->subAxis, (long)pos, speed, isAbs, isStop);

            // Store qid in FocusMotor cache so the TPDO completion callback can report it
            MotorData* rd = FocusMotor::getData()[stepperid];
            if (rd) rd->qid = motorQid;
            remoteStepperCount++;
#else
            log_w("REMOTE routing requires CANopen — motor %d ignored", stepperid);
#endif
        }

        cJSON* rs = cJSON_CreateObject();
        cJSON_AddNumberToObject(rs, "stepperid", stepperid);
        cJSON_AddNumberToObject(rs, "isDone", 0);
        cJSON_AddItemToArray(respSteppers, rs);
    }

    // Register the QID for all dispatched remote steppers so QidRegistry can
    // emit {"qid":N,"state":"done"} when their TPDO signals completion.
#ifdef CAN_CONTROLLER_CANOPEN
    if (motorQid > 0 && remoteStepperCount > 0)
        QidRegistry::registerQid(motorQid, (uint8_t)remoteStepperCount);
#endif

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "steppers", respSteppers);
    return resp;
#else
    return nullptr;
#endif
}

// ============================================================================
// Motor get — routing-table dispatch
// ============================================================================
cJSON* DeviceRouter::handleMotorGet(cJSON* doc) {
#ifdef MOTOR_CONTROLLER
    cJSON* motor = cJSON_GetObjectItem(doc, "motor");
    if (!motor) return nullptr;
    cJSON* steppers = cJSON_GetObjectItem(motor, "steppers");
    if (!steppers || !cJSON_IsArray(steppers)) return nullptr;

    cJSON* respSteppers = cJSON_CreateArray();
    int n = cJSON_GetArraySize(steppers);

    for (int i = 0; i < n; i++) {
        cJSON* s = cJSON_GetArrayItem(steppers, i);
        cJSON* idItem = cJSON_GetObjectItem(s, "stepperid");
        if (!idItem) continue;

        int stepperid = idItem->valueint;
        const auto* route = UC2::RoutingTable::find(
            UC2::RouteEntry::MOTOR, (uint8_t)stepperid);

        cJSON* rs = cJSON_CreateObject();
        cJSON_AddNumberToObject(rs, "stepperid", stepperid);

        if (!route || route->where == UC2::RouteEntry::OFF) {
            cJSON_AddNumberToObject(rs, "isDone", -1);
            cJSON_AddItemToArray(respSteppers, rs);
            continue;
        }

        if (route->where == UC2::RouteEntry::LOCAL) {
            MotorData* d = FocusMotor::getData()[stepperid];
            if (d) {
                cJSON_AddNumberToObject(rs, "position", d->currentPosition);
                cJSON_AddNumberToObject(rs, "isRunning", FocusMotor::isRunning(stepperid) ? 1 : 0);
                cJSON_AddNumberToObject(rs, "isDone", FocusMotor::isRunning(stepperid) ? 0 : 1);
            } else {
                cJSON_AddNumberToObject(rs, "isDone", -1);
            }
        } else { // REMOTE — read from TPDO cache (zero-copy, no SDO)
#ifdef CAN_CONTROLLER_CANOPEN
            uint8_t slot = route->subAxis;
            if (slot < CANopenModule::REMOTE_SLAVE_SLOTS) {
                const auto& slave = CANopenModule::s_remoteSlaves[slot];
                if (slave.seen) {
                    cJSON_AddNumberToObject(rs, "position", slave.motorPosition);
                    bool running = (slave.motorStatus & 0x01) != 0;
                    cJSON_AddNumberToObject(rs, "isRunning", running ? 1 : 0);
                    cJSON_AddNumberToObject(rs, "isDone",    running ? 0 : 1);
                } else {
                    cJSON_AddNumberToObject(rs, "isDone", -1);
                }
            } else {
                cJSON_AddNumberToObject(rs, "isDone", -1);
            }
#else
            ESP_LOGW(TAG, "REMOTE routing requires CANopen — motor_get %d ignored", stepperid);
            cJSON_AddNumberToObject(rs, "isDone", -1);
#endif
        }
        cJSON_AddItemToArray(respSteppers, rs);
    }

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "steppers", respSteppers);
    return resp;
#else
    return nullptr;
#endif
}

// ============================================================================
// Laser act — routing-table dispatch
// ============================================================================
cJSON* DeviceRouter::handleLaserAct(cJSON* doc) {
#ifdef LASER_CONTROLLER
    cJSON* laser = cJSON_GetObjectItem(doc, "laser");
    if (!laser) return nullptr;

    cJSON* laserid_item = cJSON_GetObjectItem(laser, "LASERid");
    cJSON* val_item     = cJSON_GetObjectItem(laser, "LASERval");
    if (!laserid_item || !val_item) return nullptr;

    int laserid = laserid_item->valueint;
    int rawVal  = val_item->valueint;

    const auto* route = UC2::RoutingTable::find(
        UC2::RouteEntry::LASER, (uint8_t)laserid);

    if (!route || route->where == UC2::RouteEntry::OFF) {
        ESP_LOGW(TAG, "laser %d has no route", laserid);
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", 0);
        return resp;
    }

    bool ok = true;
    if (route->where == UC2::RouteEntry::LOCAL) {
        ok = LaserController::setLaserVal(laserid, rawVal);
    } else { // REMOTE
#ifdef CAN_CONTROLLER_CANOPEN
        uint16_t pwmVal = (uint16_t)std::min(std::max(rawVal, 0), 65535);
        uint8_t sub = route->subAxis + 1;
        ok = CANopenModule::writeSDO_u16(route->nodeId, UC2_OD::LASER_PWM_VALUE, sub, pwmVal);
        if (!ok) ESP_LOGW(TAG, "Laser SDO failed: node 0x%02X", route->nodeId);
#else
        ESP_LOGW(TAG, "REMOTE routing requires CANopen — laser %d ignored", laserid);
        ok = false;
#endif
    }

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "return", ok ? 1 : 0);
    return resp;
#else
    return nullptr;
#endif
}

// ============================================================================
// Home act — routing-table dispatch
// ============================================================================
cJSON* DeviceRouter::handleHomeAct(cJSON* doc) {
#ifdef HOME_MOTOR
    cJSON* home = cJSON_GetObjectItem(doc, "home");
    if (!home) return nullptr;

    cJSON* stepperid_item = cJSON_GetObjectItem(home, "stepperid");
    if (!stepperid_item) return nullptr;

    int stepperid = stepperid_item->valueint;
    const auto* route = UC2::RoutingTable::find(
        UC2::RouteEntry::HOME, (uint8_t)stepperid);

    if (!route || route->where == UC2::RouteEntry::OFF) {
        ESP_LOGW(TAG, "home %d has no route", stepperid);
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", 0);
        return resp;
    }

    cJSON* speedItem    = cJSON_GetObjectItem(home, "speed");
    cJSON* dirItem      = cJSON_GetObjectItem(home, "direction");
    cJSON* timeoutItem  = cJSON_GetObjectItem(home, "timeout");
    cJSON* releaseItem  = cJSON_GetObjectItem(home, "endstopRelease");
    cJSON* polarityItem = cJSON_GetObjectItem(home, "endstopPolarity");

    int speed     = speedItem    ? std::abs(speedItem->valueint) : 1000;
    int dir       = dirItem      ? dirItem->valueint              : -1;
    int timeout   = timeoutItem  ? timeoutItem->valueint          : 10000;
    int release   = releaseItem  ? releaseItem->valueint          : 0;
    int polarity  = polarityItem ? polarityItem->valueint         : 0;

    bool ok = true;
    if (route->where == UC2::RouteEntry::LOCAL) {
        // Call HomeMotor directly — pass the full JSON for maximum compatibility
        HomeMotor::act(doc);
    } else { // REMOTE
#ifdef CAN_CONTROLLER_CANOPEN
        uint8_t sub = route->subAxis + 1;
        uint32_t spd32 = (uint32_t)speed;
        int8_t   dir8  = (int8_t)dir;
        uint32_t tmo32 = (uint32_t)timeout;
        int32_t  rel32 = (int32_t)release;
        uint8_t  pol8  = (uint8_t)polarity;

        CANopenModule::writeSDO_u32(route->nodeId, UC2_OD::HOMING_SPEED, sub, spd32);
        CANopenModule::writeSDO_u8 (route->nodeId, UC2_OD::HOMING_DIRECTION, sub, (uint8_t)dir8);
        CANopenModule::writeSDO_u32(route->nodeId, UC2_OD::HOMING_TIMEOUT, sub, tmo32);
        CANopenModule::writeSDO_i32(route->nodeId, UC2_OD::HOMING_ENDSTOP_RELEASE, sub, rel32);
        CANopenModule::writeSDO_u8 (route->nodeId, UC2_OD::HOMING_ENDSTOP_POLARITY, sub, pol8);
        uint8_t trigger = 1;
        ok = CANopenModule::writeSDO_u8(route->nodeId, UC2_OD::HOMING_COMMAND, sub, trigger);
        if (!ok) ESP_LOGW(TAG, "Home SDO failed: node 0x%02X", route->nodeId);
#else
        ESP_LOGW(TAG, "REMOTE routing requires CANopen — home %d ignored", stepperid);
        ok = false;
#endif
    }

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "return", ok ? 1 : 0);
    return resp;
#else
    return nullptr;
#endif
}

// ============================================================================
// LED act — routing-table dispatch
// ============================================================================
cJSON* DeviceRouter::handleLedAct(cJSON* doc) {
#ifdef LED_CONTROLLER
    const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::LED, 0);

    if (!route || route->where == UC2::RouteEntry::OFF) {
        ESP_LOGW(TAG, "led 0 has no route");
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", 0);
        return resp;
    }

    if (route->where == UC2::RouteEntry::LOCAL) {
        // LedController::act returns int; wrap into JSON response
        int result = LedController::act(doc);
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", result);
        return resp;
    } else { // REMOTE
#ifdef CAN_CONTROLLER_CANOPEN
        cJSON* led = cJSON_GetObjectItem(doc, "led");
        if (!led) {
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddNumberToObject(resp, "return", 0);
            return resp;
        }

        uint8_t nodeId = route->nodeId;
        bool ok = true;

        // Mode-based fill
        cJSON* mode = cJSON_GetObjectItem(led, "LEDArrMode");
        if (mode && cJSON_IsNumber(mode)) {
            uint8_t m = (uint8_t)mode->valueint;
            ok = CANopenModule::writeSDO_u8(nodeId, UC2_OD::LED_ARRAY_MODE, 0, m) && ok;
        }

        // Brightness
        cJSON* br = cJSON_GetObjectItem(led, "brightness");
        if (br && cJSON_IsNumber(br)) {
            uint8_t b = (uint8_t)br->valueint;
            ok = CANopenModule::writeSDO_u8(nodeId, UC2_OD::LED_BRIGHTNESS, 0, b) && ok;
        }

        // Uniform colour (r, g, b packed into u32)
        cJSON* jr = cJSON_GetObjectItem(led, "r");
        cJSON* jg = cJSON_GetObjectItem(led, "g");
        cJSON* jb = cJSON_GetObjectItem(led, "b");
        if (jr || jg || jb) {
            uint8_t rr = (jr && cJSON_IsNumber(jr)) ? (uint8_t)jr->valueint : 0;
            uint8_t gg = (jg && cJSON_IsNumber(jg)) ? (uint8_t)jg->valueint : 0;
            uint8_t bb = (jb && cJSON_IsNumber(jb)) ? (uint8_t)jb->valueint : 0;
            uint32_t colour = ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | bb;
            ok = CANopenModule::writeSDO_u32(nodeId, UC2_OD::LED_UNIFORM_COLOUR, 0, colour) && ok;
        }

        // Pattern
        cJSON* pat = cJSON_GetObjectItem(led, "patternId");
        if (pat && cJSON_IsNumber(pat)) {
            uint8_t p = (uint8_t)pat->valueint;
            ok = CANopenModule::writeSDO_u8(nodeId, UC2_OD::LED_PATTERN_ID, 0, p) && ok;
        }
        cJSON* patSpeed = cJSON_GetObjectItem(led, "patternSpeed");
        if (patSpeed && cJSON_IsNumber(patSpeed)) {
            uint16_t s = (uint16_t)patSpeed->valueint;
            ok = CANopenModule::writeSDO_u16(nodeId, UC2_OD::LED_PATTERN_SPEED, 0, s) && ok;
        }

        // Per-pixel array — SDO domain (segmented) transfer
        cJSON* arr = cJSON_GetObjectItem(led, "led_array");
        if (arr && cJSON_IsArray(arr)) {
            int n = cJSON_GetArraySize(arr);
            if (n > 0) {
                uint8_t* buf = (uint8_t*)malloc(n * 3);
                if (buf) {
                    for (int i = 0; i < n; i++) {
                        cJSON* px = cJSON_GetArrayItem(arr, i);
                        cJSON* pr = cJSON_GetObjectItem(px, "r");
                        cJSON* pg = cJSON_GetObjectItem(px, "g");
                        cJSON* pb = cJSON_GetObjectItem(px, "b");
                        buf[i*3+0] = (pr && cJSON_IsNumber(pr)) ? (uint8_t)pr->valueint : 0;
                        buf[i*3+1] = (pg && cJSON_IsNumber(pg)) ? (uint8_t)pg->valueint : 0;
                        buf[i*3+2] = (pb && cJSON_IsNumber(pb)) ? (uint8_t)pb->valueint : 0;
                    }
                    ok = CANopenModule::writeSDODomain(nodeId, UC2_OD::LED_PIXEL_DATA, 0,
                                                       buf, n * 3) && ok;
                    free(buf);
                } else {
                    ESP_LOGE(TAG, "LED pixel array malloc failed (%d pixels)", n);
                    ok = false;
                }
            }
        }

        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", ok ? 1 : 0);
        return resp;
#else
        ESP_LOGW(TAG, "LED remote routing requires CANopen");
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", 0);
        return resp;
#endif
    }
#else
    return nullptr;
#endif
}

// ============================================================================
// Galvo act — routing-table dispatch
// ============================================================================
cJSON* DeviceRouter::handleGalvoAct(cJSON* doc) {
#ifdef GALVO_CONTROLLER
    const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::GALVO, 0);

    if (!route || route->where == UC2::RouteEntry::OFF) {
        ESP_LOGW(TAG, "galvo 0 has no route");
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", 0);
        return resp;
    }

    if (route->where == UC2::RouteEntry::LOCAL) {
        return GalvoController::act(doc);
    } else { // REMOTE
#ifdef CAN_CONTROLLER_CANOPEN
        uint8_t nodeId = route->nodeId;
        bool ok = true;

        // Check for stop command
        cJSON* stop_cmd = cJSON_GetObjectItem(doc, "stop");
        if (stop_cmd && cJSON_IsTrue(stop_cmd)) {
            uint8_t cmd = 0;
            ok = CANopenModule::writeSDO_u8(nodeId, UC2_OD::GALVO_COMMAND_WORD, 0, cmd);
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddNumberToObject(resp, "return", ok ? 1 : 0);
            return resp;
        }

        // Check for config — push scan parameters via SDO then issue command
        cJSON* config_obj = cJSON_GetObjectItem(doc, "config");
        if (config_obj) {
            cJSON* item;
            if ((item = cJSON_GetObjectItem(config_obj, "x_min")))
                ok = CANopenModule::writeSDO_i32(nodeId, UC2_OD::GALVO_X_START, 0, item->valueint) && ok;
            if ((item = cJSON_GetObjectItem(config_obj, "y_min")))
                ok = CANopenModule::writeSDO_i32(nodeId, UC2_OD::GALVO_Y_START, 0, item->valueint) && ok;
            if ((item = cJSON_GetObjectItem(config_obj, "nx")))
                ok = CANopenModule::writeSDO_u16(nodeId, UC2_OD::GALVO_N_STEPS_LINE, 0, (uint16_t)item->valueint) && ok;
            if ((item = cJSON_GetObjectItem(config_obj, "ny")))
                ok = CANopenModule::writeSDO_u16(nodeId, UC2_OD::GALVO_N_STEPS_PIXEL, 0, (uint16_t)item->valueint) && ok;
            if ((item = cJSON_GetObjectItem(config_obj, "sample_period_us")))
                ok = CANopenModule::writeSDO_u32(nodeId, UC2_OD::GALVO_SCAN_SPEED, 0, (uint32_t)item->valueint) && ok;
            if ((item = cJSON_GetObjectItem(config_obj, "pre_samples")))
                ok = CANopenModule::writeSDO_u16(nodeId, UC2_OD::GALVO_T_PRE_US, 0, (uint16_t)item->valueint) && ok;
            if ((item = cJSON_GetObjectItem(config_obj, "fly_samples")))
                ok = CANopenModule::writeSDO_u16(nodeId, UC2_OD::GALVO_T_POST_US, 0, (uint16_t)item->valueint) && ok;
            if ((item = cJSON_GetObjectItem(config_obj, "enable_trigger")))
                ok = CANopenModule::writeSDO_u8(nodeId, UC2_OD::GALVO_CAMERA_TRIGGER_MODE, 0, (uint8_t)item->valueint) && ok;

            // Compute x_step, y_step from x_min/x_max/nx and y_min/y_max/ny
            cJSON* xMin = cJSON_GetObjectItem(config_obj, "x_min");
            cJSON* xMax = cJSON_GetObjectItem(config_obj, "x_max");
            cJSON* yMin = cJSON_GetObjectItem(config_obj, "y_min");
            cJSON* yMax = cJSON_GetObjectItem(config_obj, "y_max");
            cJSON* nx   = cJSON_GetObjectItem(config_obj, "nx");
            cJSON* ny   = cJSON_GetObjectItem(config_obj, "ny");
            if (xMin && xMax && nx && nx->valueint > 0) {
                int32_t xStep = (xMax->valueint - xMin->valueint) / nx->valueint;
                ok = CANopenModule::writeSDO_i32(nodeId, UC2_OD::GALVO_X_STEP, 0, xStep) && ok;
            }
            if (yMin && yMax && ny && ny->valueint > 0) {
                int32_t yStep = (yMax->valueint - yMin->valueint) / ny->valueint;
                ok = CANopenModule::writeSDO_i32(nodeId, UC2_OD::GALVO_Y_STEP, 0, yStep) && ok;
            }

            // Issue raster scan command (cmd=3)
            uint8_t cmd = 3;
            ok = CANopenModule::writeSDO_u8(nodeId, UC2_OD::GALVO_COMMAND_WORD, 0, cmd) && ok;
            if (!ok) ESP_LOGW(TAG, "Galvo SDO failed: node 0x%02X", nodeId);

            cJSON* resp = cJSON_CreateObject();
            cJSON_AddNumberToObject(resp, "return", ok ? 1 : 0);
            return resp;
        }

        // Direct X/Y positioning
        cJSON* galvo = cJSON_GetObjectItem(doc, "galvo");
        if (galvo) {
            cJSON* tx = cJSON_GetObjectItem(galvo, "x");
            cJSON* ty = cJSON_GetObjectItem(galvo, "y");
            if (tx) {
                int32_t v = tx->valueint;
                ok = CANopenModule::writeSDO_i32(nodeId, UC2_OD::GALVO_TARGET_POSITION, 1, v) && ok;
            }
            if (ty) {
                int32_t v = ty->valueint;
                ok = CANopenModule::writeSDO_i32(nodeId, UC2_OD::GALVO_TARGET_POSITION, 2, v) && ok;
            }
            if (tx || ty) {
                // Issue goto command (cmd=1)
                uint8_t cmd = 1;
                ok = CANopenModule::writeSDO_u8(nodeId, UC2_OD::GALVO_COMMAND_WORD, 0, cmd) && ok;
            }
        }

        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", ok ? 1 : 0);
        return resp;
#else
        ESP_LOGW(TAG, "Galvo remote routing requires CANopen");
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", 0);
        return resp;
#endif
    }
#else
    return nullptr;
#endif
}

// ============================================================================
// TMC act — routing-table dispatch
// ============================================================================
cJSON* DeviceRouter::handleTmcAct(cJSON* doc) {
#ifdef TMC_CONTROLLER
    // TMC config is board-specific — always local
    int result = TMCController::act(doc);
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "return", result);
    return resp;
#else
    return nullptr;
#endif
}

// ============================================================================
// Laser get — routing-table dispatch
// ============================================================================
cJSON* DeviceRouter::handleLaserGet(cJSON* doc) {
#ifdef LASER_CONTROLLER
    // For now, always query local controller.
    // TODO: REMOTE SDO read for laser state when OD entries are wired up
    return LaserController::get(doc);
#else
    return nullptr;
#endif
}

// ============================================================================
// Home get — routing-table dispatch
// ============================================================================
cJSON* DeviceRouter::handleHomeGet(cJSON* doc) {
#ifdef HOME_MOTOR
    // Homing status is always local (homing runs on the board that owns the motor)
    return HomeMotor::get(doc);
#else
    return nullptr;
#endif
}

// ============================================================================
// LED get — routing-table dispatch
// ============================================================================
cJSON* DeviceRouter::handleLedGet(cJSON* doc) {
#ifdef LED_CONTROLLER
    return LedController::get(doc);
#else
    return nullptr;
#endif
}

// ============================================================================
// Galvo get — routing-table dispatch
// ============================================================================
cJSON* DeviceRouter::handleGalvoGet(cJSON* doc) {
#ifdef GALVO_CONTROLLER
    const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::GALVO, 0);

    if (!route || route->where == UC2::RouteEntry::OFF) {
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", 0);
        return resp;
    }

    if (route->where == UC2::RouteEntry::LOCAL) {
        return GalvoController::get(doc);
    } else { // REMOTE — read galvo status via SDO
#ifdef CAN_CONTROLLER_CANOPEN
        uint8_t nodeId = route->nodeId;
        cJSON* resp = cJSON_CreateObject();

        // Read status word
        uint8_t statusWord = 0;
        size_t readSize = 0;
        if (CANopenModule::readSDO(nodeId, UC2_OD::GALVO_STATUS_WORD, 0,
                                   &statusWord, sizeof(statusWord), &readSize)) {
            cJSON_AddBoolToObject(resp, "running", (statusWord & 0x01) != 0);
            cJSON_AddBoolToObject(resp, "scanActive", (statusWord & 0x02) != 0);
            cJSON_AddBoolToObject(resp, "scanComplete", (statusWord & 0x04) != 0);
        } else {
            cJSON_AddBoolToObject(resp, "running", false);
        }

        // Read actual position
        int32_t posX = 0, posY = 0;
        if (CANopenModule::readSDO(nodeId, UC2_OD::GALVO_ACTUAL_POSITION, 1,
                                   (uint8_t*)&posX, sizeof(posX), &readSize)) {
            cJSON_AddNumberToObject(resp, "x", posX);
        }
        if (CANopenModule::readSDO(nodeId, UC2_OD::GALVO_ACTUAL_POSITION, 2,
                                   (uint8_t*)&posY, sizeof(posY), &readSize)) {
            cJSON_AddNumberToObject(resp, "y", posY);
        }

        cJSON_AddNumberToObject(resp, "return", 1);
        return resp;
#else
        ESP_LOGW(TAG, "Galvo remote get requires CANopen");
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", 0);
        return resp;
#endif
    }
#else
    return nullptr;
#endif
}

// ============================================================================
// TMC get — routing-table dispatch
// ============================================================================
cJSON* DeviceRouter::handleTmcGet(cJSON* doc) {
#ifdef TMC_CONTROLLER
    return TMCController::get(doc);
#else
    return nullptr;
#endif
}

// ============================================================================
// State get — read uptime from slave UC2 OD (x2503 = uptime_seconds, VAR)
// ============================================================================
#ifdef CAN_CONTROLLER_CANOPEN
cJSON* DeviceRouter::handleStateGet(uint8_t nodeId) {
    cJSON* resp = cJSON_CreateObject();

    uint32_t uptime = 0;
    size_t readSize = 0;
    if (CANopenModule::readSDO(nodeId, UC2_OD::UPTIME_SECONDS, 0x00,
                               (uint8_t*)&uptime, 4, &readSize)) {
        cJSON_AddNumberToObject(resp, "uptime", uptime);
        cJSON_AddNumberToObject(resp, "online", 1);
    } else {
        cJSON_AddNumberToObject(resp, "online", 0);
    }

    return resp;
}
#endif // CAN_CONTROLLER_CANOPEN

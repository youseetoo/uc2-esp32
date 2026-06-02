// DeviceRouter.cpp — Routing-table-driven command dispatcher.
// Consults RoutingTable to decide LOCAL vs REMOTE for each logical device.
// Active on ALL builds. REMOTE dispatch requires CAN_CONTROLLER_CANOPEN.
#include "DeviceRouter.h"
#include <PinConfig.h>

#include "../config/RuntimeConfig.h"
#include "RoutingTable.h"
#include "../wifi/Endpoints.h"
#include "esp_log.h"
#include <cstring>
#include <algorithm>

#ifdef CAN_CONTROLLER_CANOPEN
#include "CANopenModule.h"
#include "UC2_OD_Indices.h"
#include "OtaBinaryReceive.h"
#endif

#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
#include "../motor/MotorJsonParser.h"
#endif
// QidRegistry is used by the motor, laser, LED and galvo REMOTE paths to
// emit {"qid":N,"state":"done"} acknowledgements consistent with the LOCAL
// controllers. Include it whenever any of those controllers are present.
#if defined(MOTOR_CONTROLLER) || defined(LASER_CONTROLLER) || defined(LED_CONTROLLER) || defined(GALVO_CONTROLLER)
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
#ifdef DIGITAL_OUT_CONTROLLER
#include "../digitalout/DigitalOutController.h"
#endif
#ifdef DIGITAL_IN_CONTROLLER
#include "../digitalin/DigitalInController.h"
#endif
#include "cJsonTool.h"

#include "../state/State.h"


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

    // Digital I/O — always routed, even on builds without local DIGITAL_*_CONTROLLER.
    // A master without a local DigitalOutController still needs to forward
    // /digitalout_act to a remote GPIO slave; that's the primary use case.
    if (strcmp(task, digitalout_act_endpoint) == 0)
        return handleDigitalOutAct(doc);
    if (strcmp(task, digitalout_get_endpoint) == 0)
        return handleDigitalOutGet(doc);
    if (strcmp(task, digitalin_get_endpoint) == 0)
        return handleDigitalInGet(doc);

    // State act — handles "restart" with optional remote nodeId targeting.
    // state_get stays in SerialProcess for now (local State::get also serves
    // the master uptime; remote uptime is reachable via /can_get).
    if (strcmp(task, state_act_endpoint) == 0) // {"task":"/state_act","state":"restart","nodeId":11}
        return handleStateAct(doc);

#ifdef CAN_CONTROLLER_CANOPEN
    if (strcmp(task, ota_start_endpoint) == 0){
        return handleOtaStart(doc);
    }
#endif

    return nullptr; // not a routed command
}

// ============================================================================
// Motor act — routing-table dispatch
//
// PR-7.7: DeviceRouter is now the single entry point for /motor_act. We first
// run all master-side config sub-parsers (enable/setpos/hardlimits/...) and
// the stage scan orchestration, then route the per-stepper drive command via
// the routing table. The previous behaviour silently dropped every config and
// stage-scan command on routed builds because SerialProcess only fell through
// to MotorJsonParser::act when DeviceRouter returned nullptr.
// ============================================================================
cJSON* DeviceRouter::handleMotorAct(cJSON* doc) {
#ifdef MOTOR_CONTROLLER
    log_i("Handling motor_act via DeviceRouter");

    int qid = cJsonTool::getJsonInt(doc, "qid");

    // ── 1. Config sub-commands — always run locally ──
    // TODO: Master-side configuration that doesn't depend on motor location. For
    // hardware-side fields (setpos, setdir) the parsers themselves apply only
    // to motors physically present on this board; remote forwarding via SDO
    // is deferred (see PR-8). joystickdir/hardlimits are master-side config.
    MotorJsonParser::parseEnableMotor(doc);          // isen
    MotorJsonParser::parseAutoEnableMotor(doc);      // isenauto
    MotorJsonParser::parseSetPosition(doc);          // setpos
    MotorJsonParser::parseMotorPinDirection(doc);    // setdir
    MotorJsonParser::parseSetHardLimits(doc);        // hardlimits
    MotorJsonParser::parseSetJoystickDirection(doc); // joystickdir

#ifdef CAN_CONTROLLER_CANOPEN
    // Forward motor-enable to remote nodes via SDO. parseEnableMotor only
    // handles the local driver(s); without this, a master's {"isen":1} never
    // reaches a slave-mounted motor.
    {
        cJSON* isenItem = cJSON_GetObjectItemCaseSensitive(doc, "isen");
        if (isenItem) {
            uint8_t isen = (cJSON_IsTrue(isenItem)
                            || (cJSON_IsNumber(isenItem) && isenItem->valueint != 0)) ? 1 : 0;
            for (int ax = 0; ax < 4; ax++) {
                const auto* r = UC2::RoutingTable::find(UC2::RouteEntry::MOTOR, (uint8_t)ax);
                if (!r || r->where != UC2::RouteEntry::REMOTE) continue;
                CANopenModule::writeSDO_u8(r->nodeId, UC2_OD::MOTOR_ENABLE,
                                           (uint8_t)(r->subAxis + 1), isen);
            }
        }
    }

    // Forward hardlimits (enabled/polarity/clear) to remote nodes via SDO.
    // parseSetHardLimits above only updated local axes; remote axes need OD
    // writes to 0x2031/0x2032 (config) and 0x2030 (clear command).
    {
        cJSON* hardObj = cJSON_GetObjectItemCaseSensitive(doc, "hardlimits");
        if (hardObj) {
            cJSON* stprs = cJSON_GetObjectItemCaseSensitive(hardObj, "steppers");
            if (stprs && cJSON_IsArray(stprs)) {
                cJSON* stp = nullptr;
                cJSON_ArrayForEach(stp, stprs) {
                    cJSON* idItem = cJSON_GetObjectItemCaseSensitive(stp, "stepperid");
                    if (!cJSON_IsNumber(idItem)) continue;
                    int axis = idItem->valueint;
                    const auto* r = UC2::RoutingTable::find(UC2::RouteEntry::MOTOR, (uint8_t)axis);
                    if (!r || r->where != UC2::RouteEntry::REMOTE) continue;
                    uint8_t sub = (uint8_t)(r->subAxis + 1);

                    cJSON* clearItem = cJSON_GetObjectItemCaseSensitive(stp, "clear");
                    if (clearItem && cJSON_IsNumber(clearItem) && clearItem->valueint) {
                        log_i("Forward hardlimit CLEAR -> node 0x%02X axis %u",
                              r->nodeId, r->subAxis);
                        CANopenModule::writeSDO_u8(r->nodeId, UC2_OD::HARDLIMIT_COMMAND, sub, 1);
                        continue;
                    }

                    cJSON* enabledItem  = cJSON_GetObjectItemCaseSensitive(stp, "enabled");
                    cJSON* polarityItem = cJSON_GetObjectItemCaseSensitive(stp, "polarity");
                    uint8_t enabled  = enabledItem  ? (enabledItem->valueint  ? 1 : 0) : 0;
                    uint8_t polarity = polarityItem ? (polarityItem->valueint ? 1 : 0) : 0;
                    log_i("Forward hardlimit CONFIG -> node 0x%02X axis %u: en=%u pol=%u",
                          r->nodeId, r->subAxis, enabled, polarity);
                    CANopenModule::writeSDO_u8(r->nodeId, UC2_OD::HARDLIMIT_ENABLED,  sub, enabled);
                    CANopenModule::writeSDO_u8(r->nodeId, UC2_OD::HARDLIMIT_POLARITY, sub, polarity);
                }
            }
        }
    }
#endif

#ifdef STAGE_SCAN
    // Stage scanning is master-side orchestration — it issues motor.move
    // commands internally which themselves go through DeviceRouter again.
    MotorJsonParser::parseStageScan(doc);
#endif

    // ── 2. Drive command — route per stepper ──
    cJSON* motor = cJSON_GetObjectItem(doc, "motor");
    if (!motor) {
        // Config-only request (e.g. {"task":"/motor_act","isen":1}) — done.
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "qid", qid);
        cJSON_AddNumberToObject(resp, "return", 1);
        return resp;
    }
    cJSON* steppers = cJSON_GetObjectItem(motor, "steppers");
    if (!steppers || !cJSON_IsArray(steppers))
    {
        log_i("motor_act missing 'steppers' array");
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "qid", qid);
        cJSON_AddNumberToObject(resp, "return", 1);
        return resp;
    }

    cJSON* respSteppers = cJSON_CreateArray();
    int n = cJSON_GetArraySize(steppers);

    // Extract QID so TPDO-driven completion (syncRpdoToModules_master) and
    // FocusMotor::stopStepper (LOCAL completion) can report it via QidRegistry.
    int motorQid = 0;
    {
        cJSON* qidItem = cJSON_GetObjectItem(doc, "qid");
        if (qidItem && cJSON_IsNumber(qidItem)) motorQid = qidItem->valueint;
    }
    int remoteStepperCount = 0;
    int localStepperCount  = 0;

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
        int32_t speed = speedItem ? speedItem->valueint : 0;
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
                    FocusMotor::getData()[stepperid]->qid = motorQid;
                    FocusMotor::startStepper(stepperid, 0);
                }
            } else {
                MotorData* d = FocusMotor::getData()[stepperid];
                if (d) {
                    d->targetPosition   = pos;
                    d->speed            = speed;
                    #ifdef USE_FASTACCEL
                    d->maxspeed         = speed;
                    #else
                    d->maxspeed         = abs(speed);   // ensure setMaxSpeed >= requested speed
                    #endif
                    d->absolutePosition = isAbs;
                    d->isforever        = isForever;
                    d->isStop           = false;
                    d->stopped          = false;
                    d->qid              = motorQid;
                    if (accel > 0) d->acceleration = accel;
                    else if (d->acceleration <= 0) d->acceleration = 40000;
                    FocusMotor::startStepper(stepperid, 0); // TODO: Shouldn't we use stopstepper instead?
                }
            }
            localStepperCount++;
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

    // Register the QID with the total number of dispatched steppers (LOCAL +
    // REMOTE) so QidRegistry emits {"qid":N,"state":"done"} once every axis
    // signals completion (FocusMotor::stopStepper for LOCAL, TPDO callback for
    // REMOTE).
    int totalStepperCount = localStepperCount + remoteStepperCount;
    if (motorQid > 0 && totalStepperCount > 0)
        QidRegistry::registerQid(motorQid, (uint8_t)totalStepperCount);

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddItemToObject(resp, "steppers", respSteppers);
    if (motorQid > 0)
        cJSON_AddNumberToObject(resp, "qid", motorQid);
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
    // Bare {"task":"/motor_get"} (no "motor" filter) → return all local axes
    // via the existing controller. Previously this was handled by the now-
    // deleted SerialProcess fallback branch.
    if (!motor) return MotorJsonParser::get(doc);
    cJSON* steppers = cJSON_GetObjectItem(motor, "steppers");
    if (!steppers || !cJSON_IsArray(steppers)) return MotorJsonParser::get(doc);

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
// Mirrors the motor pattern: register the qid in QidRegistry on the master,
// fire the SDO write, and immediately report the action done. We cannot wait
// for a TPDO ack like the motor does because the laser has no
// running->stopped transition mapped to a TPDO; the SDO server on the slave
// updates OD_RAM.x2100_laser_pwm_value synchronously and the slave's
// syncRpdoToModules_slave dispatches it to LaserController on the next 1ms
// tick, so by the time the master returns the value is already (or about to
// be) applied. A spurious SDO timeout on the master does NOT mean the slave
// missed the write — the OD entry is updated as part of the SDO server state
// machine before the response frame is sent.
// ============================================================================
cJSON* DeviceRouter::handleLaserAct(cJSON* doc) {
#ifdef LASER_CONTROLLER
    cJSON* laserid_item = cJSON_GetObjectItem(doc, "LASERid");
    cJSON* val_item     = cJSON_GetObjectItem(doc, "LASERval");
    if (!laserid_item || !val_item) {
        log_i("Laser act missing LASERid or LASERval");
        return nullptr;
    }

    int laserid = laserid_item->valueint;
    int rawVal  = val_item->valueint;

    // Extract qid (mirrors handleMotorAct). LOCAL path registers/reports the
    // qid inside LaserController::setLaserVal already, so we only register on
    // the REMOTE path here.
    int laserQid = 0;    
    cJSON* qidItem = cJSON_GetObjectItem(doc, "qid");
    if (qidItem && cJSON_IsNumber(qidItem)) laserQid = qidItem->valueint;


    const auto* route = UC2::RoutingTable::find(
        UC2::RouteEntry::LASER, (uint8_t)laserid);
    log_i("DeviceRouter handling laser_act for laser %d with value %d, route: %p", laserid, rawVal, route);
    if (!route || route->where == UC2::RouteEntry::OFF) {
        log_e("Laser %d has no route", laserid);
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", 0);
        return resp;
    }

    bool ok = true;
    if (route->where == UC2::RouteEntry::LOCAL) {
        log_i("Routing laser_act to LOCAL laser %d with value %d", laserid, rawVal);
        // Delegate to LaserController::act so we keep full feature parity:
        // PWM frequency/resolution, despeckle, servo mode, qid registration.
        // act() returns qid (>=0) and logs validation errors internally; we
        // treat the call as successful from the router's perspective.
        (void)LaserController::act(doc);
    } else { // REMOTE
#ifdef CAN_CONTROLLER_CANOPEN
        uint16_t pwmVal = (uint16_t)std::min(std::max(rawVal, 0), 65535);
        uint8_t  sub    = route->subAxis + 1;
        log_i("Routing laser_act to REMOTE node 0x%02X subAxis %d (OD sub 0x%02X) for laser %d with value %u",
              route->nodeId, route->subAxis, sub, laserid, (unsigned)pwmVal);

        // Register the qid BEFORE dispatching, so a fast TPDO/ack path could
        // report it back later if we ever add a laser status TPDO.
        if (laserQid > 0) QidRegistry::registerQid(laserQid, 1);

        bool sdoOk = CANopenModule::writeSDO_u16(route->nodeId, UC2_OD::LASER_PWM_VALUE, sub, pwmVal);
        if (!sdoOk) {
            // Match motor's severity (warn, not error). The OD value is
            // updated by the SDO server before the response frame is sent,
            // so a master-side timeout does NOT mean the slave missed it.
            log_w("Laser SDO write timed out: node 0x%02X sub 0x%02X (best-effort delivered)",
                  route->nodeId, sub);
        }

        // Acknowledge the qid immediately — there is no laser TPDO completion
        // to wait for. Treat the dispatch as done so the master emits
        // {"qid":N,"state":"done"} / {"qid":N,"success":1} just like motor.
        if (laserQid > 0) QidRegistry::reportActionDone(laserQid);

        // Always return success on the REMOTE path: the OD is updated as part
        // of the SDO server transaction, and the slave's syncRpdoToModules
        // will pick it up on the next tick. Reporting failure here caused the
        // host to retry needlessly even though the laser had already changed.
        ok = true;
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
// Parses {"task":"/home_act", "home": {"steppers": [{"stepperid":1,
//   "timeout":20000, "speed":5000, "maxspeed":10000, "direction":1,
//   "endstoppolarity":0, "endstoprelease":0, "endoffset":0}]}, "qid":1234}
// Iterates all stepper entries and routes each to LOCAL (startHome) or REMOTE (SDO).
// ============================================================================
cJSON* DeviceRouter::handleHomeAct(cJSON* doc) {
#ifdef HOME_MOTOR
    log_i("Handling home_act via DeviceRouter");

    cJSON* home = cJSON_GetObjectItem(doc, "home");
    if (!home) {
        log_w("home_act missing 'home' object");
        return nullptr;
    }
    cJSON* steppers = cJSON_GetObjectItem(home, "steppers");
    if (!steppers || !cJSON_IsArray(steppers)) {
        log_w("home_act missing 'steppers' array inside 'home'");
        return nullptr;
    }

    int qid = cJsonTool::getJsonInt(doc, "qid");

    cJSON* stp = nullptr;
    cJSON_ArrayForEach(stp, steppers) {
        cJSON* stepperid_item = cJSON_GetObjectItemCaseSensitive(stp, key_stepperid);
        if (!stepperid_item) continue;

        int stepperid = stepperid_item->valueint;
        const auto* route = UC2::RoutingTable::find(
            UC2::RouteEntry::HOME, (uint8_t)stepperid);

        if (!route || route->where == UC2::RouteEntry::OFF) {
            ESP_LOGW(TAG, "home %d has no route", stepperid);
            continue;
        }

        int homeTimeout  = cJsonTool::getJsonInt(stp, key_home_timeout);
        int homeSpeed    = cJsonTool::getJsonInt(stp, key_home_speed);
        int homeMaxspeed = cJsonTool::getJsonInt(stp, key_home_maxspeed);
        int homeDir      = cJsonTool::getJsonInt(stp, key_home_direction);
        int homePolarity = cJsonTool::getJsonInt(stp, key_home_endstoppolarity);
        int homeRelease  = cJsonTool::getJsonInt(stp, key_home_endstoprelease);
        int homeOffset   = cJsonTool::getJsonInt(stp, key_home_endoffset);

        if (route->where == UC2::RouteEntry::LOCAL) {
            log_i("Routing home_act to LOCAL stepper %d: speed=%d dir=%d timeout=%d maxspeed=%d polarity=%d offset=%d",
                  stepperid, homeSpeed, homeDir, homeTimeout, homeMaxspeed, homePolarity, homeOffset);
            HomeMotor::startHome(stepperid, homeTimeout, homeSpeed, homeMaxspeed, homeDir, homePolarity, homeOffset, qid);
        } else { // REMOTE
#ifdef CAN_CONTROLLER_CANOPEN
            uint8_t sub = route->subAxis + 1;
            log_i("Routing home_act to REMOTE node 0x%02X subAxis %d (OD sub 0x%02X) for stepper %d: speed=%d dir=%d timeout=%d release=%d polarity=%d",
                  route->nodeId, route->subAxis, sub, stepperid, homeSpeed, homeDir, homeTimeout, homeRelease, homePolarity);
            CANopenModule::writeSDO_u32(route->nodeId, UC2_OD::HOMING_SPEED, sub, (uint32_t)std::abs(homeSpeed));
            CANopenModule::writeSDO_u8 (route->nodeId, UC2_OD::HOMING_DIRECTION, sub, (uint8_t)(int8_t)homeDir);
            CANopenModule::writeSDO_u32(route->nodeId, UC2_OD::HOMING_TIMEOUT, sub, (uint32_t)homeTimeout);
            CANopenModule::writeSDO_i32(route->nodeId, UC2_OD::HOMING_ENDSTOP_RELEASE, sub, (int32_t)homeRelease);
            CANopenModule::writeSDO_u8 (route->nodeId, UC2_OD::HOMING_ENDSTOP_POLARITY, sub, (uint8_t)homePolarity);
            bool ok = CANopenModule::writeSDO_u8(route->nodeId, UC2_OD::HOMING_COMMAND, sub, 1);
            if (!ok) ESP_LOGW(TAG, "Home SDO failed: node 0x%02X", route->nodeId);
#else
            ESP_LOGW(TAG, "REMOTE routing requires CANopen — home %d ignored", stepperid);
#endif
        }
    }

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "return", 1);
    return resp;
#else
    return nullptr;
#endif
}

// ============================================================================
// LED act — routing-table dispatch.
// Mirrors motor/laser pattern: register qid in QidRegistry on REMOTE path and
// report done immediately (no LED status TPDO is mapped, so we cannot wait
// for an ack — the SDO server updates the slave OD synchronously).
// ============================================================================
cJSON* DeviceRouter::handleLedAct(cJSON* doc) {
#ifdef LED_CONTROLLER
    log_i("Handling led_act via DeviceRouter");
    const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::LED, 0);
    log_i("DeviceRouter found route for LED: %p", route);
    // Extract qid up-front so both LOCAL and REMOTE paths can use it.
    int ledQid = 0;
    cJSON* qidItem = cJSON_GetObjectItem(doc, "qid");
    if (qidItem && cJSON_IsNumber(qidItem)) ledQid = qidItem->valueint;
    
    
    if (!route || route->where == UC2::RouteEntry::OFF) {
        log_e("led 0 has no route");
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", 0);
        return resp;
    }

    if (route->where == UC2::RouteEntry::LOCAL) {
        // LedController::act returns int; wrap into JSON response
        int result = LedController::act(doc);
        log_i("Routing led_act to LOCAL LED controller, result: %d", result);
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", result);
        return resp;
    } else { // REMOTE
#ifdef CAN_CONTROLLER_CANOPEN
        log_i("Routing led_act to REMOTE node 0x%02X", route->nodeId);
        cJSON* led = cJSON_GetObjectItem(doc, "led");
        if (!led) {
            log_e("led_act missing 'led' object");
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddNumberToObject(resp, "return", 0);
            return resp;
        }

        uint8_t nodeId = route->nodeId;
        bool ok = true;
        bool anySdoTimeout = false;

        // Register the qid before issuing any SDO writes — see laser comment.
        if (ledQid > 0) QidRegistry::registerQid(ledQid, 1);

        // Derive LED array mode from the high-level "action" field so the slave
        // can reconstruct the intended pattern.  The slave's setMode() maps:
        //   0 = OFF
        //   1 = FILL (uniform colour for the whole strip)
        //   2 = HALVES left    3 = HALVES right
        //   4 = HALVES top     5 = HALVES bottom
        // Falls back to any explicit "LEDArrMode" value if present.
        //
        // NOTE: actions like "single" (ledIndex+RGB) and pattern actions
        // ("circles", "rings") are NOT representable by the current SDO OD
        // (LED_ARRAY_MODE + LED_UNIFORM_COLOUR).  Until a dedicated OD entry
        // for per-index pixel writes is added on the slave, route them via
        // the per-pixel LED_PIXEL_DATA path if the caller supplied a
        // "led_array"; otherwise refuse so we don't silently light the whole
        // strip.
        uint8_t ledMode = 1; // default: on (fill)
        bool actionUnsupported = false;
        cJSON* jAction = cJSON_GetObjectItem(led, "action");
        if (jAction && cJSON_IsString(jAction)) {
            const char* actStr = jAction->valuestring;
            if (strcasecmp(actStr, "off") == 0) {
                ledMode = 0;
            } else if (strcasecmp(actStr, "halves") == 0) {
                cJSON* jRegion = cJSON_GetObjectItem(led, "region");
                const char* region = (jRegion && cJSON_IsString(jRegion)) ? jRegion->valuestring : "left";
                if      (strcasecmp(region, "right")  == 0) ledMode = 3;
                else if (strcasecmp(region, "top")    == 0) ledMode = 4;
                else if (strcasecmp(region, "bottom") == 0) ledMode = 5;
                else                                         ledMode = 2; // default: left
            } else if (strcasecmp(actStr, "single")  == 0 ||
                       strcasecmp(actStr, "circles") == 0 ||
                       strcasecmp(actStr, "rings")   == 0) {
                actionUnsupported = true;
            }
            // fill / uniform → mode 1
        }
        // Allow explicit "LEDArrMode" override
        cJSON* jLedArrMode = cJSON_GetObjectItem(led, "LEDArrMode");
        if (jLedArrMode && cJSON_IsNumber(jLedArrMode)) ledMode = (uint8_t)jLedArrMode->valueint;

        // Refuse actions we can't represent via the current OD, unless the
        // caller supplied an explicit per-pixel led_array (handled below) or
        // overrode LEDArrMode directly.
        cJSON* jArrEarly = cJSON_GetObjectItem(led, "led_array");
        if (actionUnsupported && !(jArrEarly && cJSON_IsArray(jArrEarly)) && !jLedArrMode) {
            // "single" → write 5 bytes to LED_SINGLE_PIXEL (0x2211).
            if (jAction && strcasecmp(jAction->valuestring, "single") == 0) {
                cJSON* jIdx = cJSON_GetObjectItem(led, "ledIndex");
                if (!jIdx || !cJSON_IsNumber(jIdx)) {
                    log_e("led_act single: missing ledIndex");
                    if (ledQid > 0) QidRegistry::reportActionDone(ledQid);
                    cJSON* resp = cJSON_CreateObject();
                    cJSON_AddNumberToObject(resp, "return", 0);
                    return resp;
                }
                uint16_t idx = (uint16_t)jIdx->valueint;
                cJSON* sjr = cJSON_GetObjectItem(led, "r");
                cJSON* sjg = cJSON_GetObjectItem(led, "g");
                cJSON* sjb = cJSON_GetObjectItem(led, "b");
                uint8_t rr = (sjr && cJSON_IsNumber(sjr)) ? (uint8_t)sjr->valueint : 0;
                uint8_t gg = (sjg && cJSON_IsNumber(sjg)) ? (uint8_t)sjg->valueint : 0;
                uint8_t bb = (sjb && cJSON_IsNumber(sjb)) ? (uint8_t)sjb->valueint : 0;
                uint8_t payload[5] = {
                    (uint8_t)(idx & 0xFF), (uint8_t)((idx >> 8) & 0xFF),
                    rr, gg, bb
                };
                log_i("Routing LED single pixel: idx=%u R:%u G:%u B:%u", idx, rr, gg, bb);
                bool sok = CANopenModule::writeSDO(nodeId, UC2_OD::LED_SINGLE_PIXEL, 0,
                                                   payload, sizeof(payload));
                if (ledQid > 0) QidRegistry::reportActionDone(ledQid);
                cJSON* resp = cJSON_CreateObject();
                cJSON_AddNumberToObject(resp, "return", sok ? 1 : 0);
                return resp;
            }
            // "circles" / "rings" → write 5 bytes to LED_SHAPE (0x2212).
            if (jAction && (strcasecmp(jAction->valuestring, "rings")   == 0 ||
                            strcasecmp(jAction->valuestring, "circles") == 0)) {
                uint8_t shape = (strcasecmp(jAction->valuestring, "circles") == 0) ? 1 : 0;
                cJSON* jRad = cJSON_GetObjectItem(led, "radius");
                uint8_t radius = (jRad && cJSON_IsNumber(jRad)) ? (uint8_t)jRad->valueint : 1;
                cJSON* sjr2 = cJSON_GetObjectItem(led, "r");
                cJSON* sjg2 = cJSON_GetObjectItem(led, "g");
                cJSON* sjb2 = cJSON_GetObjectItem(led, "b");
                uint8_t rr = (sjr2 && cJSON_IsNumber(sjr2)) ? (uint8_t)sjr2->valueint : 0;
                uint8_t gg = (sjg2 && cJSON_IsNumber(sjg2)) ? (uint8_t)sjg2->valueint : 0;
                uint8_t bb = (sjb2 && cJSON_IsNumber(sjb2)) ? (uint8_t)sjb2->valueint : 0;
                uint8_t payload[5] = { shape, radius, rr, gg, bb };
                log_i("Routing LED shape: %s radius=%u R:%u G:%u B:%u",
                      jAction->valuestring, radius, rr, gg, bb);
                bool sok = CANopenModule::writeSDO(nodeId, UC2_OD::LED_SHAPE, 0,
                                                   payload, sizeof(payload));
                if (ledQid > 0) QidRegistry::reportActionDone(ledQid);
                cJSON* resp = cJSON_CreateObject();
                cJSON_AddNumberToObject(resp, "return", sok ? 1 : 0);
                return resp;
            }
            // Any remaining unsupported action — refuse.
            log_w("led_act: action '%s' not supported via REMOTE SDO routing. "
                  "Send 'led_array' for arbitrary patterns.",
                  (jAction && jAction->valuestring) ? jAction->valuestring : "?");
            if (ledQid > 0) QidRegistry::reportActionDone(ledQid);
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddNumberToObject(resp, "return", 0);
            return resp;
        }

        log_i("Setting LED array mode to %u", ledMode);
        if (!CANopenModule::writeSDO_u8(nodeId, UC2_OD::LED_ARRAY_MODE, 0, ledMode)) {
            anySdoTimeout = true;
            ok = false;
        }

        // Brightness — skip subsequent SDO writes once a write has failed,
        // so an unhealthy bus doesn't cost 250 ms per remaining field.
        cJSON* br = cJSON_GetObjectItem(led, "brightness");
        if (ok && br && cJSON_IsNumber(br)) {
            uint8_t b = (uint8_t)br->valueint;
            log_i("Setting LED brightness to %u", b);
            if (!CANopenModule::writeSDO_u8(nodeId, UC2_OD::LED_BRIGHTNESS, 0, b)) { anySdoTimeout = true; ok = false; }
        }

        // Uniform colour (r, g, b packed into u32)
        cJSON* jr = cJSON_GetObjectItem(led, "r");
        cJSON* jg = cJSON_GetObjectItem(led, "g");
        cJSON* jb = cJSON_GetObjectItem(led, "b");
        if (ok && (jr || jg || jb)) {
            uint8_t rr = (jr && cJSON_IsNumber(jr)) ? (uint8_t)jr->valueint : 0;
            uint8_t gg = (jg && cJSON_IsNumber(jg)) ? (uint8_t)jg->valueint : 0;
            uint8_t bb = (jb && cJSON_IsNumber(jb)) ? (uint8_t)jb->valueint : 0;
            uint32_t colour = ((uint32_t)rr << 16) | ((uint32_t)gg << 8) | bb;
            log_i("Setting LED uniform colour to R:%u G:%u B:%u (0x%06X)", rr, gg, bb, colour);
            if (!CANopenModule::writeSDO_u32(nodeId, UC2_OD::LED_UNIFORM_COLOUR, 0, colour)) { anySdoTimeout = true; ok = false; }
        }

        // Pattern
        cJSON* pat = cJSON_GetObjectItem(led, "patternId");
        if (ok && pat && cJSON_IsNumber(pat)) {
            uint8_t p = (uint8_t)pat->valueint;
            log_i("Setting LED pattern ID to %u", p);
            if (!CANopenModule::writeSDO_u8(nodeId, UC2_OD::LED_PATTERN_ID, 0, p)) { anySdoTimeout = true; ok = false; }
        }
        cJSON* patSpeed = cJSON_GetObjectItem(led, "patternSpeed");
        if (ok && patSpeed && cJSON_IsNumber(patSpeed)) {
            uint16_t s = (uint16_t)patSpeed->valueint;
            log_i("Setting LED pattern speed to %u", s);
            if (!CANopenModule::writeSDO_u16(nodeId, UC2_OD::LED_PATTERN_SPEED, 0, s)) { anySdoTimeout = true; ok = false; }
        }

        // Per-pixel array — SDO domain (segmented) transfer
        cJSON* arr = cJSON_GetObjectItem(led, "led_array");
        if (ok && arr && cJSON_IsArray(arr)) {
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
                    bool domOk = CANopenModule::writeSDODomain(nodeId, UC2_OD::LED_PIXEL_DATA, 0,
                                                                buf, n * 3);
                    if (!domOk) {
                        log_w("LED pixel SDO domain write timed out: node 0x%02X", nodeId);
                        anySdoTimeout = true;
                    }
                    free(buf);
                } else {
                    log_e("LED pixel array malloc failed (%d pixels)", n);
                    ok = false;
                }
            }
        }

        // Acknowledge qid like motor/laser. SDO timeouts are best-effort
        // delivered (the OD is updated by the SDO server before the response
        // frame is sent), so we report done regardless and the caller sees
        // {"qid":N,"state":"done"} consistently.
        if (anySdoTimeout) {
            log_w("LED SDO write(s) timed out: node 0x%02X (best-effort delivered)", nodeId);
        }
        if (ledQid > 0) QidRegistry::reportActionDone(ledQid);

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
        log_e("galvo 0 has no route");
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", 0);
        return resp;
    }

    if (route->where == UC2::RouteEntry::LOCAL) {
        log_i("Routing galvo_act to LOCAL galvo controller");
        return GalvoController::act(doc);
    } else { // REMOTE
#ifdef CAN_CONTROLLER_CANOPEN
        log_i("Routing galvo_act to REMOTE node 0x%02X", route->nodeId);
        uint8_t nodeId = route->nodeId;
        bool ok = true;

        // Check for stop command
        cJSON* stop_cmd = cJSON_GetObjectItem(doc, "stop");
        if (stop_cmd && cJSON_IsTrue(stop_cmd)) {
            uint8_t cmd = 0;
            log_i("Issuing galvo stop command to node 0x%02X", nodeId);
            ok = CANopenModule::writeSDO_u8(nodeId, UC2_OD::GALVO_COMMAND_WORD, 0, cmd);
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddNumberToObject(resp, "return", ok ? 1 : 0);
            return resp;
        }

        // Check for config — push scan parameters via SDO then issue command
        cJSON* config_obj = cJSON_GetObjectItem(doc, "config");
        if (config_obj) {
            cJSON* item;
            // {"task":"/galvo_act", "config": {"nx":256,"ny":256,"x_min":500,"x_max":3500,"y_min":500,"y_max":3500,"pre_samples":0,"fly_samples":0,"sample_period_us":0,"frame_count":0,"bidirectional":false}}
            log_i("Configuring galvo scan parameters for node 0x%02X", nodeId);
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
                log_i("Computed x_step: (x_max %d - x_min %d) / nx %d", xMax->valueint, xMin->valueint, nx->valueint);
                int32_t xStep = (xMax->valueint - xMin->valueint) / nx->valueint;
                ok = CANopenModule::writeSDO_i32(nodeId, UC2_OD::GALVO_X_STEP, 0, xStep) && ok;
            }
            if (yMin && yMax && ny && ny->valueint > 0) {
                log_i("Computed y_step: (y_max %d - y_min %d) / ny %d", yMax->valueint, yMin->valueint, ny->valueint);
                int32_t yStep = (yMax->valueint - yMin->valueint) / ny->valueint;
                ok = CANopenModule::writeSDO_i32(nodeId, UC2_OD::GALVO_Y_STEP, 0, yStep) && ok;
            }

            // Issue raster scan command (cmd=3)
            uint8_t cmd = 3;
            ok = CANopenModule::writeSDO_u8(nodeId, UC2_OD::GALVO_COMMAND_WORD, 0, cmd) && ok;
            // TODO: galvo should follow the motor pattern — register qid here
            // and wire syncRpdoToModules_master to call QidRegistry::reportActionDone
            // on a GALVO_STATUS_WORD running->stopped transition. The slave already
            // pushes OD_RAM.x2603_galvo_status_word via syncModulesToTpdo, but the
            // master side currently ignores it.
            if (!ok) log_w("Galvo SDO write timed out: node 0x%02X (best-effort delivered)", nodeId);

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
// REMOTE: write the TMC OD entries (0x2020..0x2026) for the slave's axis,
// the slave's syncRpdoToModules_slave reassembles a TMCData and applies it.
// LOCAL : delegate to TMCController::act for the legacy single-board path.
// ============================================================================
cJSON* DeviceRouter::handleTmcAct(cJSON* doc) {
#ifdef TMC_CONTROLLER
    int axis = cJsonTool::getJsonInt(doc, "axis");
    if (axis < 0) axis = 0;

    const auto* route = UC2::RoutingTable::find(
        UC2::RouteEntry::TMC, (uint8_t)axis);

    // Default to local when no route is configured (single-board builds).
    bool isRemote = (route && route->where == UC2::RouteEntry::REMOTE);

    if (!isRemote) {
        int result = TMCController::act(doc);
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", result);
        return resp;
    }

#ifdef CAN_CONTROLLER_CANOPEN
    uint8_t nodeId = route->nodeId;
    uint8_t sub    = (uint8_t)(route->subAxis + 1);
    bool ok = true;

    log_i("Routing tmc_act to REMOTE node 0x%02X axis %u (logical %d)",
          nodeId, route->subAxis, axis);

    int v;
    v = cJsonTool::getJsonInt(doc, "msteps");
    if (v > 0) ok = CANopenModule::writeSDO_u16(nodeId, UC2_OD::TMC_MICROSTEPS,           sub, (uint16_t)v) && ok;
    v = cJsonTool::getJsonInt(doc, "rms_current");
    if (v > 0) ok = CANopenModule::writeSDO_u16(nodeId, UC2_OD::TMC_RMS_CURRENT,          sub, (uint16_t)v) && ok;
    v = cJsonTool::getJsonInt(doc, "sgthrs");
    if (v > 0) ok = CANopenModule::writeSDO_u8 (nodeId, UC2_OD::TMC_STALLGUARD_THRESHOLD, sub, (uint8_t)v)  && ok;
    v = cJsonTool::getJsonInt(doc, "semin");
    if (v > 0) ok = CANopenModule::writeSDO_u8 (nodeId, UC2_OD::TMC_COOLSTEP_SEMIN,       sub, (uint8_t)v)  && ok;
    v = cJsonTool::getJsonInt(doc, "semax");
    if (v > 0) ok = CANopenModule::writeSDO_u8 (nodeId, UC2_OD::TMC_COOLSTEP_SEMAX,       sub, (uint8_t)v)  && ok;
    v = cJsonTool::getJsonInt(doc, "blank_time");
    if (v > 0) ok = CANopenModule::writeSDO_u8 (nodeId, UC2_OD::TMC_BLANK_TIME,           sub, (uint8_t)v)  && ok;
    v = cJsonTool::getJsonInt(doc, "toff");
    if (v > 0) ok = CANopenModule::writeSDO_u8 (nodeId, UC2_OD::TMC_TOFF,                 sub, (uint8_t)v)  && ok;

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "return", ok ? 1 : 0);
    return resp;
#else
    log_w("REMOTE TMC routing requires CANopen — falling back to local");
    int result = TMCController::act(doc);
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "return", result);
    return resp;
#endif
#else
    return nullptr;
#endif
}

// ============================================================================
// Laser get — routing-table dispatch
// ============================================================================
cJSON* DeviceRouter::handleLaserGet(cJSON* doc) {
#ifdef LASER_CONTROLLER
    // PR-7.8 Issue 7: warn if any laser is REMOTE — get over CAN not yet implemented.
    for (uint8_t id = 0; id < 4; ++id) {
        const auto* r = UC2::RoutingTable::find(UC2::RouteEntry::LASER, id);
        if (r && r->where == UC2::RouteEntry::REMOTE) {
            log_w("laser_get: laser %u is REMOTE — get over CAN not implemented yet", id);
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddNumberToObject(resp, "return", 0);
            cJSON_AddStringToObject(resp, "error",
                "device is on remote node — get not implemented over CAN yet");
            return resp;
        }
    }
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
    // PR-7.8 Issue 7: warn if any homing axis is REMOTE.
    for (uint8_t ax = 0; ax < 4; ++ax) {
        const auto* r = UC2::RoutingTable::find(UC2::RouteEntry::HOME, ax);
        if (r && r->where == UC2::RouteEntry::REMOTE) {
            log_w("home_get: axis %u is REMOTE — get over CAN not implemented yet", ax);
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddNumberToObject(resp, "return", 0);
            cJSON_AddStringToObject(resp, "error",
                "device is on remote node — get not implemented over CAN yet");
            return resp;
        }
    }
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
    // PR-7.8 Issue 7: warn if LED array is REMOTE.
    const auto* r = UC2::RoutingTable::find(UC2::RouteEntry::LED, 0);
    if (r && r->where == UC2::RouteEntry::REMOTE) {
        log_w("led_get: LED array is REMOTE — get over CAN not implemented yet");
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", 0);
        cJSON_AddStringToObject(resp, "error",
            "device is on remote node — get not implemented over CAN yet");
        return resp;
    }
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

// ============================================================================
// State act — routes restart/reboot to a remote node when "nodeId" is given,
// otherwise delegates to the local State::act handler.
// JSON shape examples:
//   {"task":"/state_act","restart":1}                — local restart
//   {"task":"/state_act","restart":1,"nodeId":11}    — restart remote node 11
// ============================================================================
cJSON* DeviceRouter::handleStateAct(cJSON* doc) {
#ifdef CAN_CONTROLLER_CANOPEN
    cJSON* nodeItem = cJSON_GetObjectItemCaseSensitive(doc, "nodeId");
    cJSON* restartItem = cJSON_GetObjectItemCaseSensitive(doc, "restart");
    bool wantRestart = restartItem
        && (cJSON_IsTrue(restartItem)
            || (cJSON_IsNumber(restartItem) && restartItem->valueint != 0));

    if (nodeItem && cJSON_IsNumber(nodeItem) && wantRestart) {
        uint8_t nodeId = (uint8_t)nodeItem->valueint;
        log_w("state_act restart -> remote node 0x%02X via SDO", nodeId);
        bool ok = CANopenModule::writeSDO_u8(nodeId, UC2_OD::REBOOT_COMMAND, 0x00, 1);
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", ok ? 1 : 0);
        cJSON_AddNumberToObject(resp, "nodeId", nodeId);
        return resp;
    }
#endif
    // Local handling — State::act will perform ESP.restart() or other ops
    int rc = State::act(doc);
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "return", rc);
    return resp;
}

#ifdef CAN_CONTROLLER_CANOPEN
// ============================================================================
// /ota_start — JSON preamble that primes binary OTA receive.
//
// JSON format:
//   {"task":"/ota_start","ota":{"nodeId":11,"size":1048576,"crc32":"0xABCD1234"}}
//
// On success, OtaBinaryReceive::isActive() becomes true and SerialProcess::loop
// will divert raw Serial bytes into the receive buffer until the full
// firmware payload arrives.
// ============================================================================
cJSON* DeviceRouter::handleOtaStart(cJSON* doc) {
    cJSON* otaObj = cJSON_GetObjectItem(doc, "ota");
    log_i("handleOtaStart: found ota object: %p", otaObj);
    if (!otaObj) {
        log_e("ota_start missing 'ota' object");
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "ota_status", "error");
        cJSON_AddStringToObject(resp, "error", "missing_ota_object");
        return resp;
    }

    cJSON* nodeIdItem = cJSON_GetObjectItem(otaObj, "nodeId");
    cJSON* sizeItem   = cJSON_GetObjectItem(otaObj, "size");
    if (!nodeIdItem || !cJSON_IsNumber(nodeIdItem) ||
        !sizeItem   || !cJSON_IsNumber(sizeItem)) {
        log_e("ota_start missing/invalid 'nodeId' or 'size' in ota object");
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "ota_status", "error");
        cJSON_AddStringToObject(resp, "error", "missing_nodeId_or_size");
        return resp;
    }

    // nodeId: must be a valid CANopen node ID (1..127)
    int nodeIdRaw = nodeIdItem->valueint;
    if (nodeIdRaw < 1 || nodeIdRaw > 127) {
        log_e("ota_start nodeId out of range (1..127): %d", nodeIdRaw);
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "ota_status", "error");
        cJSON_AddStringToObject(resp, "error", "invalid_nodeId");
        return resp;
    }
    else{
        log_i("ota_start nodeId: %d", nodeIdRaw);
    }
    uint8_t nodeId = (uint8_t)nodeIdRaw;

    // size: cJSON ints are 32-bit; for very large firmware (>2 GiB) the JSON
    // would have to come in as a double — read valuedouble as a fallback so
    // sizes up to 2^53 are represented losslessly.
    double sizeD = cJSON_IsNumber(sizeItem) ? sizeItem->valuedouble : 0.0;
    if (sizeD <= 0.0 || sizeD > (double)UINT32_MAX) {
        log_e("ota_start size out of range: %f", sizeD);
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "ota_status", "error");
        cJSON_AddStringToObject(resp, "error", "invalid_size");
        return resp;
    }
    uint32_t size = (uint32_t)sizeD;

    // crc32: optional — accept "0x......" / decimal string / number; 0 disables check.
    uint32_t crc32 = 0;
    cJSON* crc32Item = cJSON_GetObjectItem(otaObj, "crc32");
    if (crc32Item) {
        if (cJSON_IsString(crc32Item) && crc32Item->valuestring) {
            const char* crcStr = crc32Item->valuestring;
            // strtoul with base 0 auto-detects "0x" prefix vs decimal.
            char* endPtr = nullptr;
            unsigned long parsed = strtoul(crcStr, &endPtr, 0);
            if (endPtr == crcStr) {
                log_w("ota_start crc32 string unparseable, defaulting to 0: %s", crcStr);
            } else {
                crc32 = (uint32_t)parsed;
            }
        } else if (cJSON_IsNumber(crc32Item)) {
            // Use valuedouble — valueint is signed 32-bit and would corrupt
            // CRC values with the high bit set.
            crc32 = (uint32_t)crc32Item->valuedouble;
        } else {
            log_w("ota_start crc32 item is not a string or number, defaulting to 0");
        }
    } else {
        log_i("ota_start crc32 not provided, defaulting to 0 (no check)");
    }

    log_i("ota_start: nodeId=%u size=%u crc32=0x%08X", nodeId, size, crc32);
    // TODO: Are we certain we parse the CRC correctly?
    return OtaBinaryReceive::begin(nodeId, size, crc32);
}
#endif

// ============================================================================
// Digital I/O — LOCAL or REMOTE dispatch
// ----------------------------------------------------------------------------
// Request shape (act):
//   {"task":"/digitalout_act","digitaloutid":1,"digitaloutval":1}            // LOCAL
//   {"task":"/digitalout_act","node":60,"digitaloutid":1,"digitaloutval":1}  // REMOTE
//   {"task":"/digitalout_act","node":60,"digitaloutid":2,"digitaloutval":-1} // REMOTE pulse
// Request shape (get):
//   {"task":"/digitalin_get","digitalinid":1}            // LOCAL
//   {"task":"/digitalin_get","node":60,"digitalinid":1}  // REMOTE (SDO read x2300 sub N)
// ============================================================================

// Returns the node-id from doc->"node" if present + numeric, else 0.
static uint8_t extractRemoteNode(cJSON* doc) {
    if (!doc) return 0;
    cJSON* it = cJSON_GetObjectItemCaseSensitive(doc, "node");
    if (it && cJSON_IsNumber(it)) return (uint8_t)it->valueint;
    return 0;
}

cJSON* DeviceRouter::handleDigitalOutAct(cJSON* doc) {
    uint8_t node = extractRemoteNode(doc);
    // We use literal key strings here instead of the JsonKeys.h constants
    // because those are gated behind #ifdef DIGITAL_OUT_CONTROLLER and the
    // master build (which is the primary REMOTE forwarder) does NOT define
    // DIGITAL_OUT_CONTROLLER. The keys themselves are stable JSON contract.
    int id  = cJsonTool::getJsonInt(doc, "digitaloutid");
    int val = cJsonTool::getJsonInt(doc, "digitaloutval");

#ifdef CAN_CONTROLLER_CANOPEN
    // REMOTE path: explicit "node" key on a master forces SDO forwarding.
    if (runtimeConfig.isMaster() && node > 0) {
        log_i("DR digitalout_act REMOTE  node=%u id=%d val=%d", node, id, val);
        bool ok = CANopenModule_setRemoteGpio(node, (uint8_t)id, val);
        if (!ok) {
            log_w("DR digitalout_act REMOTE  node=%u id=%d val=%d -> SDO FAIL",
                  node, id, val);
        }
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "node", node);
        cJSON_AddNumberToObject(resp, "digitaloutid", id);
        cJSON_AddNumberToObject(resp, "digitaloutval", val);
        cJSON_AddBoolToObject(resp, "ok", ok);
        cJSON* qidItem = cJSON_GetObjectItem(doc, "qid");
        if (qidItem && cJSON_IsNumber(qidItem))
            cJSON_AddNumberToObject(resp, "qid", qidItem->valueint);
        return resp;
    }
#endif

    // LOCAL path: delegate to the controller. DigitalOutController::act
    // returns an int (the qid), not cJSON — wrap it into a small response so
    // callers still see {"qid":..., "ok":true} like the routed paths.
#ifdef DIGITAL_OUT_CONTROLLER
    if (runtimeConfig.digitalOut) {
        log_i("DR digitalout_act LOCAL  id=%d val=%d", id, val);
        int retQid = DigitalOutController::act(doc);
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "digitaloutid", id);
        cJSON_AddNumberToObject(resp, "digitaloutval", val);
        cJSON_AddNumberToObject(resp, "qid", retQid);
        cJSON_AddBoolToObject(resp, "ok", true);
        return resp;
    }
#endif
    log_w("DR digitalout_act: no LOCAL path (DIGITAL_OUT_CONTROLLER off) and no \"node\" routed");
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "error", "digitalout not available locally and no remote node specified");
    cJSON_AddNumberToObject(resp, "digitaloutid", id);
    cJSON_AddNumberToObject(resp, "digitaloutval", val);
    return resp;
}

cJSON* DeviceRouter::handleDigitalOutGet(cJSON* doc) {
    // We don't push x2301 OD reads remotely — outputs are write-only state on
    // the slave's side anyway. So /digitalout_get always runs LOCAL when
    // available, and returns an error otherwise.
#ifdef DIGITAL_OUT_CONTROLLER
    if (runtimeConfig.digitalOut)
        return DigitalOutController::get(doc);
#endif
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "error", "digitalout_get not available locally");
    return resp;
}

cJSON* DeviceRouter::handleDigitalInGet(cJSON* doc) {
    uint8_t node = extractRemoteNode(doc);
    int id = cJsonTool::getJsonInt(doc, "digitalinid");

#ifdef CAN_CONTROLLER_CANOPEN
    if (runtimeConfig.isMaster() && node > 0) {
        // SDO read OD 0x2300 sub `id` from the remote slave. id is 1-based to
        // mirror the local API; sub 0 is the array length per CiA 301.
        uint8_t v = 0; size_t got = 0;
        bool ok = CANopenModule::readSDO(node, 0x2300, (uint8_t)id,
                                         &v, sizeof(v), &got);
        log_i("DR digitalin_get REMOTE  node=%u id=%d -> ok=%d val=%u",
              node, id, ok, (unsigned)v);
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "node", node);
        cJSON_AddNumberToObject(resp, "digitalinid", id);
        cJSON_AddNumberToObject(resp, "digitalinval", ok ? v : 0);
        cJSON_AddBoolToObject(resp, "ok", ok);
        return resp;
    }
#endif

#ifdef DIGITAL_IN_CONTROLLER
    if (runtimeConfig.digitalIn)
        return DigitalInController::get(doc);
#endif
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "error", "digitalin_get not available locally and no remote node specified");
    cJSON_AddNumberToObject(resp, "digitalinid", id);
    return resp;
}

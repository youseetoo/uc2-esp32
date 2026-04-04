/**
 * DeviceRouter.cpp
 *
 * UC2 Device Router implementation.
 * Routes motor/laser/LED commands to local GPIO controllers or CAN satellites.
 */
#include "DeviceRouter.h"
#include "CANopen/CanOpenStack.h"
#include "CANopen/SDOHandler.h"
#include "CANopen/NMTManager.h"
#include <string.h>
#include <Arduino.h>
#include "esp_log.h"

// Pull in local controllers when they are compiled in
#ifdef MOTOR_CONTROLLER
#include "motor/FocusMotor.h"
#include "motor/MotorTypes.h"
#endif
#ifdef LASER_CONTROLLER
#include "laser/LaserController.h"
#endif
#ifdef LED_CONTROLLER
#include "led/LedController.h"
#endif
#include "cJSON.h"

#define TAG_DR "UC2_DR"

namespace DeviceRouter {

// ---------------------------------------------------------------------------
// Route tables
// ---------------------------------------------------------------------------
static UC2_MotorRoute s_motorRoutes[UC2_ROUTER_GLOBAL_MAX_MOTOR];
static UC2_LaserRoute s_laserRoutes[UC2_ROUTER_GLOBAL_MAX_LASER];
static UC2_LEDRoute   s_ledRoutes[UC2_ROUTER_GLOBAL_MAX_LED];

// Dynamic CAN slot assignment — CAN devices start right after local ones
static uint8_t s_localMotorCount = 0;
static uint8_t s_localLaserCount = 0;
static bool    s_hasLocalLED     = false;
static uint8_t s_nextCanMotorGid = 0;
static uint8_t s_nextCanLaserGid = 0;
static uint8_t s_nextCanLedGid   = 0;

// ---------------------------------------------------------------------------
// Global-ID → CAN node / local axis mapping helpers
// ---------------------------------------------------------------------------

// Returns true if globalId maps to a local GPIO device (0-9)
static inline bool isLocal(uint8_t globalId)
{
    return globalId < UC2_ROUTER_LOCAL_MAX;
}

// For a CAN-mapped global ID (≥10), compute the CAN node-ID and local index.
// CAN node-IDs: 0x10,(0x14),(0x18),... each covering 10 device IDs.
static inline uint8_t canNodeIdForGlobalId(uint8_t globalId)
{
    if (isLocal(globalId)) return 0;
    uint8_t slot = (globalId - UC2_ROUTER_LOCAL_MAX) / UC2_ROUTER_CAN_NODE_STRIDE;
    return (uint8_t)(0x10u + slot * 4u);  // 0x10, 0x14, 0x18, ...
}

static inline uint8_t canLocalIdForGlobalId(uint8_t globalId)
{
    if (isLocal(globalId)) return globalId;
    return (uint8_t)((globalId - UC2_ROUTER_LOCAL_MAX) % UC2_ROUTER_CAN_NODE_STRIDE);
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
void init(uint8_t localMotorCount, uint8_t localLaserCount, bool hasLocalLED)
{
    s_localMotorCount = localMotorCount;
    s_localLaserCount = localLaserCount;
    s_hasLocalLED     = hasLocalLED;

    // Mark everything unavailable first
    for (auto& r : s_motorRoutes) r = { UC2_RouteType::UNAVAILABLE, 0, 0, 0 };
    for (auto& r : s_laserRoutes) r = { UC2_RouteType::UNAVAILABLE, 0, 0, 0 };
    for (auto& r : s_ledRoutes)   r = { UC2_RouteType::UNAVAILABLE, 0 };

    // Register local motor axes
    for (uint8_t i = 0; i < localMotorCount && i < UC2_ROUTER_GLOBAL_MAX_MOTOR; i++) {
        s_motorRoutes[i] = { UC2_RouteType::LOCAL_GPIO, i, 0, 0 };
        log_i(TAG_DR, "Registered local motor route: globalId=%d → localAxis=%d",
              i, s_motorRoutes[i].localAxisId);
    }

    // Register local laser channels
    for (uint8_t i = 0; i < localLaserCount && i < UC2_ROUTER_GLOBAL_MAX_LASER; i++) {
        s_laserRoutes[i] = { UC2_RouteType::LOCAL_GPIO, i, 0, 0 };
        log_i(TAG_DR, "Registered local laser route: globalId=%d → localAxis=%d",
              i, s_laserRoutes[i].localAxisId);
    }

    // Register local LED (index 0 = local)
    if (hasLocalLED) {
        s_ledRoutes[0] = { UC2_RouteType::LOCAL_GPIO, 0 };
        log_i(TAG_DR, "Registered local LED route: globalId=0 → localAxis=0");
    }

    // CAN devices start right after local ones
    s_nextCanMotorGid = localMotorCount;
    s_nextCanLaserGid = localLaserCount;
    s_nextCanLedGid   = hasLocalLED ? 1 : 0;

    ESP_LOGI(TAG_DR, "DeviceRouter init: %d local motors, %d local lasers, LED=%d, CAN base motor=%d laser=%d led=%d",
             localMotorCount, localLaserCount, (int)hasLocalLED,
             s_nextCanMotorGid, s_nextCanLaserGid, s_nextCanLedGid);
}

// ---------------------------------------------------------------------------
// Dynamic slave route updates
// ---------------------------------------------------------------------------
void onSlaveOnline(uint8_t canNodeId, uint8_t caps,
                   uint8_t motorCount, uint8_t laserCount)
{
    ESP_LOGI(TAG_DR, "Slave 0x%02X online: caps=0x%02X motors=%d lasers=%d",
             canNodeId, caps, motorCount, laserCount);

    // Check if this node already has routes (reconnect after going offline)
    bool reactivated = false;
    for (auto& r : s_motorRoutes) {
        if (r.canNodeId == canNodeId && r.type == UC2_RouteType::UNAVAILABLE) {
            r.type = UC2_RouteType::CAN_REMOTE;
            reactivated = true;
        }
    }
    for (auto& r : s_laserRoutes) {
        if (r.canNodeId == canNodeId && r.type == UC2_RouteType::UNAVAILABLE) {
            r.type = UC2_RouteType::CAN_REMOTE;
            reactivated = true;
        }
    }
    for (auto& r : s_ledRoutes) {
        if (r.canNodeId == canNodeId && r.type == UC2_RouteType::UNAVAILABLE) {
            r.type = UC2_RouteType::CAN_REMOTE;
            reactivated = true;
        }
    }
    if (reactivated) {
        ESP_LOGI(TAG_DR, "Slave 0x%02X re-activated existing routes", canNodeId);
        return;
    }

    // Brand new node — assign CAN devices right after existing entries
    if (caps & UC2_CAP_MOTOR) {
        for (uint8_t a = 0; a < motorCount; a++) {
            if (s_nextCanMotorGid < UC2_ROUTER_GLOBAL_MAX_MOTOR) {
                s_motorRoutes[s_nextCanMotorGid] = { UC2_RouteType::CAN_REMOTE, a, canNodeId, a };
                ESP_LOGI(TAG_DR, "  motor[%d] → CAN 0x%02X axis %d",
                         s_nextCanMotorGid, canNodeId, a);
                s_nextCanMotorGid++;
            }
        }
    }

    if (caps & UC2_CAP_LASER) {
        for (uint8_t a = 0; a < laserCount; a++) {
            if (s_nextCanLaserGid < UC2_ROUTER_GLOBAL_MAX_LASER) {
                s_laserRoutes[s_nextCanLaserGid] = { UC2_RouteType::CAN_REMOTE, a, canNodeId, a };
                ESP_LOGI(TAG_DR, "  laser[%d] → CAN 0x%02X ch %d",
                         s_nextCanLaserGid, canNodeId, a);
                s_nextCanLaserGid++;
            }
        }
    }

    if (caps & UC2_CAP_LED) {
        if (s_nextCanLedGid < UC2_ROUTER_GLOBAL_MAX_LED) {
            s_ledRoutes[s_nextCanLedGid] = { UC2_RouteType::CAN_REMOTE, canNodeId };
            ESP_LOGI(TAG_DR, "  LED[%d] → CAN 0x%02X", s_nextCanLedGid, canNodeId);
            s_nextCanLedGid++;
        }
    }
}

void onSlaveOffline(uint8_t canNodeId)
{
    ESP_LOGW(TAG_DR, "Slave 0x%02X offline — marking routes UNAVAILABLE", canNodeId);
    for (auto& r : s_motorRoutes) {
        if (r.type == UC2_RouteType::CAN_REMOTE && r.canNodeId == canNodeId) {
            r.type = UC2_RouteType::UNAVAILABLE;
        }
    }
    for (auto& r : s_laserRoutes) {
        if (r.type == UC2_RouteType::CAN_REMOTE && r.canNodeId == canNodeId) {
            r.type = UC2_RouteType::UNAVAILABLE;
        }
    }
    for (auto& r : s_ledRoutes) {
        if (r.type == UC2_RouteType::CAN_REMOTE && r.canNodeId == canNodeId) {
            r.type = UC2_RouteType::UNAVAILABLE;
        }
    }
}

// ---------------------------------------------------------------------------
// Route lookup
// ---------------------------------------------------------------------------

const UC2_MotorRoute* getMotorRoute(uint8_t globalMotorId)
{
    if (globalMotorId >= UC2_ROUTER_GLOBAL_MAX_MOTOR) {
        log_w("getMotorRoute: globalMotorId %d out of range", globalMotorId);
        return nullptr;
    }
    const UC2_MotorRoute* r = &s_motorRoutes[globalMotorId];
    log_i("getMotorRoute: globalMotorId=%d type=%d localAxisId=%d canNodeId=0x%02X canAxisId=%d",
          globalMotorId, (int)r->type, r->localAxisId,
          r->canNodeId, r->canAxisId);
    return r;
}

const UC2_LaserRoute* getLaserRoute(uint8_t globalLaserId)
{
    if (globalLaserId >= UC2_ROUTER_GLOBAL_MAX_LASER) 
        return nullptr;
    return &s_laserRoutes[globalLaserId];
}

const UC2_LEDRoute* getLEDRoute(uint8_t globalLedId)
{
    if (globalLedId >= UC2_ROUTER_GLOBAL_MAX_LED) return nullptr;
    return &s_ledRoutes[globalLedId];
}

// ---------------------------------------------------------------------------
// Motor dispatch
// ---------------------------------------------------------------------------

bool motorMove(uint8_t globalMotorId, int32_t targetPos, int32_t speed,
               bool isAbsolute, int16_t qid)
{
    const UC2_MotorRoute* r = getMotorRoute(globalMotorId);
    if (!r || r->type == UC2_RouteType::UNAVAILABLE) {
        ESP_LOGW(TAG_DR, "motorMove: no route for global ID %d", globalMotorId);
        return false;
    }

    if (r->type == UC2_RouteType::LOCAL_GPIO) {
#ifdef MOTOR_CONTROLLER
        uint8_t cmd = isAbsolute ? UC2_MOTOR_CMD_MOVE_ABS : UC2_MOTOR_CMD_MOVE_REL;
        MotorData* md = FocusMotor::getData()[r->localAxisId];
        md->targetPosition   = targetPos;
        md->speed            = speed;
        md->absolutePosition = isAbsolute;
        md->isforever        = false;
        md->isStop           = false;
        FocusMotor::startStepper(r->localAxisId, 0);
        return true;
#else
        ESP_LOGW(TAG_DR, "MOTOR_CONTROLLER not compiled in");
        return false;
#endif
    }

    if (r->type == UC2_RouteType::CAN_REMOTE) {
        uint8_t cmd = isAbsolute ? UC2_MOTOR_CMD_MOVE_ABS : UC2_MOTOR_CMD_MOVE_REL;
        return CanOpenStack::sendMotorMove(r->canNodeId, r->canAxisId,
                                          targetPos, speed, cmd, qid);
    }

    return false;
}

bool motorStop(uint8_t globalMotorId, int16_t qid)
{
    const UC2_MotorRoute* r = getMotorRoute(globalMotorId);
    if (!r || r->type == UC2_RouteType::UNAVAILABLE) return false;

    if (r->type == UC2_RouteType::LOCAL_GPIO) {
#ifdef MOTOR_CONTROLLER
        FocusMotor::stopStepper(r->localAxisId);
        return true;
#else
        return false;
#endif
    }
    return CanOpenStack::sendMotorStop(r->canNodeId, r->canAxisId, qid);
}

bool motorHome(uint8_t globalMotorId, int16_t qid)
{
    const UC2_MotorRoute* r = getMotorRoute(globalMotorId);
    if (!r || r->type == UC2_RouteType::UNAVAILABLE) return false;

    if (r->type == UC2_RouteType::LOCAL_GPIO) {
#ifdef MOTOR_CONTROLLER
        // FocusMotor homing is handled via HomeMotor — call stop here
        FocusMotor::stopStepper(r->localAxisId);
        return true;
#else
        return false;
#endif
    }
    return CanOpenStack::sendMotorMove(r->canNodeId, r->canAxisId,
                                       0, 0, UC2_MOTOR_CMD_HOME, qid);
}

bool motorGetPos(uint8_t globalMotorId, int32_t* posOut)
{
    const UC2_MotorRoute* r = getMotorRoute(globalMotorId);
    if (!r || r->type == UC2_RouteType::UNAVAILABLE) return false;

    if (r->type == UC2_RouteType::LOCAL_GPIO) {
#ifdef MOTOR_CONTROLLER
        *posOut = FocusMotor::getCurrentMotorPosition(r->localAxisId);
        return true;
#else
        return false;
#endif
    }
    return SDOHandler::motorGetPos(r->canNodeId, r->canAxisId, posOut);
}

bool motorIsRunning(uint8_t globalMotorId)
{
    const UC2_MotorRoute* r = getMotorRoute(globalMotorId);
    if (!r || r->type == UC2_RouteType::UNAVAILABLE) return false;

    if (r->type == UC2_RouteType::LOCAL_GPIO) {
#ifdef MOTOR_CONTROLLER
        return FocusMotor::isRunning(r->localAxisId);
#else
        return false;
#endif
    }
    bool running = false;
    SDOHandler::motorIsRunning(r->canNodeId, r->canAxisId, &running);
    return running;
}

// ---------------------------------------------------------------------------
// Laser dispatch
// ---------------------------------------------------------------------------

bool laserSet(uint8_t globalLaserId, uint16_t pwm, int16_t qid)
{
    const UC2_LaserRoute* r = getLaserRoute(globalLaserId);
    if (!r || r->type == UC2_RouteType::UNAVAILABLE) {
        ESP_LOGW(TAG_DR, "laserSet: no route for global ID %d", globalLaserId);
        return false;
    }

    if (r->type == UC2_RouteType::LOCAL_GPIO) {
#ifdef LASER_CONTROLLER
        return LaserController::setLaserVal(r->localChannelId, (int)pwm, 0, 0, qid);
#else
        return false;
#endif
    }
    return CanOpenStack::sendLaserSet(r->canNodeId, r->canChannelId, pwm, qid);
}

// ---------------------------------------------------------------------------
// LED dispatch
// ---------------------------------------------------------------------------

bool ledSet(uint8_t globalLedId, uint8_t mode,
            uint8_t r, uint8_t g, uint8_t b, uint16_t ledIndex, int16_t qid)
{
    const UC2_LEDRoute* route = getLEDRoute(globalLedId);
    if (!route || route->type == UC2_RouteType::UNAVAILABLE) {
        ESP_LOGW(TAG_DR, "ledSet: no route for global ID %d", globalLedId);
        return false;
    }

    if (route->type == UC2_RouteType::LOCAL_GPIO) {
#ifdef LED_CONTROLLER
        LedCommand cmd = {};
        cmd.mode     = (LedMode)mode;
        cmd.r        = r;
        cmd.g        = g;
        cmd.b        = b;
        cmd.ledIndex = ledIndex;
        cmd.qid      = (int16_t)qid;
        LedController::execLedCommand(cmd);
        return true;
#else
        return false;
#endif
    }
    return CanOpenStack::sendLEDSet(route->canNodeId, mode, r, g, b, ledIndex, qid);
}

// ---------------------------------------------------------------------------
// Route table introspection
// ---------------------------------------------------------------------------

void dumpRouteTable()
{
    Serial.println("[DeviceRouter] === Route Table ===");
    Serial.println("  Motors:");
    for (uint8_t i = 0; i < UC2_ROUTER_GLOBAL_MAX_MOTOR; i++) {
        const auto& route = s_motorRoutes[i];
        if (route.type == UC2_RouteType::UNAVAILABLE) continue;
        if (route.type == UC2_RouteType::LOCAL_GPIO)
            Serial.printf("    [%2d] LOCAL axis %d\n", i, route.localAxisId);
        else
            Serial.printf("    [%2d] CAN 0x%02X axis %d\n", i, route.canNodeId, route.canAxisId);
    }
    Serial.println("  Lasers:");
    for (uint8_t i = 0; i < UC2_ROUTER_GLOBAL_MAX_LASER; i++) {
        const auto& route = s_laserRoutes[i];
        if (route.type == UC2_RouteType::UNAVAILABLE) continue;
        if (route.type == UC2_RouteType::LOCAL_GPIO)
            Serial.printf("    [%2d] LOCAL ch %d\n", i, route.localChannelId);
        else
            Serial.printf("    [%2d] CAN 0x%02X ch %d\n", i, route.canNodeId, route.canChannelId);
    }
    Serial.println("  LEDs:");
    for (uint8_t i = 0; i < UC2_ROUTER_GLOBAL_MAX_LED; i++) {
        const auto& route = s_ledRoutes[i];
        if (route.type == UC2_RouteType::UNAVAILABLE) continue;
        if (route.type == UC2_RouteType::LOCAL_GPIO)
            Serial.printf("    [%2d] LOCAL\n", i);
        else
            Serial.printf("    [%2d] CAN 0x%02X\n", i, route.canNodeId);
    }
    Serial.println("[DeviceRouter] ===================");
}

void* getRouteTableJSON()
{
    cJSON* root = cJSON_CreateObject();
    cJSON* motors  = cJSON_AddArrayToObject(root, "motors");
    cJSON* lasers  = cJSON_AddArrayToObject(root, "lasers");
    cJSON* leds    = cJSON_AddArrayToObject(root, "leds");

    for (uint8_t i = 0; i < UC2_ROUTER_GLOBAL_MAX_MOTOR; i++) {
        const auto& r = s_motorRoutes[i];
        if (r.type == UC2_RouteType::UNAVAILABLE) continue;
        cJSON* e = cJSON_CreateObject();
        cJSON_AddNumberToObject(e, "id", i);
        if (r.type == UC2_RouteType::LOCAL_GPIO) {
            cJSON_AddStringToObject(e, "type", "local");
            cJSON_AddNumberToObject(e, "local_axis", r.localAxisId);
        } else {
            cJSON_AddStringToObject(e, "type", "can");
            cJSON_AddNumberToObject(e, "can_node", r.canNodeId);
            cJSON_AddNumberToObject(e, "can_axis", r.canAxisId);
        }
        cJSON_AddItemToArray(motors, e);
    }

    for (uint8_t i = 0; i < UC2_ROUTER_GLOBAL_MAX_LASER; i++) {
        const auto& r = s_laserRoutes[i];
        if (r.type == UC2_RouteType::UNAVAILABLE) continue;
        cJSON* e = cJSON_CreateObject();
        cJSON_AddNumberToObject(e, "id", i);
        if (r.type == UC2_RouteType::LOCAL_GPIO) {
            cJSON_AddStringToObject(e, "type", "local");
            cJSON_AddNumberToObject(e, "local_ch", r.localChannelId);
        } else {
            cJSON_AddStringToObject(e, "type", "can");
            cJSON_AddNumberToObject(e, "can_node", r.canNodeId);
            cJSON_AddNumberToObject(e, "can_ch", r.canChannelId);
        }
        cJSON_AddItemToArray(lasers, e);
    }

    for (uint8_t i = 0; i < UC2_ROUTER_GLOBAL_MAX_LED; i++) {
        const auto& r = s_ledRoutes[i];
        if (r.type == UC2_RouteType::UNAVAILABLE) continue;
        cJSON* e = cJSON_CreateObject();
        cJSON_AddNumberToObject(e, "id", i);
        if (r.type == UC2_RouteType::LOCAL_GPIO) {
            cJSON_AddStringToObject(e, "type", "local");
        } else {
            cJSON_AddStringToObject(e, "type", "can");
            cJSON_AddNumberToObject(e, "can_node", r.canNodeId);
        }
        cJSON_AddItemToArray(leds, e);
    }

    return (void*)root;
}

} // namespace DeviceRouter

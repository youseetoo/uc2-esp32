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
    // Mark everything unavailable first
    for (auto& r : s_motorRoutes) r = { UC2_RouteType::UNAVAILABLE, 0, 0, 0 };
    for (auto& r : s_laserRoutes) r = { UC2_RouteType::UNAVAILABLE, 0, 0, 0 };
    for (auto& r : s_ledRoutes)   r = { UC2_RouteType::UNAVAILABLE, 0 };

    // Register local motor axes
    for (uint8_t i = 0; i < localMotorCount && i < UC2_ROUTER_LOCAL_MAX; i++) {
        s_motorRoutes[i] = { UC2_RouteType::LOCAL_GPIO, i, 0, 0 };
    }

    // Register local laser channels
    for (uint8_t i = 0; i < localLaserCount && i < UC2_ROUTER_LOCAL_MAX; i++) {
        s_laserRoutes[i] = { UC2_RouteType::LOCAL_GPIO, i, 0, 0 };
    }

    // Register local LED (index 0 = local)
    if (hasLocalLED) {
        s_ledRoutes[0] = { UC2_RouteType::LOCAL_GPIO, 0 };
    }

    ESP_LOGI(TAG_DR, "DeviceRouter init: %d local motors, %d local lasers, LED=%d",
             localMotorCount, localLaserCount, (int)hasLocalLED);
}

// ---------------------------------------------------------------------------
// Dynamic slave route updates
// ---------------------------------------------------------------------------
void onSlaveOnline(uint8_t canNodeId, uint8_t caps,
                   uint8_t motorCount, uint8_t laserCount)
{
    ESP_LOGI(TAG_DR, "Slave 0x%02X online: caps=0x%02X motors=%d lasers=%d",
             canNodeId, caps, motorCount, laserCount);

    // Compute global ID base for this CAN node
    // node 0x10 → base 10, node 0x14 → base 20, node 0x18 → base 30 ...
    uint8_t slot = (canNodeId >= 0x10u) ? ((canNodeId - 0x10u) / 4u) : 0u;
    uint8_t motorBase = (uint8_t)(UC2_ROUTER_LOCAL_MAX + slot * UC2_ROUTER_CAN_NODE_STRIDE);
    uint8_t laserBase = motorBase;  // Same scheme; separate per device type

    if (caps & UC2_CAP_MOTOR) {
        for (uint8_t a = 0; a < motorCount; a++) {
            uint8_t gid = motorBase + a;
            if (gid < UC2_ROUTER_GLOBAL_MAX_MOTOR) {
                s_motorRoutes[gid] = { UC2_RouteType::CAN_REMOTE, a, canNodeId, a };
                ESP_LOGD(TAG_DR, "  motor[%d] → CAN 0x%02X axis %d", gid, canNodeId, a);
            }
        }
    }

    if (caps & UC2_CAP_LASER) {
        for (uint8_t a = 0; a < laserCount; a++) {
            uint8_t gid = laserBase + a;
            if (gid < UC2_ROUTER_GLOBAL_MAX_LASER) {
                s_laserRoutes[gid] = { UC2_RouteType::CAN_REMOTE, a, canNodeId, a };
                ESP_LOGD(TAG_DR, "  laser[%d] → CAN 0x%02X ch %d", gid, canNodeId, a);
            }
        }
    }

    if (caps & UC2_CAP_LED) {
        // LED slot 0 = local, 1 = CAN 0x10, 2 = CAN 0x14 ...
        uint8_t ledSlot = slot + 1u;
        if (ledSlot < UC2_ROUTER_GLOBAL_MAX_LED) {
            s_ledRoutes[ledSlot] = { UC2_RouteType::CAN_REMOTE, canNodeId };
            ESP_LOGD(TAG_DR, "  LED[%d] → CAN 0x%02X", ledSlot, canNodeId);
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
    if (globalMotorId >= UC2_ROUTER_GLOBAL_MAX_MOTOR) return nullptr;
    return &s_motorRoutes[globalMotorId];
}

const UC2_LaserRoute* getLaserRoute(uint8_t globalLaserId)
{
    if (globalLaserId >= UC2_ROUTER_GLOBAL_MAX_LASER) return nullptr;
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
        LedController::act(&cmd);
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

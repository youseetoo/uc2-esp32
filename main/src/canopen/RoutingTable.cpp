// RoutingTable.cpp — Data-driven routing table, built at boot.
// See RoutingTable.h for the design rationale.
#include "RoutingTable.h"
#include <PinConfig.h>

#include "../config/RuntimeConfig.h"
#include "esp_log.h"

static const char* TAG = "routing";

namespace UC2 {

// Static storage
RouteEntry RoutingTable::table[MAX_ROUTES];
uint8_t    RoutingTable::count = 0;

// ============================================================================
// Default node-id map for master routing.
// Keyed by (Type, logicalId) — easy to grep and update.
// ============================================================================
struct DefaultNodeMap {
    RouteEntry::Type type;
    uint8_t logicalId;
    uint8_t nodeId;
    uint8_t subAxis;
};

// Master: for each logical device, which CAN node-id and sub-axis to use
// when the device doesn't have local pins.
static const DefaultNodeMap s_defaultNodes[] = {
    // Motors: stepperid 0=A, 1=X, 2=Y, 3=Z
    { RouteEntry::MOTOR, 0, 0, 0 },  // nodeId filled from pinConfig at runtime
    { RouteEntry::MOTOR, 1, 0, 0 },
    { RouteEntry::MOTOR, 2, 0, 0 },
    { RouteEntry::MOTOR, 3, 0, 0 },
    // Lasers: channels 0-3
    { RouteEntry::LASER, 0, 0, 0 },
    { RouteEntry::LASER, 1, 0, 0 },
    { RouteEntry::LASER, 2, 0, 0 },
    { RouteEntry::LASER, 3, 0, 0 },
    // LED: single logical LED array
    { RouteEntry::LED, 0, 0, 0 },
    // Galvo
    { RouteEntry::GALVO, 0, 0, 0 },
    // Home: axes 0-3 (same mapping as motor)
    { RouteEntry::HOME, 0, 0, 0 },
    { RouteEntry::HOME, 1, 0, 0 },
    { RouteEntry::HOME, 2, 0, 0 },
    { RouteEntry::HOME, 3, 0, 0 },
    // TMC: axes 0-3
    { RouteEntry::TMC, 0, 0, 0 },
    { RouteEntry::TMC, 1, 0, 0 },
    { RouteEntry::TMC, 2, 0, 0 },
    { RouteEntry::TMC, 3, 0, 0 },
};

// Helper: resolve default CAN node-id from pinConfig for a given type+logicalId
static uint8_t resolveDefaultNodeId(RouteEntry::Type type, uint8_t logicalId) {
    switch (type) {
        case RouteEntry::MOTOR:
        case RouteEntry::HOME:
        case RouteEntry::TMC:
            switch (logicalId) {
                case 0: return pinConfig.CAN_ID_MOT_A;
                case 1: return pinConfig.CAN_ID_MOT_X;
                case 2: return pinConfig.CAN_ID_MOT_Y;
                case 3: return pinConfig.CAN_ID_MOT_Z;
                default: return pinConfig.CAN_ID_MOT_X;
            }
        case RouteEntry::LASER:
            switch (logicalId) {
                case 0: return pinConfig.CAN_ID_LASER_0;
                case 1: return pinConfig.CAN_ID_LASER_1;
                case 2: return pinConfig.CAN_ID_LASER_2;
                case 3: return pinConfig.CAN_ID_LASER_3;
                default: return pinConfig.CAN_ID_LASER_0;
            }
        case RouteEntry::LED:
            return pinConfig.CAN_ID_LED_0;
        case RouteEntry::GALVO:
            return pinConfig.CAN_ID_GALVO_0;
        default:
            return 10;
    }
}

// Helper: check if a motor axis has local pins configured
static bool hasLocalMotorPin(uint8_t logicalId) {
    switch (logicalId) {
        case 0: return pinConfig.MOTOR_A_STEP >= 0;
        case 1: return pinConfig.MOTOR_X_STEP >= 0;
        case 2: return pinConfig.MOTOR_Y_STEP >= 0;
        case 3: return pinConfig.MOTOR_Z_STEP >= 0;
        default: return false;
    }
}

// Helper: check if a laser channel has local pins configured
static bool hasLocalLaserPin(uint8_t logicalId) {
    switch (logicalId) {
        case 0: return pinConfig.LASER_0 >= 0;
        case 1: return pinConfig.LASER_1 >= 0;
        case 2: return pinConfig.LASER_2 >= 0;
        case 3: return pinConfig.LASER_3 >= 0;
        case 4: return pinConfig.LASER_4 >= 0;
        default: return false;
    }
}

// Helper: check if LED has local pins configured
static bool hasLocalLedPin() {
    return pinConfig.LED_PIN >= 0 && pinConfig.LED_COUNT > 0;
}

// Helper: check if galvo has local pins configured
static bool hasLocalGalvoPin() {
    return pinConfig.galvo_cs != 0xFF && pinConfig.galvo_cs != 0;
}

// ============================================================================
// buildDefault() — construct the routing table from pinConfig + runtimeConfig
//
// Priority: explicit ROUTE_* override in PinConfig (-1 = infer from canRole + pins)
// ============================================================================

// Resolve a single device route: use PinConfig override if >= 0, else infer.
static void resolveRoute(RouteEntry::Type type, uint8_t logicalId,
                         int8_t override_val, bool hasLocalPin) {
    if (override_val >= 0) {
        // Explicit override from PinConfig
        RouteEntry::Where w = static_cast<RouteEntry::Where>(override_val);
        uint8_t nid = (w == RouteEntry::REMOTE) ? resolveDefaultNodeId(type, logicalId) : 0;
        log_i("Applying explicit route override: type=%s id=%d -> %s (node 0x%02X)",
              type, logicalId, w, nid);
        RoutingTable::set(type, logicalId, w, nid, 0);
        return;
    }

    // Infer from canRole + pins (legacy behaviour)
    switch (runtimeConfig.canRole) {
        log_i("Inferring route for type=%s id=%d: canRole=%d hasLocalPin=%d",
              type, logicalId, runtimeConfig.canRole, hasLocalPin);
    case NodeRole::STANDALONE:
    case NodeRole::CAN_SLAVE:
        RoutingTable::set(type, logicalId,
            hasLocalPin ? RouteEntry::LOCAL : RouteEntry::OFF);
        break;
    case NodeRole::CAN_MASTER:
        if (hasLocalPin) {
            RoutingTable::set(type, logicalId, RouteEntry::LOCAL);
        } else {
            uint8_t nid = resolveDefaultNodeId(type, logicalId);
            RoutingTable::set(type, logicalId, RouteEntry::REMOTE, nid, 0);
        }
        break;
    }
}

void RoutingTable::buildDefault() {
    count = 0;

    // Motors, Home, TMC — per axis
    for (uint8_t ax = 0; ax < 4; ax++) {
        bool hasPin = hasLocalMotorPin(ax);
        resolveRoute(RouteEntry::MOTOR, ax, pinConfig.ROUTE_MOTOR[ax], hasPin);
        // Home/TMC follow motor override unless they have their own
        int8_t homeOv = (pinConfig.ROUTE_HOME[ax] >= 0)
                        ? pinConfig.ROUTE_HOME[ax] : pinConfig.ROUTE_MOTOR[ax];
        int8_t tmcOv  = (pinConfig.ROUTE_TMC[ax] >= 0)
                        ? pinConfig.ROUTE_TMC[ax]  : pinConfig.ROUTE_MOTOR[ax];
        resolveRoute(RouteEntry::HOME, ax, homeOv, hasPin);
        resolveRoute(RouteEntry::TMC,  ax, tmcOv,  hasPin);
    }

    // Lasers
    for (uint8_t ch = 0; ch < 4; ch++) {
        resolveRoute(RouteEntry::LASER, ch,
                     pinConfig.ROUTE_LASER[ch], hasLocalLaserPin(ch));
    }

    // LED
    resolveRoute(RouteEntry::LED, 0, pinConfig.ROUTE_LED, hasLocalLedPin());

    // Galvo
    resolveRoute(RouteEntry::GALVO, 0, pinConfig.ROUTE_GALVO, hasLocalGalvoPin());
}

// ============================================================================
// applyNvsOverrides() — placeholder for future NVS-stored route overrides
// ============================================================================
void RoutingTable::applyNvsOverrides() {
    // TODO: Read NVS key "routes" and apply overrides.
    // For now, buildDefault() is the only source of truth.
}

// ============================================================================
// find() — lookup a route by type + logicalId
// ============================================================================
const RouteEntry* RoutingTable::find(RouteEntry::Type t, uint8_t logicalId) {
    for (uint8_t i = 0; i < count; i++) {
        if (table[i].type == t && table[i].logicalId == logicalId)
            return &table[i];
    }
    return nullptr;
}

// Forward declarations for logging helpers
static const char* typeToStr(RouteEntry::Type t);
static const char* whereToStr(RouteEntry::Where w);

// ============================================================================
// set() — insert or update a route entry
// ============================================================================
void RoutingTable::set(RouteEntry::Type t, uint8_t logicalId,
                       RouteEntry::Where w, uint8_t nodeId, uint8_t subAxis) {
    // Try to update existing entry
    for (uint8_t i = 0; i < count; i++) {
        if (table[i].type == t && table[i].logicalId == logicalId) {
            log_i("Updating route: type=%s id=%d -> %s (node 0x%02X axis %d)",
                  typeToStr(t), logicalId, whereToStr(w), nodeId, subAxis);
            table[i].where   = w;
            table[i].nodeId  = nodeId;
            table[i].subAxis = subAxis;
            return;
        }
    }
    // Insert new entry
    if (count < MAX_ROUTES) {
        table[count] = { t, logicalId, w, nodeId, subAxis };
        count++;
    } else {
        ESP_LOGE(TAG, "Routing table full (%d entries)", MAX_ROUTES);
    }
}

// ============================================================================
// toJson() — dump entire table as JSON array
// ============================================================================
static const char* typeToStr(RouteEntry::Type t) {
    switch (t) {
        case RouteEntry::MOTOR: return "motor";
        case RouteEntry::LASER: return "laser";
        case RouteEntry::LED:   return "led";
        case RouteEntry::GALVO: return "galvo";
        case RouteEntry::HOME:  return "home";
        case RouteEntry::TMC:   return "tmc";
        case RouteEntry::DAC:   return "dac";
        case RouteEntry::AIN:   return "ain";
        case RouteEntry::DIN:   return "din";
        default:                return "?";
    }
}

static const char* whereToStr(RouteEntry::Where w) {
    switch (w) {
        case RouteEntry::LOCAL:    return "local";
        case RouteEntry::REMOTE:   return "remote";
        case RouteEntry::OFF: return "disabled";
        default:                   return "?";
    }
}

cJSON* RoutingTable::toJson() {
    cJSON* arr = cJSON_CreateArray();
    for (uint8_t i = 0; i < count; i++) {
        cJSON* obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "type",  typeToStr(table[i].type));
        cJSON_AddNumberToObject(obj, "id",    table[i].logicalId);
        cJSON_AddStringToObject(obj, "where", whereToStr(table[i].where));
        if (table[i].where == RouteEntry::REMOTE) {
            cJSON_AddNumberToObject(obj, "nodeId",  table[i].nodeId);
            cJSON_AddNumberToObject(obj, "subAxis", table[i].subAxis);
        }
        cJSON_AddItemToArray(arr, obj);
    }
    return arr;
}

// ============================================================================
// logAll() — boot diagnostic dump
// ============================================================================
void RoutingTable::logAll() {
    ESP_LOGI(TAG, "Routing table (%d entries):", count);
    for (uint8_t i = 0; i < count; i++) {
        const RouteEntry& e = table[i];
        if (e.where == RouteEntry::REMOTE) {
            ESP_LOGI(TAG, "  %-6s %d -> REMOTE node %d axis %d",
                     typeToStr(e.type), e.logicalId, e.nodeId, e.subAxis);
        } else {
            ESP_LOGI(TAG, "  %-6s %d -> %s",
                     typeToStr(e.type), e.logicalId, whereToStr(e.where));
        }
    }
}

} // namespace UC2

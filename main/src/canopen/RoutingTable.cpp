// RoutingTable.cpp — Data-driven routing table, built at boot.
// See RoutingTable.h for the design rationale.
#include "RoutingTable.h"

#ifdef CAN_CONTROLLER_CANOPEN

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
// ============================================================================
void RoutingTable::buildDefault() {
    count = 0;

    switch (runtimeConfig.canRole) {
    case NodeRole::STANDALONE:
        // Everything LOCAL if pins exist, OFF otherwise
        for (uint8_t ax = 0; ax < 4; ax++) {
            set(RouteEntry::MOTOR, ax,
                hasLocalMotorPin(ax) ? RouteEntry::LOCAL : RouteEntry::OFF);
            set(RouteEntry::HOME, ax,
                hasLocalMotorPin(ax) ? RouteEntry::LOCAL : RouteEntry::OFF);
            set(RouteEntry::TMC, ax,
                hasLocalMotorPin(ax) ? RouteEntry::LOCAL : RouteEntry::OFF);
        }
        for (uint8_t ch = 0; ch < 4; ch++) {
            set(RouteEntry::LASER, ch,
                hasLocalLaserPin(ch) ? RouteEntry::LOCAL : RouteEntry::OFF);
        }
        set(RouteEntry::LED, 0,
            hasLocalLedPin() ? RouteEntry::LOCAL : RouteEntry::OFF);
        set(RouteEntry::GALVO, 0,
            hasLocalGalvoPin() ? RouteEntry::LOCAL : RouteEntry::OFF);
        break;

    case NodeRole::CAN_SLAVE:
        // Slave: everything LOCAL on this node, no REMOTEs
        for (uint8_t ax = 0; ax < 4; ax++) {
            set(RouteEntry::MOTOR, ax,
                hasLocalMotorPin(ax) ? RouteEntry::LOCAL : RouteEntry::OFF);
            set(RouteEntry::HOME, ax,
                hasLocalMotorPin(ax) ? RouteEntry::LOCAL : RouteEntry::OFF);
            set(RouteEntry::TMC, ax,
                hasLocalMotorPin(ax) ? RouteEntry::LOCAL : RouteEntry::OFF);
        }
        for (uint8_t ch = 0; ch < 4; ch++) {
            set(RouteEntry::LASER, ch,
                hasLocalLaserPin(ch) ? RouteEntry::LOCAL : RouteEntry::OFF);
        }
        set(RouteEntry::LED, 0,
            hasLocalLedPin() ? RouteEntry::LOCAL : RouteEntry::OFF);
        set(RouteEntry::GALVO, 0,
            hasLocalGalvoPin() ? RouteEntry::LOCAL : RouteEntry::OFF);
        break;

    case NodeRole::CAN_MASTER:
        // Master: LOCAL if pinConfig has the pins, otherwise REMOTE
        for (uint8_t ax = 0; ax < 4; ax++) {
            if (hasLocalMotorPin(ax)) {
                set(RouteEntry::MOTOR, ax, RouteEntry::LOCAL);
                set(RouteEntry::HOME,  ax, RouteEntry::LOCAL);
                set(RouteEntry::TMC,   ax, RouteEntry::LOCAL);
            } else {
                uint8_t nid = resolveDefaultNodeId(RouteEntry::MOTOR, ax);
                set(RouteEntry::MOTOR, ax, RouteEntry::REMOTE, nid, 0);
                set(RouteEntry::HOME,  ax, RouteEntry::REMOTE, nid, 0);
                set(RouteEntry::TMC,   ax, RouteEntry::REMOTE, nid, 0);
            }
        }
        for (uint8_t ch = 0; ch < 4; ch++) {
            if (hasLocalLaserPin(ch)) {
                set(RouteEntry::LASER, ch, RouteEntry::LOCAL);
            } else {
                uint8_t nid = resolveDefaultNodeId(RouteEntry::LASER, ch);
                set(RouteEntry::LASER, ch, RouteEntry::REMOTE, nid, 0);
            }
        }
        if (hasLocalLedPin()) {
            set(RouteEntry::LED, 0, RouteEntry::LOCAL);
        } else {
            set(RouteEntry::LED, 0, RouteEntry::REMOTE,
                resolveDefaultNodeId(RouteEntry::LED, 0), 0);
        }
        if (hasLocalGalvoPin()) {
            set(RouteEntry::GALVO, 0, RouteEntry::LOCAL);
        } else {
            set(RouteEntry::GALVO, 0, RouteEntry::REMOTE,
                resolveDefaultNodeId(RouteEntry::GALVO, 0), 0);
        }
        break;
    }
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

// ============================================================================
// set() — insert or update a route entry
// ============================================================================
void RoutingTable::set(RouteEntry::Type t, uint8_t logicalId,
                       RouteEntry::Where w, uint8_t nodeId, uint8_t subAxis) {
    // Try to update existing entry
    for (uint8_t i = 0; i < count; i++) {
        if (table[i].type == t && table[i].logicalId == logicalId) {
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

#endif // CAN_CONTROLLER_CANOPEN

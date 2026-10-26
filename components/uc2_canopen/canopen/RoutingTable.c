// RoutingTable.c — Data-driven routing table, built at boot.
// See RoutingTable.h for the design rationale.
//
// T4.4: C-ified from RoutingTable.cpp (behavior-identical). The historical
// UC2::RoutingTable / UC2::RouteEntry C++ API is preserved via the shim in
// RoutingTable.h; the C free functions below are the real implementation.
#include "RoutingTable.h"
#include <stdbool.h>
#include <PinConfig.h>

#include "RuntimeConfig.h"
#include "esp_log.h"

static const char* TAG = "routing";

// Static storage
static route_entry s_table[ROUTING_TABLE_MAX_ROUTES];
static uint8_t    s_count = 0;

// NOTE (T4.4 C port): the original RoutingTable.cpp declared a
// `static const DefaultNodeMap s_defaultNodes[]` lookup table here, but it was
// never referenced (dead code). Dropped to avoid a -Wunused-const-variable
// warning under the device GCC build; resolveDefaultNodeId() resolves node ids
// straight from pinConfig.

// Helper: resolve default CAN node-id from pinConfig for a given type+logicalId
static uint8_t resolveDefaultNodeId(route_entry_type_t type, uint8_t logicalId) {
    switch (type) {
        case ROUTE_ENTRY_TYPE_MOTOR:
        case ROUTE_ENTRY_TYPE_HOME:
        case ROUTE_ENTRY_TYPE_TMC:
            switch (logicalId) {
                case 0: return pinConfig.CAN_ID_MOT_A;
                case 1: return pinConfig.CAN_ID_MOT_X;
                case 2: return pinConfig.CAN_ID_MOT_Y;
                case 3: return pinConfig.CAN_ID_MOT_Z;
                default: return pinConfig.CAN_ID_MOT_X;
            }
        case ROUTE_ENTRY_TYPE_LASER:
            // Use the per-channel node ID array — allows multiple channels on the same node.
            if (logicalId < 4) return pinConfig.CAN_NODE_LASER[logicalId];
            return pinConfig.CAN_NODE_LASER[0];
        case ROUTE_ENTRY_TYPE_LED:
            return pinConfig.CAN_ID_LED_0;
        case ROUTE_ENTRY_TYPE_GALVO:
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
static bool hasLocalLedPin(void) {
    return pinConfig.LED_PIN >= 0 && pinConfig.LED_COUNT > 0;
}

// Helper: check if galvo has local pins configured
static bool hasLocalGalvoPin(void) {
    return pinConfig.galvo_cs != 0xFF && pinConfig.galvo_cs != 0;
}

// ============================================================================
// resolveRoute() — construct one row: use PinConfig override if >= 0, else
// infer from canRole + pins.
//
// (build_default() priority: explicit ROUTE_* override in PinConfig, where
// -1 = infer from canRole + pins.)
// ============================================================================
static void resolveRoute(route_entry_type_t type, uint8_t logicalId,
                         int8_t override_val, bool hasLocalPin) {
    if (override_val >= 0) {
        // Explicit override from PinConfig
        route_entry_where_t w = (route_entry_where_t)override_val;
        uint8_t nid = (w == ROUTE_ENTRY_WHERE_REMOTE) ? resolveDefaultNodeId(type, logicalId) : 0;
        log_i("Applying explicit route override: type=%d id=%d -> %d (node 0x%02X)",
              (int)type, (int)logicalId, (int)w, (int)nid);
        routing_table_set(type, logicalId, w, nid, logicalId);
        return;
    }

    // Infer from canRole + pins (legacy behaviour)
    log_i("Inferring route for type=%d id=%d: canRole=%d hasLocalPin=%d",
          (int)type, (int)logicalId, (int)runtimeConfig.canRole, (int)hasLocalPin);
    switch (runtimeConfig.canRole) {
    case NODE_ROLE_STANDALONE:
    case NODE_ROLE_CAN_SLAVE:
        routing_table_set(type, logicalId,
            hasLocalPin ? ROUTE_ENTRY_WHERE_LOCAL : ROUTE_ENTRY_WHERE_OFF, 0, 0);
        break;
    case NODE_ROLE_CAN_MASTER:
        if (hasLocalPin) {
            routing_table_set(type, logicalId, ROUTE_ENTRY_WHERE_LOCAL, 0, 0);
        } else {
            uint8_t nid = resolveDefaultNodeId(type, logicalId);
            routing_table_set(type, logicalId, ROUTE_ENTRY_WHERE_REMOTE, nid, logicalId);
        }
        break;
    }
}

// ============================================================================
// routing_table_build_default() — construct the routing table from
// pinConfig + runtimeConfig
// ============================================================================
void routing_table_build_default(void) {
    s_count = 0;

    // Motors, Home, TMC — per axis
    for (uint8_t ax = 0; ax < 4; ax++) {
        bool hasPin = hasLocalMotorPin(ax);
        resolveRoute(ROUTE_ENTRY_TYPE_MOTOR, ax, pinConfig.ROUTE_MOTOR[ax], hasPin);
        // Home/TMC follow motor override unless they have their own
        int8_t homeOv = (pinConfig.ROUTE_HOME[ax] >= 0)
                        ? pinConfig.ROUTE_HOME[ax] : pinConfig.ROUTE_MOTOR[ax];
        int8_t tmcOv  = (pinConfig.ROUTE_TMC[ax] >= 0)
                        ? pinConfig.ROUTE_TMC[ax]  : pinConfig.ROUTE_MOTOR[ax];
        resolveRoute(ROUTE_ENTRY_TYPE_HOME, ax, homeOv, hasPin);
        resolveRoute(ROUTE_ENTRY_TYPE_TMC,  ax, tmcOv,  hasPin);
    }

    // Lasers — node ID and sub-axis can be configured independently per channel
    // via PinConfig.CAN_NODE_LASER[ch] and PinConfig.CAN_SUBAXIS_LASER[ch].
    // This allows multiple logical channels to share one CAN node (e.g. all
    // four channels on node 0x14 with sub-axes 0-3) or to be spread across
    // multiple nodes (e.g. two nodes each with two channels).
    for (uint8_t ch = 0; ch < 4; ch++) {
        int8_t override_val = pinConfig.ROUTE_LASER[ch];
        bool   hasPin       = hasLocalLaserPin(ch);
        uint8_t nid         = pinConfig.CAN_NODE_LASER[ch];
        uint8_t sub         = (uint8_t)pinConfig.CAN_SUBAXIS_LASER[ch];

        if (override_val >= 0) {
            // Explicit route override from PinConfig
            route_entry_where_t w = (route_entry_where_t)override_val;
            uint8_t resolvedNid = (w == ROUTE_ENTRY_WHERE_REMOTE) ? nid : 0;
            log_i("Laser ch%d: explicit route=%d node=0x%02X subAxis=%d",
                  ch, (int)w, resolvedNid, sub);
            routing_table_set(ROUTE_ENTRY_TYPE_LASER, ch, w, resolvedNid, sub);
        } else {
            // Infer from canRole + local pin availability
            switch (runtimeConfig.canRole) {
            case NODE_ROLE_STANDALONE:
            case NODE_ROLE_CAN_SLAVE:
                routing_table_set(ROUTE_ENTRY_TYPE_LASER, ch,
                    hasPin ? ROUTE_ENTRY_WHERE_LOCAL : ROUTE_ENTRY_WHERE_OFF, 0, sub);
                break;
            case NODE_ROLE_CAN_MASTER:
                if (hasPin) {
                    routing_table_set(ROUTE_ENTRY_TYPE_LASER, ch, ROUTE_ENTRY_WHERE_LOCAL, 0, sub);
                } else {
                    log_i("Laser ch%d: inferred REMOTE node=0x%02X subAxis=%d", ch, nid, sub);
                    routing_table_set(ROUTE_ENTRY_TYPE_LASER, ch, ROUTE_ENTRY_WHERE_REMOTE, nid, sub);
                }
                break;
            }
        }
    }

    // Laser channel 4 — single extra channel for an illumination/satellite
    // board that exposes one PWM. Routing fields live in standalone members
    // (ROUTE_LASER_4 / CAN_NODE_LASER_4 / CAN_SUBAXIS_LASER_4) so that
    // existing PinConfigs declaring `int8_t ROUTE_LASER[4]` keep compiling.
    {
        uint8_t ch        = 4;
        int8_t  ov4       = pinConfig.ROUTE_LASER_4;
        bool    hasPin4   = hasLocalLaserPin(4);
        uint8_t nid4      = pinConfig.CAN_NODE_LASER_4;
        uint8_t sub4      = (uint8_t)pinConfig.CAN_SUBAXIS_LASER_4;

        if (ov4 >= 0) {
            route_entry_where_t w = (route_entry_where_t)ov4;
            uint8_t resolvedNid = (w == ROUTE_ENTRY_WHERE_REMOTE) ? nid4 : 0;
            log_i("Laser ch%d: explicit route=%d node=0x%02X subAxis=%d",
                  ch, (int)w, resolvedNid, sub4);
            routing_table_set(ROUTE_ENTRY_TYPE_LASER, ch, w, resolvedNid, sub4);
        } else {
            switch (runtimeConfig.canRole) {
            case NODE_ROLE_STANDALONE:
            case NODE_ROLE_CAN_SLAVE:
                routing_table_set(ROUTE_ENTRY_TYPE_LASER, ch,
                    hasPin4 ? ROUTE_ENTRY_WHERE_LOCAL : ROUTE_ENTRY_WHERE_OFF, 0, sub4);
                break;
            case NODE_ROLE_CAN_MASTER:
                if (hasPin4) {
                    routing_table_set(ROUTE_ENTRY_TYPE_LASER, ch, ROUTE_ENTRY_WHERE_LOCAL, 0, sub4);
                } else {
                    log_i("Laser ch%d: inferred REMOTE node=0x%02X subAxis=%d",
                          ch, nid4, sub4);
                    routing_table_set(ROUTE_ENTRY_TYPE_LASER, ch, ROUTE_ENTRY_WHERE_REMOTE, nid4, sub4);
                }
                break;
            }
        }
    }

    // LED
    resolveRoute(ROUTE_ENTRY_TYPE_LED, 0, pinConfig.ROUTE_LED, hasLocalLedPin());

    // Galvo
    resolveRoute(ROUTE_ENTRY_TYPE_GALVO, 0, pinConfig.ROUTE_GALVO, hasLocalGalvoPin());
}

// ============================================================================
// routing_table_apply_nvs_overrides() — placeholder for future NVS-stored
// route overrides
// ============================================================================
void routing_table_apply_nvs_overrides(void) {
    // TODO: Read NVS key "routes" and apply overrides.
    // For now, buildDefault() is the only source of truth.
}

// ============================================================================
// routing_table_find() — lookup a route by type + logicalId
// ============================================================================
const route_entry *routing_table_find(route_entry_type_t t, uint8_t logicalId) {
    for (uint8_t i = 0; i < s_count; i++) {
        if (s_table[i].type == t && s_table[i].logicalId == logicalId)
            return &s_table[i];
    }
    return NULL;
}

// Forward declarations for logging helpers
static const char* typeToStr(route_entry_type_t t);
static const char* whereToStr(route_entry_where_t w);

// ============================================================================
// routing_table_set() — insert or update a route entry
// ============================================================================
void routing_table_set(route_entry_type_t t, uint8_t logicalId,
                       route_entry_where_t w, uint8_t nodeId, uint8_t subAxis) {
    // Try to update existing entry
    for (uint8_t i = 0; i < s_count; i++) {
        if (s_table[i].type == t && s_table[i].logicalId == logicalId) {
            log_i("Updating route: type=%d id=%d -> %d (node 0x%02X axis %d)",
                  (int)t, (int)logicalId, (int)w, (int)nodeId, (int)subAxis);
            s_table[i].where   = w;
            s_table[i].nodeId  = nodeId;
            s_table[i].subAxis = subAxis;
            return;
        }
    }
    // Insert new entry
    if (s_count < ROUTING_TABLE_MAX_ROUTES) {
        s_table[s_count].type      = (uint8_t)t;
        s_table[s_count].logicalId = logicalId;
        s_table[s_count].where     = (uint8_t)w;
        s_table[s_count].nodeId    = nodeId;
        s_table[s_count].subAxis   = subAxis;
        s_count++;
    } else {
        log_e("RoutingTable overflow: cannot add type=%d id=%d", (int)t, (int)logicalId);
    }
}

// ============================================================================
// routing_table_to_json() — dump entire table as JSON array
// ============================================================================
static const char* typeToStr(route_entry_type_t t) {
    switch (t) {
        case ROUTE_ENTRY_TYPE_MOTOR: return "motor";
        case ROUTE_ENTRY_TYPE_LASER: return "laser";
        case ROUTE_ENTRY_TYPE_LED:   return "led";
        case ROUTE_ENTRY_TYPE_GALVO: return "galvo";
        case ROUTE_ENTRY_TYPE_HOME:  return "home";
        case ROUTE_ENTRY_TYPE_TMC:   return "tmc";
        case ROUTE_ENTRY_TYPE_DAC:   return "dac";
        case ROUTE_ENTRY_TYPE_AIN:   return "ain";
        case ROUTE_ENTRY_TYPE_DIN:   return "din";
        default:                     return "?";
    }
}

static const char* whereToStr(route_entry_where_t w) {
    switch (w) {
        case ROUTE_ENTRY_WHERE_LOCAL:  return "local";
        case ROUTE_ENTRY_WHERE_REMOTE: return "remote";
        case ROUTE_ENTRY_WHERE_OFF:    return "disabled";
        default:                       return "?";
    }
}

cJSON* routing_table_to_json(void) {
    cJSON* arr = cJSON_CreateArray();
    for (uint8_t i = 0; i < s_count; i++) {
        cJSON* obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "type",  typeToStr(s_table[i].type));
        cJSON_AddNumberToObject(obj, "id",    s_table[i].logicalId);
        cJSON_AddStringToObject(obj, "where", whereToStr(s_table[i].where));
        if (s_table[i].where == ROUTE_ENTRY_WHERE_REMOTE) {
            cJSON_AddNumberToObject(obj, "nodeId",  s_table[i].nodeId);
            cJSON_AddNumberToObject(obj, "subAxis", s_table[i].subAxis);
        }
        cJSON_AddItemToArray(arr, obj);
    }
    return arr;
}

// ============================================================================
// routing_table_log_all() — boot diagnostic dump
// ============================================================================
void routing_table_log_all(void) {
    log_i("Routing table dump (%d entries):", s_count);
    for (uint8_t i = 0; i < s_count; i++) {
        const route_entry *e = &s_table[i];
        if (e->where == ROUTE_ENTRY_WHERE_REMOTE) {
            log_i("  %-6s %d -> %s (node 0x%02X subAxis %d)",
                     typeToStr(e->type), e->logicalId, whereToStr(e->where),
                     e->nodeId, e->subAxis);
        } else {
            log_i("  %-6s %d -> %s",
                     typeToStr(e->type), e->logicalId, whereToStr(e->where));
        }
    }
}

// ============================================================================
// Inspection accessors
// ============================================================================
uint8_t routing_table_get_count(void) {
    return s_count;
}

uint8_t routing_table_get_max_routes(void) {
    return ROUTING_TABLE_MAX_ROUTES;
}

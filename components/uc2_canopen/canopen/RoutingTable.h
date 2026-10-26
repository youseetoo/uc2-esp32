// RoutingTable.h — Data-driven routing for UC2 command dispatch.
// Built at boot from pinConfig + runtimeConfig. Consulted by DeviceRouter
// to decide if a command runs locally or gets forwarded via CANopen SDO.
//
// T4.4: C-ified. The C types + C free functions below are the REAL
// implementation (RoutingTable.c). The historical UC2::RoutingTable /
// UC2::RouteEntry C++ API is preserved for existing callers by the thin shim
// at the bottom of this header.
#pragma once

#include <stdint.h>
#include "cJSON.h"

// ============================================================================
// C types (primary)
// ============================================================================

// One row per addressable hardware instance: a motor axis, a laser channel,
// an LED matrix, a galvo, an endstop bank, etc. Built at boot from pinConfig
// and runtimeConfig.canRole. Mutable at runtime via /route_set for field
// service. Named `route_entry` (not `RouteEntry`) to avoid colliding with the
// C++ shim struct UC2::RouteEntry in C++ name lookup.
struct route_entry {
    uint8_t type;          // what kind of hardware (route_entry_type_t)
    uint8_t logicalId;     // user-visible id (stepperid, LASERid, ledid, ...)
    uint8_t where;         // local GPIO or remote CAN node (route_entry_where_t)
    uint8_t nodeId;        // valid only when where == REMOTE
    uint8_t subAxis;       // valid only when where == REMOTE; which axis on that node
};
typedef struct route_entry route_entry;

typedef enum {
    ROUTE_ENTRY_TYPE_MOTOR = 1,
    ROUTE_ENTRY_TYPE_LASER,
    ROUTE_ENTRY_TYPE_LED,
    ROUTE_ENTRY_TYPE_GALVO,
    ROUTE_ENTRY_TYPE_HOME,
    ROUTE_ENTRY_TYPE_TMC,
    ROUTE_ENTRY_TYPE_DAC,
    ROUTE_ENTRY_TYPE_AIN,
    ROUTE_ENTRY_TYPE_DIN,
} route_entry_type_t;

typedef enum {
    ROUTE_ENTRY_WHERE_LOCAL = 0,
    ROUTE_ENTRY_WHERE_REMOTE,
    ROUTE_ENTRY_WHERE_OFF,
} route_entry_where_t;

// ~64 entries cover the largest envisaged setup.
#define ROUTING_TABLE_MAX_ROUTES 64

// ============================================================================
// C API (real implementation — RoutingTable.c). No default arguments: this
// block must compile as plain C. Callers that want the historical "omit
// nodeId/subAxis" convenience use the C++ shim below.
// ============================================================================
#ifdef __cplusplus
extern "C" {
#endif

// Build from pinConfig + runtimeConfig at boot
void   routing_table_build_default(void);
// Apply NVS overrides loaded by NVSConfig
void   routing_table_apply_nvs_overrides(void);
// Lookup: returns NULL if no route exists
const route_entry *routing_table_find(route_entry_type_t type, uint8_t logical_id);
// Mutation (used by /route_set and during boot construction)
void   routing_table_set(route_entry_type_t type, uint8_t logical_id,
                         route_entry_where_t where, uint8_t node_id, uint8_t sub_axis);
// /route_get — serialize the whole table (caller frees with cJSON_Delete)
cJSON *routing_table_to_json(void);
// dumps to ESP_LOG for boot diagnostics
void   routing_table_log_all(void);
// Inspection — used by unit tests
uint8_t routing_table_get_count(void);
uint8_t routing_table_get_max_routes(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
// ============================================================================
// C++ shim — preserves the historical UC2::RoutingTable / UC2::RouteEntry API.
//
// UC2::RouteEntry is layout-compatible with the C struct (five 1-byte fields,
// no padding), so find() reinterpret_casts between them. The nested Type/Where
// enums keep the `RouteEntry::MOTOR` / `e.where == RouteEntry::LOCAL` style
// call sites compiling unchanged.
// ============================================================================
namespace UC2 {

struct RouteEntry {
    enum Type : uint8_t { MOTOR = 1, LASER, LED, GALVO, HOME, TMC, DAC, AIN, DIN };
    enum Where : uint8_t { LOCAL = 0, REMOTE = 1, OFF = 2 };

    Type    type;
    uint8_t logicalId;
    Where   where;
    uint8_t nodeId;
    uint8_t subAxis;
};

class RoutingTable {
public:
    static void set(RouteEntry::Type type, uint8_t logicalId, RouteEntry::Where where,
                    uint8_t nodeId = 0, uint8_t subAxis = 0) {
        routing_table_set((route_entry_type_t)type, logicalId,
                          (route_entry_where_t)where, nodeId, subAxis);
    }
    static void buildDefault() { routing_table_build_default(); }
    static void applyNvsOverrides() { routing_table_apply_nvs_overrides(); }
    static const RouteEntry *find(RouteEntry::Type type, uint8_t logicalId) {
        return reinterpret_cast<const UC2::RouteEntry *>(
            routing_table_find((route_entry_type_t)type, logicalId));
    }
    static cJSON *toJson() { return routing_table_to_json(); }
    static void logAll() { routing_table_log_all(); }
    static uint8_t getCount() { return routing_table_get_count(); }
    static uint8_t getMaxRoutes() { return routing_table_get_max_routes(); }
};

} // namespace UC2
#endif // __cplusplus

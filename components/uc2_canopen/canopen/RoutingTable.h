// RoutingTable.h — Data-driven routing for UC2 command dispatch.
// Built at boot from pinConfig + runtimeConfig. Consulted by DeviceRouter
// to decide if a command runs locally or gets forwarded via CANopen SDO.
#pragma once

#include <stdint.h>
#include "cJSON.h"

#ifdef __cplusplus
namespace UC2 {

// One row per addressable hardware instance: a motor axis, a laser channel,
// an LED matrix, a galvo, an endstop bank, etc. Built at boot from pinConfig
// and runtimeConfig.canRole. Mutable at runtime via /route_set for field
// service. ~64 entries cover the largest envisaged setup.
struct RouteEntry {
    enum Type : uint8_t { MOTOR = 1, LASER, LED, GALVO, HOME, TMC, DAC, AIN, DIN };
    enum Where : uint8_t { LOCAL = 0, REMOTE = 1, OFF = 2 };

    Type    type;          // what kind of hardware
    uint8_t logicalId;     // user-visible id (stepperid, LASERid, ledid, ...)
    Where   where;         // local GPIO or remote CAN node
    uint8_t nodeId;        // valid only when where == REMOTE
    uint8_t subAxis;       // valid only when where == REMOTE; which axis on that node
};

class RoutingTable {
public:
    // Build from pinConfig + runtimeConfig at boot
    static void buildDefault();

    // Apply NVS overrides loaded by NVSConfig
    static void applyNvsOverrides();

    // Lookup: returns nullptr if no route exists
    static const RouteEntry* find(RouteEntry::Type t, uint8_t logicalId);

    // Mutation (used by /route_set and during boot construction)
    static void set(RouteEntry::Type t, uint8_t logicalId,
                    RouteEntry::Where w, uint8_t nodeId = 0, uint8_t subAxis = 0);

    // Inspection — used by /route_get and the boot log
    static cJSON* toJson();
    static void   logAll();   // dumps to ESP_LOG for boot diagnostics

private:
    static constexpr uint8_t MAX_ROUTES = 64;
    static RouteEntry table[MAX_ROUTES];
    static uint8_t    count;
    
public:
    // Expose for unit tests
    static uint8_t getCount() { return count; }
    static constexpr uint8_t getMaxRoutes() { return MAX_ROUTES; }
};

} // namespace UC2
#endif // __cplusplus

// T4.2: C shims for the pure-C SerialProcessIn.c (/route_get + /route_set).
// The C enums mirror RouteEntry::Type / RouteEntry::Where (same values, so the
// .cpp wrappers can cast directly).
#ifdef __cplusplus
extern "C" {
#endif

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

// /route_get — serialize the whole table (caller frees with cJSON_Delete).
cJSON *routing_table_to_json(void);
// /route_set — update one entry (type + logicalId select the row).
void routing_table_set(route_entry_type_t type, uint8_t logicalId,
                       route_entry_where_t where, uint8_t nodeId, uint8_t subAxis);

#ifdef __cplusplus
}
#endif

// RoutingTable.h — Data-driven routing for UC2 command dispatch.
// Built at boot from pinConfig + runtimeConfig. Consulted by DeviceRouter
// to decide if a command runs locally or gets forwarded via CANopen SDO.
#pragma once

#ifdef CAN_CONTROLLER_CANOPEN

#include <stdint.h>
#include "cJSON.h"

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
};

} // namespace UC2

#endif // CAN_CONTROLLER_CANOPEN

/**
 * DeviceRouter.h
 *
 * UC2 Device Router — transparently routes device commands to either:
 *   a) Local GPIO controllers (FocusMotor, LaserController, LedController)
 *   b) Remote CAN slaves (via CanOpenStack PDO/SDO)
 *
 * Enumeration scheme (dynamic, based on local device count):
 *   Motor/Laser IDs 0..N-1  → local GPIO (N = actually configured axes with
 *                               a native driver, e.g. USE_FASTACCEL)
 *   Motor/Laser IDs N..     → CAN slaves, appended in order of discovery
 *
 *   CAN-only master (N=0):  IDs 0,1,2,3… all map to CAN slaves
 *   Hybrid (N>0):           IDs 0..N-1 local, N.. CAN slaves
 *
 *   LED ID 0               → local controller (if present)
 *   LED IDs 1..             → CAN slaves
 *
 * The route table is built at boot from:
 *   1. PinConfig / RuntimeConfig  → LOCAL_GPIO entries
 *   2. CAN slave capability queries → CAN_REMOTE entries
 *
 * When a CAN slave boots (NMT boot-up), it is queried for capabilities, and
 * its device IDs are added to the route table automatically.
 * When a slave goes offline (heartbeat timeout), its routes become UNAVAILABLE.
 *
 * Command: {"task":"/route_table_get"} dumps the current table to Serial.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

// Local motor ID range per CAN node slot
#define UC2_ROUTER_CAN_NODE_STRIDE  10u    // 10 devices per CAN node
#define UC2_ROUTER_LOCAL_MAX        10u    // IDs 0-9 are local
#define UC2_ROUTER_GLOBAL_MAX_MOTOR 40u
#define UC2_ROUTER_GLOBAL_MAX_LASER 40u
#define UC2_ROUTER_GLOBAL_MAX_LED    4u

// Route entry type
enum class UC2_RouteType : uint8_t {
    UNAVAILABLE = 0,   // device not present / slave offline
    LOCAL_GPIO  = 1,   // handled by local controller on this MCU
    CAN_REMOTE  = 2,   // forwarded to a CAN satellite via CanOpenStack
};

struct UC2_MotorRoute {
    UC2_RouteType type;
    uint8_t       localAxisId;   // axis index in the local FocusMotor driver
    uint8_t       canNodeId;     // CAN node-ID of the slave (CAN_REMOTE only)
    uint8_t       canAxisId;     // axis index on the slave node
};

struct UC2_LaserRoute {
    UC2_RouteType type;
    uint8_t       localChannelId;
    uint8_t       canNodeId;
    uint8_t       canChannelId;
};

struct UC2_LEDRoute {
    UC2_RouteType type;
    uint8_t       canNodeId;   // 0 = local
};

namespace DeviceRouter {

// ============================================================================
// Initialization
// ============================================================================

// Build the full route table.
// localMotorCount: number of local GPIO motor axes (from PinConfig)
// localLaserCount: number of local GPIO laser channels
// hasLocalLED:     true if a local LED strip is configured
void init(uint8_t localMotorCount, uint8_t localLaserCount, bool hasLocalLED);

// ============================================================================
// Dynamic route updates (called by NMT status callback)
// ============================================================================

// Called when a CAN slave comes online.
// caps: UC2_CAP_* bitmask from the slave's TPDO2 or SDO query.
// motorCount, laserCount: number of devices on that slave (from SDO query or default).
void onSlaveOnline(uint8_t canNodeId, uint8_t caps,
                   uint8_t motorCount = 1u, uint8_t laserCount = 1u);

// Called when a CAN slave goes offline (heartbeat timeout).
void onSlaveOffline(uint8_t canNodeId);

// ============================================================================
// Route lookup
// ============================================================================

const UC2_MotorRoute* getMotorRoute(uint8_t globalMotorId);
const UC2_LaserRoute* getLaserRoute(uint8_t globalLaserId);
const UC2_LEDRoute*   getLEDRoute(uint8_t globalLedId);

// ============================================================================
// Motor dispatch
// ============================================================================

// Move motor: dispatches to local FocusMotor or remote CAN slave.
// isAbsolute: true=absolute position, false=relative steps.
// Returns true if the command was dispatched (not necessarily completed).
bool motorMove(uint8_t globalMotorId, int32_t targetPos, int32_t speed,
               bool isAbsolute, int16_t qid);

// Stop motor immediately
bool motorStop(uint8_t globalMotorId, int16_t qid);

// Run homing sequence
bool motorHome(uint8_t globalMotorId, int16_t qid);

// Read current position (blocks on SDO read for CAN remote)
bool motorGetPos(uint8_t globalMotorId, int32_t* posOut);

// Check if motor is moving (SDO read for remote motors)
bool motorIsRunning(uint8_t globalMotorId);

// ============================================================================
// Laser dispatch
// ============================================================================

// Set laser PWM (0 = off, up to 1023)
bool laserSet(uint8_t globalLaserId, uint16_t pwm, int16_t qid = -1);

// ============================================================================
// LED dispatch
// ============================================================================

// Set LED command (mode, color, pixel index)
bool ledSet(uint8_t globalLedId, uint8_t mode,
            uint8_t r, uint8_t g, uint8_t b,
            uint16_t ledIndex = 0u, int16_t qid = -1);

// ============================================================================
// Route table introspection
// ============================================================================

// Dump route table to Serial (called by /route_table_get)
void dumpRouteTable();

// Get a JSON string describing the route table (caller must free with cJSON_Delete)
// Returns nullptr if cJSON is not available.
// Prototype is intentionally left without cJSON dependency here.
void* getRouteTableJSON();  // returns cJSON* — cast at call site

} // namespace DeviceRouter

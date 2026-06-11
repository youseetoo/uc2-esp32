# UC2 Device Routing System

## Overview

The routing system decides whether each logical device (motor axis, laser channel, LED, galvo, etc.) is handled **locally** on the current board or forwarded to a **remote** CAN slave via CANopen SDO.

The routing table is built once at boot (`RoutingTable::buildDefault()`) and consulted on every incoming command by `DeviceRouter::routeCommand()`.

## Architecture

```
Serial/WiFi JSON command
        │
        ▼
  SerialProcess.cpp
        │
        ▼
  DeviceRouter::routeCommand(task, doc)
        │
        ├─ LOCAL  → FocusMotor / LaserController / LedController / ...
        │
        ├─ REMOTE → CANopenModule::writeSDO() → CAN bus → slave OD → slave module
        │
        └─ OFF    → ignored (log warning)
```

## ROUTE_* Override Arrays

Each `PinConfig.h` declares override arrays that explicitly control routing per device/axis:

| Array | Dimension | Controls |
|-------|-----------|----------|
| `ROUTE_MOTOR[4]` | Per axis (A, X, Y, Z) | Motor step/dir commands |
| `ROUTE_HOME[4]` | Per axis | Homing commands (follows ROUTE_MOTOR if -1) |
| `ROUTE_TMC[4]` | Per axis | TMC UART config (follows ROUTE_MOTOR if -1) |
| `ROUTE_LASER[4]` | Per channel (0–3) | Laser PWM commands |
| `ROUTE_LED` | Scalar | LED array (WS2812 etc.) |
| `ROUTE_GALVO` | Scalar | Galvo scanner |

### Values

| Value | Enum | Meaning |
|-------|------|---------|
| `-1` | *(infer)* | Auto-detect from `canRole` + whether the axis has local GPIO pins |
| `0` | `LOCAL` | Handle on this board — call the local controller directly |
| `1` | `REMOTE` | Forward to a CAN slave via SDO (uses `CAN_ID_MOT_X` etc. for node ID) |
| `2` | `OFF` | Device is disabled — commands are silently dropped |

### Resolution Logic (`resolveRoute()` in RoutingTable.cpp)

```
if (ROUTE_xxx[ax] >= 0):
    use the explicit value directly    →  LOCAL / REMOTE / OFF
else (-1 = infer):
    if canRole == STANDALONE or CAN_SLAVE:
        has local pin?  → LOCAL  :  OFF
    if canRole == CAN_MASTER:
        has local pin?  → LOCAL  :  REMOTE (auto-assign node ID)
```

## Default Configuration (PinConfigDefault.h)

All ROUTE_* values default to **-1 (infer)**:

```cpp
int8_t ROUTE_MOTOR[4]  = {-1, -1, -1, -1};
int8_t ROUTE_HOME[4]   = {-1, -1, -1, -1};
int8_t ROUTE_TMC[4]    = {-1, -1, -1, -1};
int8_t ROUTE_LASER[4]  = {-1, -1, -1, -1};
int8_t ROUTE_LED        = -1;
int8_t ROUTE_GALVO      = -1;
```

This means standalone boards (UC2_2, UC2_3) with no CAN will automatically route everything LOCAL if the pin exists, or OFF if not. No manual configuration needed.

## Example: CANopen Master (UC2_canopen_master/PinConfig.h)

The master has no motor step pins — all motors are on CAN slaves:

```cpp
int8_t ROUTE_MOTOR[4] = {1, 1, 1, 1}; // All REMOTE
int8_t ROUTE_HOME[4]  = {1, 1, 1, 1}; // All REMOTE
int8_t ROUTE_TMC[4]   = {1, 1, 1, 1}; // All REMOTE
int8_t ROUTE_LASER[4] = {1, 1, 1, 1}; // All REMOTE
int8_t ROUTE_LED       = 0;            // LOCAL (on-board WS2812)
int8_t ROUTE_GALVO     = 1;            // REMOTE
```

Result: When the master receives `{"task":"/motor_act","motor":{"steppers":[{"stepperid":1,"position":1000,"speed":5000}]}}`, the DeviceRouter sends SDO writes to the slave node responsible for axis X.

## Example: CANopen Slave (UC2_canopen_slave/PinConfig.h)

The slave has one physical motor on axis X (stepperid=1):

```cpp
int8_t ROUTE_MOTOR[4] = {2, 0, 2, 2}; // A=OFF, X=LOCAL, Y=OFF, Z=OFF
int8_t ROUTE_HOME[4]  = {2, 0, 2, 2};
int8_t ROUTE_TMC[4]   = {2, 0, 2, 2};
int8_t ROUTE_LASER[4] = {2, 2, 2, 2}; // All OFF
int8_t ROUTE_LED       = 2;            // OFF
int8_t ROUTE_GALVO     = 2;            // OFF
```

Only axis X (index 1) is LOCAL. All other devices are OFF because the slave doesn't have those peripherals.

## Runtime Inspection

### Serial endpoint: `/route_get`

Returns the full routing table as JSON:

```json
{"routes":[
  {"type":"motor","id":0,"where":"remote","nodeId":10,"subAxis":0},
  {"type":"motor","id":1,"where":"remote","nodeId":11,"subAxis":0},
  {"type":"led","id":0,"where":"local"},
  ...
]}
```

### Boot log

At startup, `RoutingTable::logAll()` prints the entire table to serial:

```
[routing] Routing table (18 entries):
[routing]   motor  0 -> REMOTE node 10 axis 0
[routing]   motor  1 -> REMOTE node 11 axis 0
[routing]   led    0 -> local
[routing]   galvo  0 -> REMOTE node 14 axis 0
```

## Position Sync (Master ↔ Slave)

When the master dispatches a motor command to a REMOTE slave:

1. Master writes target position, speed, acceleration, isAbsolute via SDO
2. Master writes command word via SDO → slave starts motor
3. Slave updates `OD_RAM.x2001_motor_actual_position` every 1ms in `syncModulesToTpdo()`
4. Master polls slave position+status via SDO every 200ms in `CANopenModule::loop()`
5. Master updates its local `FocusMotor::getData()[ax]->currentPosition` and `stopped` flag

This ensures `motor_get` and serial position reports on the master reflect the actual slave position.

## Files

| File | Purpose |
|------|---------|
| `main/src/canopen/RoutingTable.h` | RouteEntry struct, RoutingTable class declaration |
| `main/src/canopen/RoutingTable.cpp` | buildDefault(), resolveRoute(), find(), set() |
| `main/src/canopen/DeviceRouter.h` | DeviceRouter class declaration |
| `main/src/canopen/DeviceRouter.cpp` | routeCommand() → per-device handle*() methods |
| `main/PinConfigDefault.h` | Default ROUTE_* arrays (all -1) |
| `main/config/*/PinConfig.h` | Board-specific ROUTE_* overrides |

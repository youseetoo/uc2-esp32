# UC2 CANopen-Lite — WP3/4/5 Implementation Reference

> **Status:** Implemented (2026-03-31)  
> **Firmware branch:** `rewrite-canopen`  
> **Source directories:** `main/src/CANopen/`, `main/src/DeviceRouter.h/.cpp`

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [CAN-ID Allocation](#2-can-id-allocation)
3. [WP3 — CANopen-Lite Protocol Engine](#3-wp3--canopen-lite-protocol-engine)
   - 3.1 [Object Dictionary](#31-object-dictionary)
   - 3.2 [PDO Engine — Payload Reference](#32-pdo-engine--payload-reference)
   - 3.3 [SDO Handler](#33-sdo-handler)
   - 3.4 [NMT Manager](#34-nmt-manager)
   - 3.5 [Heartbeat Manager](#35-heartbeat-manager)
4. [WP4 — Device Router](#4-wp4--device-router)
   - 4.1 [Enumeration Scheme](#41-enumeration-scheme)
   - 4.2 [Route Table Lifecycle](#42-route-table-lifecycle)
5. [WP5 — Slave Firmware](#5-wp5--slave-firmware)
   - 5.1 [Slave Boot Sequence](#51-slave-boot-sequence)
   - 5.2 [SlaveController OD Adapter](#52-slavecontroller-od-adapter)
6. [Flow Diagrams](#6-flow-diagrams)
   - 6.1 [Master Boot & Slave Discovery](#61-master-boot--slave-discovery)
   - 6.2 [Motor Move (Master → Slave)](#62-motor-move-master--slave)
   - 6.3 [SDO Read (Master reads slave OD)](#63-sdo-read-master-reads-slave-od)
   - 6.4 [Motor Move Completion (Slave → Master)](#64-motor-move-completion-slave--master)
   - 6.5 [Heartbeat & Offline Detection](#65-heartbeat--offline-detection)
   - 6.6 [Slave NMT State Machine](#66-slave-nmt-state-machine)
7. [Build System Integration](#7-build-system-integration)
   - 7.1 [Source Files](#71-source-files)
   - 7.2 [CMakeLists.txt](#72-cmakeliststxt)
   - 7.3 [main.cpp Wiring](#73-maincpp-wiring)
8. [PlatformIO Environments](#8-platformio-environments)
9. [Integration Checklist](#9-integration-checklist)

---

## 1. Architecture Overview

The UC2 CANopen-Lite stack replaces the legacy `CanController` (which forwarded raw JSON over ISO-TP). It implements a **subset of CiA 301** using the ESP-IDF TWAI driver directly — no external CANopenNode library is required.

```
┌─────────────────────────────────────────────────────────────────────┐
│                         MASTER NODE                                 │
│  (ESP32-DevKit / UC2 v4 hat, runs full UC2 firmware)               │
│                                                                      │
│  ┌────────────┐    ┌─────────────────┐    ┌──────────────────────┐  │
│  │ Host (USB/ │    │  DeviceRouter   │    │  CanOpenStack        │  │
│  │ Serial/    │───▶│  (WP4)          │───▶│  (WP3)               │  │
│  │ WiFi/BT)   │    │  motorMove()    │    │  sendMotorMove()     │  │
│  └────────────┘    │  laserSet()     │    │  sendLaserSet()      │  │
│                    │  ledSet()       │    │  sendLEDSet()        │  │
│                    └────────┬────────┘    └──────────┬───────────┘  │
│                             │ LOCAL_GPIO              │ CAN_REMOTE  │
│                    ┌────────▼────────┐               │              │
│                    │ FocusMotor      │               │              │
│                    │ LaserController │               │              │
│                    │ LedController   │               │              │
│                    └─────────────────┘               │              │
│                                                      │              │
│  Entry: main.cpp → CanOpenMaster::setup() / loop()   │              │
└─────────────────────────────────────────────────────┼──────────────┘
                                                       │ ISO 11898
                                                       │ TWAI / CAN 2.0B
                          ┌────────────────────────────┼───────────────┐
                          │  CAN BUS  (500 kbit/s)     │               │
                          └────────────────────────────┼───────────────┘
                          ┌─────────────────┐          │   ┌───────────┐
                          │  SLAVE 0x10     │◀─────────┘   │ SLAVE 0x14│
                          │  (Xiao, motor)  │              │ (motor+   │
                          │  CanOpenSlave   │              │  laser+   │
                          │  SlaveController│              │  LED)     │
                          └─────────────────┘              └───────────┘

Entry: main.cpp → CanOpenSlave::setup() / loop()
```

**Key design decisions:**

| Decision | Rationale |
|---|---|
| Raw TWAI, no CANopenNode library | Avoids complex OD auto-generation tool; reduces binary size ~80 kB |
| Fixed PDO mappings | Runtime OD mapping (PDO mapping objects 0x1600/0x1A00) not needed for UC2 use case |
| Expedited SDO only (≤ 4 bytes) | All UC2 OD entries fit in 4 bytes; segmented transfers reserved for OTA |
| FreeRTOS task for CAN RX | Ensures frames are never dropped; keeps `loop()` non-blocking |
| Capability bitmask at boot | Slave announces what it has in boot-up TPDO2; master builds route table automatically |

---

## 2. CAN-ID Allocation

Standard CANopen CAN-ID allocation, following CiA 301. All slaves share the same schema — the node-ID determines the CAN-ID offset.

```
CAN-ID (11-bit)      Direction          Purpose
---------------------------------------------------------------------------
0x000                broadcast          NMT command
0x080                broadcast          SYNC (not used in UC2-Lite)
0x180 + nodeId       slave → master     TPDO1: Motor Status
0x200 + nodeId       master → slave     RPDO1: Motor Position + Speed
0x280 + nodeId       slave → master     TPDO2: Node Status
0x300 + nodeId       master → slave     RPDO2: Control (motor trigger, laser, LED)
0x380 + nodeId       slave → master     TPDO3: Extended Sensor Data
0x400 + nodeId       master → slave     RPDO3: LED Extended (blue + ledIndex + qid)
0x580 + nodeId       slave → master     SDO Response
0x600 + nodeId       master → slave     SDO Request
0x700 + nodeId       slave → master     NMT Heartbeat
---------------------------------------------------------------------------
```

**Example for node 0x10 (decimal 16):**

| CAN-ID | Function |
|--------|----------|
| `0x190` | TPDO1 — Motor Status |
| `0x210` | RPDO1 — Motor Pos/Speed |
| `0x290` | TPDO2 — Node Status |
| `0x310` | RPDO2 — Control |
| `0x390` | TPDO3 — Sensor Data |
| `0x410` | RPDO3 — LED Extra |
| `0x590` | SDO Response |
| `0x610` | SDO Request |
| `0x710` | Heartbeat |

---

## 3. WP3 — CANopen-Lite Protocol Engine

### 3.1 Object Dictionary

The UC2 Object Dictionary (OD) is **static** — no runtime-configurable entries, no OD auto-generation. All indices are `#define` constants in `ObjectDictionary.h`.

#### Standard CiA 301 Entries

| Index | Name | Type | Description |
|-------|------|------|-------------|
| `0x1000` | Device Type | uint32 | Profile: 0x00000000 (generic) |
| `0x1001` | Error Register | uint8 | Bit 0 = generic error |
| `0x1017` | Heartbeat Time | uint16 | Producer period in ms (0 = off) |
| `0x1018` | Identity | record | Vendor, product, serial |
| `0x1F50` | FW Data | octet string | OTA firmware payload |
| `0x1F51` | Program Control | uint8 | 0x01 = start OTA programming |

#### UC2 Manufacturer-Specific Entries (0x2000–0x2FFF)

##### Node Information

| Index | Sub | Type | R/W | Description |
|-------|-----|------|-----|-------------|
| `0x2000` | 0x00 | uint8 | RO | Capability bitmask |
| `0x2001` | 0x00 | uint8 | RO | Node role (0=slave, 1=master) |

##### Motor Axes (one object per axis, `0x2101 + axis`)

| Index | Sub | Type | R/W | Description |
|-------|-----|------|-----|-------------|
| `0x2100` | 0x00 | uint8 | RO | Number of motor axes |
| `0x2101` + axis | 0x01 | int32 | RW | Target position (steps) |
| `0x2101` + axis | 0x02 | int32 | RW | Speed (steps/s) |
| `0x2101` + axis | 0x03 | uint8 | RW | Command — **write triggers move** |
| `0x2101` + axis | 0x04 | int32 | RO | Actual position |
| `0x2101` + axis | 0x05 | uint8 | RO | Is running (0/1) |
| `0x2101` + axis | 0x06 | int16 | RW | QID for completion event |

**Motor command byte values:**

| Value | Constant | Effect |
|-------|----------|--------|
| `0` | `UC2_MOTOR_CMD_STOP` | Stop immediately |
| `1` | `UC2_MOTOR_CMD_MOVE_REL` | Move relative (±steps) |
| `2` | `UC2_MOTOR_CMD_MOVE_ABS` | Move to absolute position |
| `3` | `UC2_MOTOR_CMD_HOME` | Run homing sequence |

##### Laser Channels (`0x2201 + channel`)

| Index | Sub | Type | R/W | Description |
|-------|-----|------|-----|-------------|
| `0x2200` | 0x00 | uint8 | RO | Number of laser channels |
| `0x2201` + ch | 0x01 | uint16 | RW | PWM value 0–1023 |
| `0x2201` + ch | 0x02 | uint8 | RW | Despeckle on/off |
| `0x2201` + ch | 0x03 | uint16 | RW | Despeckle period (ms) |

##### LED Controller

| Index | Sub | Type | R/W | Description |
|-------|-----|------|-----|-------------|
| `0x2300` | 0x00 | uint16 | RO | Number of LEDs in strip |
| `0x2301` | 0x01 | uint8 | RW | Mode (0=off, 1=fill, 2=single, …) |
| `0x2301` | 0x02 | uint8 | RW | Red channel (0–255) |
| `0x2301` | 0x03 | uint8 | RW | Green channel (0–255) |
| `0x2301` | 0x04 | uint8 | RW | Blue channel (0–255) |
| `0x2301` | 0x05 | uint16 | RW | LED pixel index (single mode) |
| `0x2301` | 0x06 | int16 | RW | QID |

##### Misc Devices

| Index | Device | Sub | Description |
|-------|--------|-----|-------------|
| `0x2401` + ch | DAC | 0x01 | uint16 — output value (0–4095) |
| `0x2500` | Heater | 0x01/0x02/0x03 | target temp, actual temp, PID enable |
| `0x2601` + ax | Encoder | 0x01 | int32 — current position |
| `0x2700` | Digital I/O | 0x01–0x04 | in/out counts + bitmasks |
| `0x2F00` | Capability table | 0x00 | same as `0x2000` |
| `0x2FF0` | Self-test | 0x00 | write `0x01` to trigger |

#### Capability Bitmask (uint8 at `0x2000`)

| Bit | Constant | Device |
|-----|----------|--------|
| 0 | `UC2_CAP_MOTOR` | Stepper motors |
| 1 | `UC2_CAP_LASER` | Laser PWM channels |
| 2 | `UC2_CAP_LED` | NeoPixel LED strip |
| 3 | `UC2_CAP_ENCODER` | Quadrature encoder |
| 4 | `UC2_CAP_HEATER` | Heater / PID controller |
| 5 | `UC2_CAP_DAC` | Analog DAC output |
| 6 | `UC2_CAP_DOUT` | Digital outputs |
| 7 | `UC2_CAP_DIN` | Digital inputs |

---

### 3.2 PDO Engine — Payload Reference

All PDO payloads are **exactly 8 bytes**, little-endian, `#pragma pack(push, 1)`.

#### RPDO1 — Motor Position + Speed (master → slave)

**CAN-ID:** `0x200 + nodeId`  
**Trigger:** Always sent before RPDO2 that carries the motor command.

```
Byte  0   1   2   3     4   5   6   7
      [  target_pos (int32)  ] [  speed (int32)  ]
```

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `target_pos` | int32 | ±2³¹ steps | Target in steps. Relative or absolute depending on RPDO2 cmd. |
| `speed` | int32 | 0–max steps/s | 0 = use slave default configured in NVS |

#### RPDO2 — Control Frame (master → slave)

**CAN-ID:** `0x300 + nodeId`  
**Trigger:** Sent after RPDO1 for motor moves; standalone for laser / LED head-only.

```
Byte  0      1      2          3   4       5        6      7
      [axis] [cmd]  [laser_id] [ laser_pwm (u16) ] [mode] [r] [g]
```

| Field | Type | Description |
|-------|------|-------------|
| `axis` | uint8 | Motor axis 0–9. `0xFF` = laser-only frame. `0xFE` = LED-only frame. |
| `cmd` | uint8 | `UC2_MOTOR_CMD_*` (ignored when `axis=0xFF/0xFE`) |
| `laser_id` | uint8 | Laser channel 0–9 |
| `laser_pwm` | uint16 | 0–1023 |
| `led_mode` | uint8 | LED mode enum from `LedMode` |
| `r` | uint8 | LED red |
| `g` | uint8 | LED green |

#### RPDO3 — LED Extended + QID (master → slave)

**CAN-ID:** `0x400 + nodeId`  
**Trigger:** Always sent together with RPDO2 when an LED or QID is needed.

```
Byte  0   1   2       3   4       5   6   7
      [b] [ led_index ] [   qid  ] [  pad   ]
```

| Field | Type | Description |
|-------|------|-------------|
| `b` | uint8 | LED blue channel |
| `led_index` | uint16 | Pixel index (single-LED mode) |
| `qid` | int16 | QID to echo in TPDO1 on completion. `-1` = no ack required. |
| `_pad` | uint8[3] | Reserved (set to 0) |

#### TPDO1 — Motor Status (slave → master)

**CAN-ID:** `0x180 + nodeId`  
**Sent:** Event-driven — on move completion and when motor is started.

```
Byte  0   1   2   3     4       5       6   7
      [  actual_pos (int32) ] [axis] [run] [ qid  ]
```

| Field | Type | Description |
|-------|------|-------------|
| `actual_pos` | int32 | Current position in steps at time of event |
| `axis` | uint8 | Which axis this status belongs to |
| `is_running` | uint8 | 1 = still moving, 0 = stopped |
| `qid` | int16 | QID from the command that triggered this status |

#### TPDO2 — Node Status (slave → master)

**CAN-ID:** `0x280 + nodeId`  
**Sent:** Periodically (default: every 1 s) and on NMT state change.

```
Byte  0       1        2          3   4       5   6     7
      [caps] [nodeid] [err_reg] [ temp (i16) ] [enc] [nmt]
```

| Field | Type | Description |
|-------|------|-------------|
| `capabilities` | uint8 | `UC2_CAP_*` bitmask |
| `node_id` | uint8 | Own node-ID (self-identification at first boot) |
| `error_reg` | uint8 | Bit 0 = communication error, bit 1 = device error |
| `temperature` | int16 | °C × 10 (`0x7FFF` = no sensor present) |
| `encoder_pos` | int16 | Truncated encoder position axis 0 (int32 available via SDO/TPDO3) |
| `nmt_state` | uint8 | Own NMT state (`0x05` = OPERATIONAL, `0x7F` = PRE-OPERATIONAL) |

#### TPDO3 — Extended Sensor Data (slave → master)

**CAN-ID:** `0x380 + nodeId`  
**Sent:** On demand or every few seconds when sensors are active.

```
Byte  0   1   2   3       4   5     6          7
      [  encoder_pos_hi  ] [ adc ] [din_state] [dout_state]
```

| Field | Type | Description |
|-------|------|-------------|
| `encoder_pos_hi` | int32 | Full 32-bit encoder position axis 0 |
| `adc_raw` | uint16 | Raw ADC channel 0 (0–4095) |
| `din_state` | uint8 | Digital input bitmask (8 inputs) |
| `dout_state` | uint8 | Current digital output bitmask |

---

### 3.3 SDO Handler

#### Protocol (CiA 301 Expedited Transfer)

All UC2 OD entries are ≤ 4 bytes → only expedited transfers are used during normal operation.

**Download (master writes to slave):**

```
Master (COB-ID 0x600+n) ──────────────────────────────► Slave
  [cs] [idx_lo] [idx_hi] [sub] [d0] [d1] [d2] [d3]
   cs = 0x23 (4B) | 0x27 (3B) | 0x2B (2B) | 0x2F (1B)

Slave (COB-ID 0x580+n)  ◄──────────────────────────────
  [0x60] [idx_lo] [idx_hi] [sub] [0] [0] [0] [0]    ← ACK
  [0x80] [idx_lo] [idx_hi] [sub] [ac0][ac1][ac2][ac3] ← ABORT
```

**Upload (master reads from slave):**

```
Master (COB-ID 0x600+n) ──────────────────────────────► Slave
  [0x40] [idx_lo] [idx_hi] [sub] [0] [0] [0] [0]

Slave (COB-ID 0x580+n)  ◄──────────────────────────────
  [4x] [idx_lo] [idx_hi] [sub] [d0] [d1] [d2] [d3]
   x = 3 (1B) | 2 (2B) | 1 (3B) | 0 (4B)
```

#### SDO Client API (master side)

```cpp
// Low-level
SDOHandler::writeExpedited(nodeId, index, sub, data, len);
SDOHandler::readExpedited(nodeId, index, sub, buf, bufLen, &bytesRead);

// Typed helpers
SDOHandler::writeU8(nodeId, idx, sub, value);
SDOHandler::writeU16(nodeId, idx, sub, value);
SDOHandler::writeU32(nodeId, idx, sub, value);
SDOHandler::writeI32(nodeId, idx, sub, value);
SDOHandler::readU8(nodeId, idx, sub, &out);
SDOHandler::readU16(nodeId, idx, sub, &out);
SDOHandler::readI32(nodeId, idx, sub, &out);

// UC2 convenience (combine multiple writes internally)
SDOHandler::motorCommand(nodeId, axis, targetPos, speed, cmd);
SDOHandler::laserSet(nodeId, channel, pwm);
SDOHandler::ledSet(nodeId, mode, r, g, b, ledIndex);
SDOHandler::motorGetPos(nodeId, axis, &posOut);
SDOHandler::motorIsRunning(nodeId, axis, &runningOut);
SDOHandler::nodeGetCaps(nodeId, &capsOut);
SDOHandler::configureHeartbeat(nodeId, periodMs);
```

#### Error (Abort) Codes

| Code | Constant | Meaning |
|------|----------|---------|
| `0x06020000` | `SDO_ABORT_NOT_EXIST` | OD entry does not exist |
| `0x06010002` | `SDO_ABORT_RO` | Attempt to write a read-only entry |
| `0x08000000` | `SDO_ABORT_GENERAL` | General internal error |

#### Retry behaviour

- Timeout per attempt: `SDO_TIMEOUT_MS = 500 ms`
- Max retries: `SDO_MAX_RETRIES = 3`
- On final failure: returns `false`; caller should log and mark slave suspect

---

### 3.4 NMT Manager

#### States

```
              ┌─────────────────────────────────────────┐
              │             INITIALIZING (0x00)          │
              │  (entered at power-on; sends boot-up HB) │
              └──────────────────────┬──────────────────┘
                                     │ automatically
                                     ▼
              ┌─────────────────────────────────────────┐
              │           PRE-OPERATIONAL (0x7F)         │
              │  SDO: allowed   PDO: blocked             │
              └────────────────┬──────────┬─────────────┘
       NMT START (0x01) ───────┘          └──── NMT STOP (0x02)
                               │                         │
                               ▼                         ▼
              ┌───────────────────────┐   ┌─────────────────────────┐
              │   OPERATIONAL (0x05)  │   │     STOPPED (0x04)      │
              │  SDO+PDO: allowed     │   │  SDO: blocked           │
              │  HeartBeat: running   │   │  PDO: blocked           │
              └───────────────────────┘   └─────────────────────────┘
```

NMT commands are broadcast at **COB-ID 0x000** and contain 2 data bytes:
- `data[0]` = command byte (`0x01`=START, `0x02`=STOP, `0x80`=PRE-OP, `0x81`=RESET, `0x82`=RESET-COMM)
- `data[1]` = target node-ID (0 = all nodes)

#### Master API

```cpp
NMTManager::sendStart(nodeId);           // 0x01 → node
NMTManager::sendStop(nodeId);            // 0x02 → node
NMTManager::sendPreOperational(nodeId);  // 0x80 → node
NMTManager::sendReset(nodeId);           // 0x81 → node
NMTManager::startAll();                  // 0x01 → 0x00 (broadcast)
NMTManager::update();                    // call from loop() — checks HB timeouts
```

#### NodeEntry struct (per slave in master's table)

```cpp
struct UC2_NodeEntry {
    uint8_t       nodeId;
    UC2_NMT_State state;          // current NMT state
    uint8_t       capabilities;   // UC2_CAP_* bitmask from TPDO2
    uint32_t      lastHeartbeatMs;
    uint32_t      heartbeatTimeoutMs;  // 0 = monitoring disabled
    bool          online;
};
```

---

### 3.5 Heartbeat Manager

- **Producer (slave):** Sends `0x700 + nodeId` at `heartbeatPeriodMs` interval.  
  Data byte = NMT state: `0x00` = boot-up (sent once), then `0x05` / `0x7F` / `0x04`.
- **Consumer (master):** `processFrame()` dispatches to `NMTManager::onBootUp()` or `onHeartbeat()`.
- **Timeout detection:** `NMTManager::update()` marks node offline if `now − lastHB > heartbeatTimeoutMs`.

```cpp
HeartbeatManager::init(ownNodeId, periodMs);   // slave
HeartbeatManager::tick();                       // slave — call from loop()
HeartbeatManager::processFrame(frame);          // master — call from CAN RX task
HeartbeatManager::setProducerPeriod(periodMs);  // change at runtime
```

---

## 4. WP4 — Device Router

### 4.1 Enumeration Scheme

Device IDs are **globally unique** across the entire system. The DeviceRouter maps a global ID to either a local GPIO controller or a CAN remote slave.

```
Global Motor ID  │  Route
─────────────────┼──────────────────────────────────────────────────
  0 – 9          │  LOCAL — FocusMotor axis 0–9 on this MCU
 10 – 19         │  CAN node 0x10, local axis = globalId − 10
 20 – 29         │  CAN node 0x14, local axis = globalId − 20
 30 – 39         │  CAN node 0x18, local axis = globalId − 30
─────────────────┼──────────────────────────────────────────────────
Global Laser ID  │  Route
─────────────────┼──────────────────────────────────────────────────
  0 – 9          │  LOCAL — LaserController channel 0–9
 10 – 19         │  CAN node 0x10, local channel = globalId − 10
 20 – 29         │  CAN node 0x14, local channel = globalId − 20
─────────────────┼──────────────────────────────────────────────────
Global LED ID    │  Route
─────────────────┼──────────────────────────────────────────────────
  0              │  LOCAL — LedController
  1              │  CAN node 0x10
  2              │  CAN node 0x14
```

CAN node-ID formula for any global ID ≥ 10:

```
canNodeId = 0x10 + ((globalId / 10) − 1) × 4
canAxisId = globalId mod 10
```

### 4.2 Route Table Lifecycle

```
                init(localMotors, localLasers, hasLED)
                        │
              Build LOCAL_GPIO entries for IDs 0–(n-1)
                        │
                   CAN slave boots
                        │
          CanOpenStack RX task receives HB boot-up
                        │
           NMTManager::onBootUp(nodeId)
           → sendStart(nodeId)  ← auto-start slave
                        │
        Wait for TPDO2 (Node Status) from slave
                        │
           DeviceRouter::onSlaveOnline(canNodeId, caps, nMotors, nLasers)
           → Add CAN_REMOTE entries to route table
                        │
                Slave goes offline after HB timeout
                        │
           DeviceRouter::onSlaveOffline(canNodeId)
           → Mark all routes for that node as UNAVAILABLE
```

#### Route Table struct

```cpp
struct UC2_MotorRoute {
    UC2_RouteType type;       // UNAVAILABLE | LOCAL_GPIO | CAN_REMOTE
    uint8_t localAxisId;      // axis index in FocusMotor
    uint8_t canNodeId;        // CAN node-ID (CAN_REMOTE only)
    uint8_t canAxisId;        // axis index on slave (CAN_REMOTE only)
};
```

#### DeviceRouter API

```cpp
// Initialization
DeviceRouter::init(localMotorCount, localLaserCount, hasLocalLED);

// Dynamic updates (called from NMT/status callbacks)
DeviceRouter::onSlaveOnline(canNodeId, caps, motorCount, laserCount);
DeviceRouter::onSlaveOffline(canNodeId);

// Command dispatch
DeviceRouter::motorMove(globalId, targetPos, speed, isAbsolute, qid) → bool;
DeviceRouter::motorStop(globalId, qid) → bool;
DeviceRouter::motorHome(globalId, qid) → bool;
DeviceRouter::motorGetPos(globalId, &posOut) → bool;
DeviceRouter::motorIsRunning(globalId) → bool;
DeviceRouter::laserSet(globalId, pwm, qid) → bool;
DeviceRouter::ledSet(globalId, mode, r, g, b, ledIndex, qid) → bool;

// Diagnostics
DeviceRouter::dumpRouteTable();         // Serial output
DeviceRouter::getRouteTableJSON();      // returns cJSON*
```

---

## 5. WP5 — Slave Firmware

All slave code lives in `main/src/CANopen/` alongside the protocol engine.
There is **no separate `slave/` directory** — master and slave are the same
firmware, differentiated by compile flags `UC2_CANOPEN_MASTER` or
`UC2_CANOPEN_SLAVE`. The entry point is always `main/main.cpp`.

### 5.1 Slave Boot Sequence

```
main/main.cpp  (built with -DUC2_CANOPEN_ENABLED=1 -DUC2_CANOPEN_SLAVE=1)
   │
   ├── setup()
   │     ├── SerialProcess::setup()
   │     ├── FocusMotor::setup()       (if MOTOR_CONTROLLER)
   │     ├── LaserController::setup()  (if LASER_CONTROLLER)
   │     ├── LedController::setup()    (if LED_CONTROLLER)
   │     │
   │     └── CanOpenSlave::setup()     ← #ifdef UC2_CANOPEN_SLAVE
   │           ├── SlaveController::init(caps, nodeId, numMotors, numLasers, hasLED)
   │           │     └── Clears runtime state tables
   │           ├── CanOpenStack::setSdoODCallbacks(sdoRead, sdoWrite, nullptr)
   │           ├── CanOpenStack::setRPDOCallback(onRPDO)
   │           └── CanOpenStack::init(TX_PIN, RX_PIN, nodeId, UC2_CAN_500K, HB_PERIOD_MS)
   │                 ├── Installs TWAI driver
   │                 ├── Sends boot-up heartbeat (data=0x00 at 0x700+nodeId)
   │                 ├── Slave enters PRE-OPERATIONAL
   │                 └── Starts FreeRTOS RX task on core 1
   │
   └── loop()
         ├── ... other controllers ...
         │
         └── CanOpenSlave::loop()      ← #ifdef UC2_CANOPEN_SLAVE
               ├── CanOpenStack::loop()    ← HeartbeatManager::tick() + NMTManager::update()
               └── (if OPERATIONAL) SlaveController::loop()
                     ├── Update actualPos from FocusMotor (if running)
                     ├── Detect motor stop → publishMotorStatus() → TPDO1
                     └── Every 1 s → publishNodeStatus() → TPDO2
```

### 5.1b Master Boot Sequence

```
main/main.cpp  (built with -DUC2_CANOPEN_ENABLED=1 -DUC2_CANOPEN_MASTER=1)
   │
   ├── setup()
   │     ├── ... local controllers ...
   │     │
   │     └── CanOpenMaster::setup()    ← #ifdef UC2_CANOPEN_MASTER
   │           ├── DeviceRouter::init(localMotors, localLasers, localLED)
   │           ├── CanOpenStack::setTPDOCallback(onTPDO)
   │           ├── CanOpenStack::setNodeStatusCallback(onNodeStatus)
   │           ├── CanOpenStack::init(TX_PIN, RX_PIN, MASTER_NODE_ID, ...)
   │           └── CanOpenStack::startAll()
   │
   └── loop()
         └── CanOpenMaster::loop()     ← #ifdef UC2_CANOPEN_MASTER
               └── CanOpenStack::loop()
```

### 5.2 SlaveController OD Adapter

`SlaveController` sits between CanOpenStack's SDO/RPDO callbacks and the actual device controllers. Its callbacks implement read/write access to every UC2 OD entry.

#### SDO Read path

```
Master sends: [0x40][idx_lo][idx_hi][sub][0][0][0][0]  at 0x600+nodeId
       │
CanOpenStack → SDOHandler::sdoServerProcessFrame()
       │
SlaveController::sdoRead(index, subIndex, buf, &len, ctx)
       │
 index == 0x2000 → return s_caps
 index == 0x21xx → axis = index - 0x2101
   sub 0x04  → read FocusMotor::getCurrentMotorPosition(axis)
   sub 0x05  → read FocusMotor::isRunning(axis)
   ...
       │
SDOHandler sends: [4x][idx_lo][idx_hi][sub][d0..d3]   at 0x580+nodeId
```

#### SDO Write path (with side effects)

```
Master sends: [cs][idx_lo][idx_hi][sub][d0..d3]  at 0x600+nodeId
       │
SlaveController::sdoWrite(index, subIndex, data, len, ctx)
       │
 index == 0x21xx, sub 0x01 → store targetPos
 index == 0x21xx, sub 0x02 → store speed
 index == 0x21xx, sub 0x03 → execute motor command  ← SIDE EFFECT
   UC2_MOTOR_CMD_MOVE_REL → FocusMotor::startStepper(axis, ...)
   UC2_MOTOR_CMD_STOP     → FocusMotor::stopStepper(axis)
 index == 0x2201+ch, sub 0x01 → LaserController::setLaserVal(ch, pwm)
 index == 0x2301, sub 0x02/03/04 → store r/g/b → LedController::act()
       │
SDOHandler sends ACK: [0x60][idx_lo][idx_hi][sub][0][0][0][0]
```

#### RPDO Dispatch

```
Master sends 3 PDO frames in sequence for a motor move + status echo:

  Frame 1 (RPDO1, 0x200+n):  target_pos + speed
  Frame 2 (RPDO2, 0x300+n):  axis + cmd + laser_id + laser_pwm + led_mode + r + g
  Frame 3 (RPDO3, 0x400+n):  b + led_index + qid

CanOpenStack RX task → PDOEngine::decodeMotorPosPDO / decodeControlPDO / decodeLEDExtraPDO
       │
SlaveController::onRPDO(rp1, rp2, rp3)
       │
 rp1 != nullptr → store targetPos + speed in s_motors[0]
 rp2 != nullptr
   axis < numMotors → store axis-specific cmd
                    → FocusMotor::startStepper(axis) / stopStepper(axis)
   laser_id < numLasers → LaserController::setLaserVal(laser_id, laser_pwm)
   led_mode present → store r, g in s_led
 rp3 != nullptr → store b, led_index → LedController::act()
```

---

## 6. Flow Diagrams

### 6.1 Master Boot & Slave Discovery

```
MASTER                                    SLAVE 0x10
   │                                          │
   │  setup()                                 │  setup()
   │  CanOpenMaster::setup()                  │  CanOpenSlave::setup()
   │  ├─ DeviceRouter::init(4, 4, true)       │  ├─ SlaveController::init(...)
   │  ├─ CanOpenStack::setTPDOCallback(…)     │  ├─ CanOpenStack::setSdoODCallbacks(…)
   │  ├─ CanOpenStack::setNodeStatusCb(…)     │  ├─ CanOpenStack::setRPDOCallback(…)
   │  └─ CanOpenStack::init(…)                │  └─ CanOpenStack::init()
   │       │                                  │       │
   │       │                                  │       ├─ TWAI driver installed
   │       │                                  │       └─ Boot-up HB sent ──────────────────────────▶
   │       │                                  │                         HB frame (0x710, data=0x00)
   │       │◀────────────────────────────────────────────────────────────────────────
   │  HeartbeatManager::processFrame()         │
   │  → NMTManager::onBootUp(0x10)             │
   │  → NMTManager::sendStart(0x10)  ─────────────────────────────────────────────▶
   │                                  NMT START (0x000, [0x01, 0x10])              │
   │                                                                    NMTManager::onNMTCommand()
   │                                                                    → state = OPERATIONAL
   │                                                                    PDO / SDO now active
   │                                                                          │
   │  (Master reads caps via SDO)                                             │
   │  SDOHandler::nodeGetCaps(0x10, &caps)  ─────────────────────────────────▶
   │                SDO Request (0x610, [0x40, 0x00, 0x20, 0x00, ...])        │
   │                                                                    sdoRead(0x2000, 0x00)
   │                                                                    → return s_caps
   │◀──────────────────────────────────────────────────────────────────────────
   │          SDO Response (0x590, [0x4F, 0x00, 0x20, 0x00, caps])    │
   │                                                                          │
   │  DeviceRouter::onSlaveOnline(0x10, caps, motorCount, laserCount)         │
   │  → Add CAN_REMOTE entries: motorRoutes[10..13] → CAN 0x10               │
   │                                                                          │
   │  loop(): periodic TPDO2 from slave ◀────────────────────────────────────┤
   │     TPDO2 (0x290, node_status)                                           │
   │  NMTManager::onNodeStatusPDO() → update caps, temperature, NMT state    │
   │                                                                          │
```

### 6.2 Motor Move (Master → Slave)

```
HOST (Python/Serial)        MASTER                           SLAVE 0x10
       │                       │                                  │
  {"task":"/motor_act",        │                                  │
   "motor":{"stepperid":10,    │                                  │
            "position":5000,   │                                  │
            "speed":1000}}     │                                  │
       │──────────────────────▶│                                  │
       │              JsonParser parses globalId=10               │
       │              DeviceRouter::motorMove(10, 5000, 1000,     │
       │                                      false, qid)         │
       │              Route: CAN_REMOTE → canNodeId=0x10, axis=0  │
       │                                  │                        │
       │              CanOpenStack::sendMotorMove(0x10, 0, 5000,  │
       │                                          1000, REL, qid) │
       │                                  │                        │
       │                          Frame 1 (RPDO1 → 0x210):        │
       │                          [5000 as i32][1000 as i32] ─────▶
       │                                  │              Store target+speed
       │                          Frame 2 (RPDO2 → 0x310):        │
       │                          [ax=0][cmd=1][lid=0][lpwm=0]    │
       │                          [mode=0][r=0][g=0]        ──────▶
       │                                  │              → FocusMotor::startStepper(0)
       │                          Frame 3 (RPDO3 → 0x410):        │
       │                          [b=0][idx=0][qid][pad]   ───────▶
       │                                  │              Store qid for ack
       │                                  │                        │
       │                                  │              ... motor running ...
       │                                  │                        │
       │                          TPDO1 (0x190) on stop: ◀─────────
       │                          [actualPos][axis=0][run=0][qid] │
       │              CanOpenStack TPDOCallback()                  │
       │              NMTManager::onNodeStatusPDO(0x10, …)        │
       │◀──────────────────────────────────                        │
  {"motor":{"stepperid":10,                                        │
            "position":actualPos,                                  │
            "isDone":true,                                         │
            "qid":qid}}                                            │
```

### 6.3 SDO Read (Master reads slave OD)

```
MASTER                                SLAVE
   │                                     │
   │  SDOHandler::readExpedited(         │
   │    nodeId=0x10,                     │
   │    index=0x2101,  ← motor axis 0   │
   │    sub=0x04,      ← actual_pos     │
   │    buf, 4, &len)                    │
   │                                     │
   │  Build request frame:               │
   │  COB-ID = 0x600 + 0x10 = 0x610     │
   │  data = [0x40][0x01][0x21][0x04]   │
   │         [0x00][0x00][0x00][0x00]   │
   │──────────────────────────────────▶ │
   │                            sdoServerProcessFrame()
   │                            → slaveController::sdoRead(0x2101, 0x04, …)
   │                            → FocusMotor::getCurrentMotorPosition(0)
   │                            → pos = 4321
   │◀────────────────────────────────── │
   │  COB-ID = 0x580 + 0x10 = 0x590    │
   │  data = [0x43][0x01][0x21][0x04]  │  ← cs 0x43 = 4 bytes expedited
   │         [0xE1][0x10][0x00][0x00]  │  ← 4321 LE
   │                                     │
   │  readExpedited() returns true,      │
   │  buf = {0xE1, 0x10, 0x00, 0x00},   │
   │  len = 4  → (int32_t)4321           │
```

### 6.4 Motor Move Completion (Slave → Master)

```
SLAVE 0x10                            MASTER
   │                                     │
   │  SlaveController::loop()            │
   │  ├─ actualPos = FocusMotor::getCurrentMotorPosition(0)
   │  ├─ isRunning = FocusMotor::isRunning(0) → false
   │  └─ wasRunning[0] == true → TRANSITION DETECTED
   │                                     │
   │  CanOpenStack::publishMotorStatus(  │
   │    axis=0, actualPos=5000,          │
   │    isRunning=false, qid=qid)        │
   │                                     │
   │  Build TPDO1:                       │
   │  COB-ID = 0x180 + 0x10 = 0x190     │
   │  data = [5000 LE][axis=0][run=0][qid LE]
   │──────────────────────────────────▶ │
   │                            CAN RX task decodes TPDO1
   │                            → TPDOCallback fired
   │                            → DeviceRouter notified
   │                            → Serial event sent to Host
```

### 6.5 Heartbeat & Offline Detection

```
SLAVE 0x10                            MASTER                    HOST
   │                                     │                        │
   │ (every 1000 ms)                     │                        │
   │ HeartbeatManager::tick()            │                        │
   │ Sends HB: COB-ID=0x710, data=[0x05] │                        │
   │──────────────────────────────────▶ │                        │
   │                            HeartbeatManager::processFrame()  │
   │                            → NMTManager::onHeartbeat(0x10)  │
   │                            → lastHeartbeatMs = now          │
   │                                     │                        │
   │  (slave power lost / disconnected)  │                        │
   │  (no more heartbeats)               │                        │
   │                                     │                        │
   │                            After heartbeatTimeoutMs (3000 ms):
   │                            NMTManager::update()              │
   │                            → online = false                  │
   │                            → NodeStatusCallback(0x10, STOPPED, false)
   │                            → DeviceRouter::onSlaveOffline(0x10)
   │                            → routes 10-19 → UNAVAILABLE     │
   │                                     │──────────────────────▶ │
   │                                    {"event":"can_node_offline",
   │                                     "node":16}               │
```

### 6.6 Slave NMT State Machine

```
              Power-on / esp_restart()
                        │
                        ▼
              ┌─────────────────────┐
              │   INITIALIZING      │  Send boot-up HB (0x700+n, data=0x00)
              │     (0x00)          │  State is transient — auto-advance
              └──────────┬──────────┘
                         │ immediately
                         ▼
              ┌─────────────────────┐
              │  PRE-OPERATIONAL    │  SDO accessible.
              │     (0x7F)          │  PDO blocked.
              │                     │  Heartbeat running.
              └──────────┬──────────┘
          NMT 0x01 ──── ┘ ─────────── NMT 0x02 ─────────────────────┐
                         │                                             │
                         ▼                                             ▼
              ┌─────────────────────┐                    ┌────────────────────┐
              │  OPERATIONAL        │                    │     STOPPED        │
              │     (0x05)          │◀── NMT 0x01 ───── │      (0x04)        │
              │  SDO+PDO active     │                    │  SDO+PDO blocked   │
              │  TPDOs published    │ ── NMT 0x02 ──────▶│                    │
              └─────────┬───────────┘                    └────────────────────┘
                        │
                NMT 0x80 │
                        │
                        ▼
              ┌─────────────────────┐
              │  PRE-OPERATIONAL    │
              └─────────────────────┘
              
              NMT 0x81 (Reset Node) → esp_restart()
              NMT 0x82 (Reset Comm) → deinit/reinit TWAI
```

---

## 7. Build System Integration

### 7.1 Source Files

All CANopen code lives inside `main/src/CANopen/` (protocol engine + slave + wrappers)
and `main/src/DeviceRouter.h/.cpp` (master route table). There is **no separate
`slave/` source directory** — the same `main/main.cpp` entry point is used for both
master and slave builds, controlled by compile flags.

```
main/
├── main.cpp                          ← #ifdef UC2_CANOPEN_ENABLED blocks
├── src/
│   ├── CANopen/
│   │   ├── ObjectDictionary.h        ← OD indices, PDO structs, capability bits
│   │   ├── PDOEngine.h / .cpp        ← encode/decode all 6 PDO types
│   │   ├── SDOHandler.h / .cpp       ← expedited SDO client + server
│   │   ├── NMTManager.h / .cpp       ← NMT state machine, node table
│   │   ├── HeartbeatManager.h / .cpp ← producer + consumer
│   │   ├── CanOpenStack.h / .cpp     ← TWAI init, RX task, frame dispatch
│   │   ├── CanOpenMaster.h / .cpp    ← master setup()/loop() wrapper  [WP4]
│   │   ├── CanOpenSlave.h / .cpp     ← slave setup()/loop() wrapper   [WP5]
│   │   └── SlaveController.h / .cpp  ← OD adapter + device callbacks  [WP5]
│   └── DeviceRouter.h / .cpp         ← global ID → local/CAN routing  [WP4]
└── CMakeLists.txt                    ← conditional source includes
```

### 7.2 CMakeLists.txt

Three CMake `OPTION()` flags control compilation (set via `-D` build flags in
`platformio.ini`, forwarded through `board_build.cmake_extra_args`):

```cmake
# In main/CMakeLists.txt — options section
OPTION(UC2_CANOPEN_ENABLED "UC2 CANopen-Lite protocol stack" OFF)
OPTION(UC2_CANOPEN_MASTER "CANopen master role (DeviceRouter)" OFF)
OPTION(UC2_CANOPEN_SLAVE "CANopen slave role (SlaveController)" OFF)
```

```cmake
# In main/CMakeLists.txt — conditional source includes
if(UC2_CANOPEN_ENABLED EQUAL 1)
    list(APPEND srcs ${mainsrc}src/CANopen/CanOpenStack.cpp)
    list(APPEND srcs ${mainsrc}src/CANopen/PDOEngine.cpp)
    list(APPEND srcs ${mainsrc}src/CANopen/SDOHandler.cpp)
    list(APPEND srcs ${mainsrc}src/CANopen/NMTManager.cpp)
    list(APPEND srcs ${mainsrc}src/CANopen/HeartbeatManager.cpp)
    if(UC2_CANOPEN_MASTER EQUAL 1)
        list(APPEND srcs ${mainsrc}src/DeviceRouter.cpp)
        list(APPEND srcs ${mainsrc}src/CANopen/CanOpenMaster.cpp)
    endif()
    if(UC2_CANOPEN_SLAVE EQUAL 1)
        list(APPEND srcs ${mainsrc}src/CANopen/SlaveController.cpp)
        list(APPEND srcs ${mainsrc}src/CANopen/CanOpenSlave.cpp)
    endif()
endif()
```

### 7.3 main.cpp Wiring

CANopen is wired into `main/main.cpp` with `#ifdef` guards, following the same
pattern as `CAN_BUS_ENABLED`. Both stacks can coexist — the build flags
determine which is compiled.

```cpp
// Includes (after CAN_BUS_ENABLED include)
#ifdef UC2_CANOPEN_ENABLED
#  ifdef UC2_CANOPEN_MASTER
#    include "src/CANopen/CanOpenMaster.h"
#  endif
#  ifdef UC2_CANOPEN_SLAVE
#    include "src/CANopen/CanOpenSlave.h"
#  endif
#endif

// In setup() — after CAN_BUS_ENABLED setup
#ifdef UC2_CANOPEN_ENABLED
#  ifdef UC2_CANOPEN_MASTER
    CanOpenMaster::setup();
#  endif
#  ifdef UC2_CANOPEN_SLAVE
    CanOpenSlave::setup();
#  endif
#endif

// In loop() — after CAN_BUS_ENABLED loop
#ifdef UC2_CANOPEN_ENABLED
#  ifdef UC2_CANOPEN_MASTER
    CanOpenMaster::loop();
    vTaskDelay(1);
#  endif
#  ifdef UC2_CANOPEN_SLAVE
    CanOpenSlave::loop();
    vTaskDelay(1);
#  endif
#endif
```

---

## 8. PlatformIO Environments

### Dedicated CANopen Environments

| Environment | Board | Role | Node-ID | Devices |
|---|---|---|---|---|
| `UC2_4_CANopen_Master` | ESP32-DevKit | Master | `0x01` | Motor+Laser+LED (local+remote) |
| `UC2_4_CANopen_Master_debug` | ESP32-DevKit | Master (debug) | `0x01` | same |
| `UC2_CANopen_Slave_motor` | Seeed XIAO S3 | Slave | `0x10` | 1 motor axis |
| `UC2_CANopen_Slave_motor_debug` | Seeed XIAO S3 | Slave (debug) | `0x10` | 1 motor axis |
| `UC2_CANopen_Slave_multi` | Seeed XIAO S3 | Slave | `0x14` | 1 motor + 2 laser + LED |
| `UC2_CANopen_Slave_multi_debug` | Seeed XIAO S3 | Slave (debug) | `0x14` | 1 motor + 2 laser + LED |

### Existing Environments with CANopen Enabled

The CANopen stack can also be activated in existing CAN bus environments by
adding the appropriate build flags. The following environment now includes
CANopen master support **alongside** the legacy `can_controller`:

| Environment | Added Flags |
|---|---|
| `UC2_3_CAN_HAT_Master_v2` | `-DUC2_CANOPEN_ENABLED=1 -DUC2_CANOPEN_MASTER=1 -DUC2_CAN_TX_PIN=17 -DUC2_CAN_RX_PIN=18 -DUC2_HB_PERIOD_MS=1000` |

> **Note:** Both `CAN_BUS_ENABLED` (legacy ISO-TP/JSON) and `UC2_CANOPEN_ENABLED`
> (CANopen-Lite) can coexist in the same build. They use separate `#ifdef` blocks
> in `main.cpp`. The legacy stack handles OTA and JSON command forwarding; the
> CANopen stack handles PDO/SDO device control.

#### Key build flags

| Flag | Description |
|------|-------------|
| `-DUC2_CANOPEN_ENABLED=1` | Activates CanOpenStack in the firmware |
| `-DUC2_CANOPEN_MASTER=1` | Master role — manages route table, queries slaves |
| `-DUC2_CANOPEN_SLAVE=1` | Slave role — responds to NMT/SDO/PDO from master |
| `-DUC2_NODE_ID=0x10` | Own CAN node-ID (slave only) |
| `-DUC2_CAN_TX_PIN=3` | TWAI TX GPIO |
| `-DUC2_CAN_RX_PIN=2` | TWAI RX GPIO |
| `-DUC2_HB_PERIOD_MS=1000` | Heartbeat interval |
| `-DSLAVE_NUM_MOTORS=1` | Number of motor axes compiled in (slave only) |
| `-DSLAVE_NUM_LASERS=2` | Number of laser channels (slave only) |
| `-DSLAVE_HAS_LED=1` | LED strip present (slave only) |

#### Build commands

```bash
# Master (dedicated CANopen env)
pio run -e UC2_4_CANopen_Master
pio run -e UC2_4_CANopen_Master -t upload

# Master (existing CAN HAT env, now with CANopen)
pio run -e UC2_3_CAN_HAT_Master_v2
pio run -e UC2_3_CAN_HAT_Master_v2 -t upload

# Slave — single motor
pio run -e UC2_CANopen_Slave_motor
pio run -e UC2_CANopen_Slave_motor -t upload

# Slave — motor + laser + LED
pio run -e UC2_CANopen_Slave_multi -t upload
```

---

## 9. Integration Checklist

The wrapper modules `CanOpenMaster` and `CanOpenSlave` handle all wiring
internally. To add CANopen support to an existing environment:

### For a Master Environment

1. Add build flags to `platformio.ini`:
   ```
   -DUC2_CANOPEN_ENABLED=1
   -DUC2_CANOPEN_MASTER=1
   -DUC2_CAN_TX_PIN=<tx_gpio>
   -DUC2_CAN_RX_PIN=<rx_gpio>
   -DUC2_HB_PERIOD_MS=1000
   ```
2. Create a PinConfig directory under `main/config/<ENV_NAME>/` (copy from
   an existing CAN environment if needed).
3. The `#ifdef UC2_CANOPEN_MASTER` block in `main.cpp` will call
   `CanOpenMaster::setup()` and `CanOpenMaster::loop()` automatically.
4. To use the DeviceRouter for command dispatch, replace direct
   `FocusMotor::startStepper()` calls with `DeviceRouter::motorMove()`.

### For a Slave Environment

1. Add build flags to `platformio.ini`:
   ```
   -DUC2_CANOPEN_ENABLED=1
   -DUC2_CANOPEN_SLAVE=1
   -DUC2_NODE_ID=0x10
   -DUC2_CAN_TX_PIN=<tx_gpio>
   -DUC2_CAN_RX_PIN=<rx_gpio>
   -DUC2_HB_PERIOD_MS=1000
   -DSLAVE_NUM_MOTORS=1
   -DSLAVE_NUM_LASERS=0
   -DSLAVE_HAS_LED=0
   ```
2. Include the appropriate device controller flags (`-DMOTOR_CONTROLLER=1`, etc.)
3. Create a PinConfig directory under `main/config/<ENV_NAME>/`.
4. `CanOpenSlave::setup()` and `CanOpenSlave::loop()` are called automatically.

### JsonParser Integration (future)

```cpp
// In JsonParser (for /motor_act):
// Replace direct FocusMotor call with:
DeviceRouter::motorMove(globalId, pos, speed, isAbsolute, qid);
```

**Wiring (500 kbit/s, up to ~20 m bus length):**

```
ESP32 GPIO-17 ─── TX ─┐
                       ├─ SN65HVD230 or TJA1050 transceiver ─── CAN_H ───┬─── (120 Ω termination)
ESP32 GPIO-18 ─── RX ─┘                                     ─── CAN_L ───┘
                                                                    │
                                               Xiao GPIO-3 ─── TX ─┐
                                               Xiao GPIO-2 ─── RX ─┴─ SN65HVD230 ─── CAN_H/L
```

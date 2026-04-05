# UC2-ESP Firmware v2 — Upgrade Plan

**Version 2.0 — March 2026 — openUC2 GmbH**

> Incremental modernization of the [uc2-esp32](https://github.com/youseetoo/uc2-esp32) firmware.
> **Not** a rewrite — targeted upgrades to CAN protocol, serial transport, device addressing, and runtime configuration.

---

## Table of Contents

1. [Design Decisions & Trade-offs](#1-design-decisions--trade-offs)
2. [Node Roles & Topology](#2-node-roles--topology)
3. [Compile Switches vs Runtime Config](#3-compile-switches-vs-runtime-config)
4. [Serial Protocol: Two-Phase ACK with QID](#4-serial-protocol-two-phase-ack-with-qid)
5. [CANopen-Lite Protocol](#5-canopen-lite-protocol)
6. [Capability Registry (EDS-lite)](#6-capability-registry-eds-lite)
7. [Endpoint Design: Keep motor/laser/led Explicit](#7-endpoint-design-keep-motorlaserled-explicit)
8. [Hybrid Routing: Native GPIO vs CAN](#8-hybrid-routing-native-gpio-vs-can)
9. [PS4/Gamepad Controller: Bluetooth + USB-HID via CAN](#9-ps4gamepad-controller-bluetooth--usb-hid-via-can)
10. [Firmware Update over CAN](#10-firmware-update-over-can)
11. [CAN Bus Test Protocols](#11-can-bus-test-protocols)
12. [Work Packages — Detailed Task Lists](#12-work-packages--detailed-task-lists)
13. [Dataflow Diagrams](#13-dataflow-diagrams)

---

## 1. Design Decisions & Trade-offs

### Why CANopen-lite and not full CANopen?

Full CiA 301/402 requires dynamic PDO mapping via EDS files, LSS for node-ID assignment, SYNC/TIME producers, emergency objects, and substantial state management. The ESP32-C3 (Xiao) has only 400 KB SRAM.

**Our approach:** cherry-pick the concepts that solve real problems — CAN-ID allocation scheme, PDO/SDO split, NMT state machine, heartbeat — while keeping fixed PDO mappings and static node-ID assignment (NVS/EEPROM). We build on the working prototype at [`youseetoo/uc2_canopen_mwe`](https://github.com/youseetoo/uc2_canopen_mwe) and extend it beyond just motors to cover lasers, LEDs, sensors, heaters, etc.

**Importantly:** we still use standard CANopen CAN-ID allocation so that off-the-shelf CANopen tools (e.g. `python-canopen`, commercial CAN analyzers) can decode our traffic. If a third-party device speaks CANopen, it can join the bus. This keeps the door open to full compliance later without locking us in now.

### Why COBS framing for serial?

COBS has deterministic 1-byte overhead per 254 payload bytes, clean frame boundaries (0x00 delimiter), and is well-tested in embedded contexts. This replaces the current newline-delimited JSON which breaks on any embedded newline or binary data.

### Why MessagePack and not protobuf?

MsgPack is self-describing (like JSON but binary), needs no schema compilation step, has a tiny C library (`mpack` ≈ 30 KB flash), and maps 1:1 to JSON keys — making the dual-mode parser trivial. For our typical 8–200 byte payloads the difference vs protobuf is negligible.

### What about WiFi?

WiFi stays available behind its existing `#ifdef WIFI_CONTROLLER` guard but is **not** included in default builds. No code removal needed — just leave it disabled in `platformio.ini`. This frees ~80 KB heap for CAN queues and OD tables in production builds while preserving the option for development/debug use.

---

## 2. Node Roles & Topology

```
┌─────────────────────────────┐
│  HOST  (Raspberry Pi)       │
│                             │
│  Python (UC2-REST / ImSwitch│
│  / µManager)                │
│                             │
│  Digital Twin State Cache   │
│  Workflow Orchestrator      │
│  Camera Acquisition         │
└────────────┬────────────────┘
             │ USB-Serial (JSON or MsgPack, COBS-framed)
             │
┌────────────▼────────────────────────────────────────────┐
│  MASTER / HYBRID  ESP32  (CAN-HAT on Pi)                │
│                                                          │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────┐  │
│  │ Serial Parser│→ │ Device Router│→ │ CAN Gateway   │  │
│  │ JSON+MsgPack │  │ (route flag) │  │ TWAI + PDO/SDO│  │
│  └──────────────┘  └──────┬───────┘  └───────┬───────┘  │
│                           │ GPIO              │ CAN bus  │
│              ┌────────────▼───────┐           │          │
│              │ Native Controllers │           │          │
│              │ FastAccelStepper   │           │          │
│              │ PWM Laser          │           │          │
│              │ LED strip          │           │          │
│              └────────────────────┘           │          │
│  ┌─────────────────┐  ┌──────────────────┐   │          │
│  │ Heartbeat Mgr   │  │ NMT-lite State M │   │          │
│  │ Capability Reg.  │  │ OD (local slots) │   │          │
│  └─────────────────┘  └──────────────────┘   │          │
└──────────────────────────────────────────────┬──────────┘
                                               │
                    ┌──────────────────────────┬┘
                    │ CAN bus (TWAI, 500 kbit) │
          ┌────────▼────────┐       ┌─────────▼───────┐
          │ SLAVE A (Xiao)  │       │ SLAVE B (Xiao)  │
          │ Motor X,Y,Z     │       │ Laser 1,2       │
          │ Endstops         │       │ LED array       │
          │ OD + PDO/SDO    │       │ Temp sensor     │
          │ Heartbeat prod.  │       │ Heater          │
          │ NMT-lite         │       │ OD + PDO/SDO   │
          └─────────────────┘       └─────────────────┘
```

| Role | Description | Communication |
|------|-------------|---------------|
| **Host** | Raspberry Pi running Python. Maintains digital twin. Sends commands, receives state. | USB-Serial to Master (JSON or MsgPack, COBS-framed) |
| **Master (Hybrid)** | ESP32 HAT on Pi. Parses serial, routes to GPIO or CAN. Runs its own device controllers for GPIO-wired hardware. Acts as CAN gateway. | USB-Serial ↑ Host, CAN bus ↓ Slaves, GPIO → local actuators |
| **Slave** | ESP32 (Xiao C3/S3) with CAN transceiver. Runs subset of firmware with own Object Dictionary. | CAN bus ↑ Master |

---

## 3. Compile Switches vs Runtime Config

### The Problem

The current firmware uses `#define` flags in `platformio.ini` (e.g. `-DMOTOR_CONTROLLER=1`, `-DLASER_CONTROLLER=1`) to include/exclude entire controllers at compile time. This means:
- Any change to enabled features requires recompilation and reflashing
- The WebSerial flashing tool needs separate binary builds per board variant
- Code readability suffers from `#ifdef` nesting everywhere
- Bug surface area increases because each combination is effectively a different firmware

### Recommendation: Hybrid approach (don't remove compile switches entirely)

**Keep compile switches for hardware-level differences:**
- Board pin mapping (`PinConfig` structs) — these are physically different
- Chip-family differences (ESP32 vs ESP32-C3 vs ESP32-S3)  
- Major peripheral availability (has CAN transceiver? has USB-OTG for HID host?)
- Libraries that add significant flash/RAM (e.g. Bluetooth stack ≈ 200 KB, NeoPixel DMA)

**Move to NVS runtime config for feature enable/disable:**
- Which controllers are active (motor, laser, LED, DAC, heater, encoder, etc.)
- Number of motors, number of lasers, pin assignments
- CAN node ID, CAN bitrate
- Joystick speed multipliers, default illumination values
- Serial baud rate, MsgPack vs JSON default

**How it works in practice:**
```cpp
// At boot, read NVS config (with compiled-in defaults as fallback)
struct RuntimeConfig {
    bool motor_enabled;       // default from compile flag, overridable via NVS
    bool laser_enabled;
    bool led_enabled;
    uint8_t motor_count;      // 0-4
    uint8_t laser_count;      // 0-3
    uint8_t can_node_id;      // 0x00 = master
    uint32_t can_bitrate;     // 500000
    // ... etc
};

// ModuleController reads this at startup instead of #ifdefs:
void ModuleController::init() {
    RuntimeConfig cfg = NVS::loadConfig();  // falls back to compiled defaults
    if (cfg.motor_enabled) modules.push_back(new MotorController(cfg));
    if (cfg.laser_enabled) modules.push_back(new LaserController(cfg));
    // ...
}
```

**The key benefit:** you compile ONE binary per board family (e.g. "ESP32-CAN-HAT", "Xiao-S3-Slave"), then configure features at runtime via a serial command:

```json
{"task": "/config_set", "config": {"motor_enabled": true, "motor_count": 3, "can_node_id": 16}}
```

After writing to NVS, a reboot activates the new config. The WebSerial flasher already does something similar for CAN ID — this extends the pattern.

**What you gain:**
- Far fewer binary variants to build and maintain
- Users can enable/disable features without PlatformIO
- The code becomes cleaner: `#ifdef` blocks shrink to just the hardware-level stuff
- Board-specific `platformio.ini` environments shrink to pin mappings + chip family

**What you keep:**
- Compile switches for things that genuinely save significant flash/RAM (Bluetooth stack, WiFi, USB-HID host)
- Separate PlatformIO environments for genuinely different hardware (ESP32 vs C3 vs S3)

---

## 4. Serial Protocol: Two-Phase ACK with QID

### Current Problem (from `feature-canpushing` branch)

The existing firmware has inconsistent acknowledgment: sometimes you get a response when the command is parsed, sometimes when it completes, and for CAN-forwarded commands there's often no response at all. The `qid` field exists but isn't used consistently.

### Proposed Protocol: ACK + DONE

Every command from the host carries a `qid` (query ID). The firmware sends exactly **two** responses per command:

```
Host → Master:
  {"task":"/motor_act", "qid": 42, "motor": {"steppers":[{"stepperid":0, "position":10000}]}}

Master → Host (immediate, <5ms):
  {"qid": 42, "status": "ACK"}
  Meaning: "I received and parsed the command, it's valid, execution has started"

Master → Host (when complete):
  {"qid": 42, "status": "DONE", "motor": {"steppers":[{"stepperid":0, "position":10000, "isbusy":false}]}}
  Meaning: "The motor reached position (or error occurred)"

Error case:
  {"qid": 42, "status": "ERROR", "error": "MOTOR_STALL", "detail": "stepper 0 stalled at position 4320"}
```

### Rules

1. **ACK** is sent as soon as the command is parsed and dispatched (before execution begins). For CAN-routed commands, ACK means "the CAN frame was transmitted successfully."
2. **DONE** is sent when the physical action completes (motor reached target, laser set, etc.). For CAN-routed commands, DONE comes when the slave's TPDO confirms completion.
3. **ERROR** replaces DONE if something goes wrong (timeout, stall, invalid parameter, CAN slave not responding).
4. If the command is invalid JSON or unknown task, only an ERROR is sent (no ACK).
5. The `qid` is a `uint32_t` chosen by the host. The firmware echoes it verbatim.
6. Host-side UC2-REST library: `send_command()` returns a Future/Promise that resolves on DONE and has a configurable timeout.

### Sequence for CAN-routed commands

```
Host              Master                  CAN Slave
  │                  │                        │
  │──motor_act qid42─▶│                        │
  │                  │ parse OK               │
  │◀──ACK qid42──────│                        │
  │                  │──RPDO1 (motor cmd)────▶│
  │                  │                        │ execute...
  │                  │                        │ motor moving...
  │                  │◀──TPDO1 (pos reached)──│
  │◀──DONE qid42─────│                        │
  │                  │                        │
```

### Sequence for native GPIO commands

```
Host              Master
  │                  │
  │──laser_act qid43─▶│
  │                  │ parse OK
  │◀──ACK qid43──────│
  │                  │ set PWM pin
  │◀──DONE qid43─────│  (immediate for laser — PWM is instant)
  │                  │
```

---

## 5. CANopen-Lite Protocol

### CAN-ID Allocation (standard CANopen scheme)

| Function Code | Description | CAN-ID Range | Direction |
|--------------|-------------|--------------|-----------|
| 0x0 | NMT Command | 0x000 | Master → All |
| 0x1 | Heartbeat / Boot-up | 0x700 + node_id | Slave → Master |
| 0x3 | TPDO1 — Motor status | 0x180 + node_id | Slave → Master |
| 0x4 | RPDO1 — Motor command | 0x200 + node_id | Master → Slave |
| 0x5 | TPDO2 — Laser/LED status | 0x280 + node_id | Slave → Master |
| 0x6 | RPDO2 — Laser/LED command | 0x300 + node_id | Master → Slave |
| 0x7 | TPDO3 — Sensor/encoder data | 0x380 + node_id | Slave → Master |
| 0x8 | RPDO3 — DAC/heater command | 0x400 + node_id | Master → Slave |
| 0xB | SDO Request (config) | 0x600 + node_id | Master → Slave |
| 0xB | SDO Response | 0x580 + node_id | Slave → Master |
| — | Firmware Update (special) | 0x7E0 + node_id | Master → Slave |

### PDO Payloads (fixed mapping, ≤ 8 bytes each)

**RPDO1 — Motor command (Master → Slave):**
```
Byte 0:    sub_index      (u8)   — which motor on this node (0-3)
Byte 1:    flags           (u8)   — bit0:isabs, bit1:isaccel, bit2:isenable, bit3:ishome
Byte 2-5:  target_position (i32)  — steps, little-endian
Byte 6-7:  speed           (u16)  — steps/sec
```

**TPDO1 — Motor status (Slave → Master):**
```
Byte 0:    sub_index      (u8)
Byte 1:    status          (u8)   — 0:idle, 1:moving, 2:homing, 3:stalled, 4:fault
Byte 2-5:  actual_position (i32)
Byte 6-7:  actual_speed    (u16)
```

**RPDO2 — Laser/LED command (Master → Slave):**
```
Byte 0:    sub_index      (u8)   — which laser/LED (0-3)
Byte 1:    device_type     (u8)   — 0:laser, 1:LED_single, 2:LED_array
Byte 2-3:  value           (u16)  — intensity (0-4095) or LED index
Byte 4-5:  value2          (u16)  — for LED array: color or pattern ID
Byte 6-7:  reserved        (u16)
```

**TPDO2 — Laser/LED status (Slave → Master):**
```
Byte 0:    sub_index      (u8)
Byte 1:    device_type     (u8)
Byte 2-3:  actual_value    (u16)
Byte 4-5:  temperature     (u16)  — if sensor available, else 0xFFFF
Byte 6-7:  reserved
```

### Why this fixes the bandwidth problem

Current firmware sends `{"task":"/motor_act","motor":{"steppers":[{"stepperid":1,"position":10000,"speed":5000}]}}` (≈ 90 bytes) over CAN using ISO-TP fragmentation — **12+ CAN frames** for one motor move.

With PDO: **1 CAN frame, 8 bytes.** That's a 12× reduction.

SDO is only used for configuration (set microstep ratio, PID gains, pin mapping, read firmware version) — rare, not latency-critical, can use ISO-TP segmentation when needed.

### NMT State Machine (per node)

```
                    ┌──────────────┐
                    │ INITIALIZING │
                    └──────┬───────┘
                           │ boot complete
                    ┌──────▼───────┐
           ┌────────│PRE-OPERATIONAL│◀───────────────┐
           │        │ SDO active    │                 │
           │        │ PDO disabled  │                 │
           │        └──────┬───────┘                 │
           │               │ NMT start               │ NMT reset
           │        ┌──────▼───────┐                 │
           │        │ OPERATIONAL  │                 │
           │        │ SDO + PDO    │─────────────────┤
           │        └──────┬───────┘   NMT stop      │
           │               │                   ┌─────▼─────┐
           │               │ error             │  STOPPED   │
           │        ┌──────▼───────┐           │ HB only    │
           │        │    FAULT     │           └───────────┘
           └────────│              │
    NMT reset       └──────────────┘
```

---

## 6. Capability Registry (EDS-lite)

### The Problem

Currently there's no way to ask "what can this device do?" You need to know in advance which `platformio.ini` was used to compile the firmware.

### Solution: Queryable capability table per node

Each node (Master and every Slave) maintains a **capability table** — a simplified version of a CANopen EDS (Electronic Data Sheet). It's stored as a compact C struct in flash and returned via SDO or serial query.

```json
// Query:
{"task": "/capabilities_get"}

// Response from Master (includes local + all discovered CAN slaves):
{
  "qid": 1,
  "status": "DONE",
  "nodes": [
    {
      "node_id": 0,
      "role": "master",
      "firmware_version": "2.1.0",
      "firmware_hash": "a3f7c2e1",
      "board": "CAN-HAT-V2",
      "devices": [
        {"type": "motor", "count": 2, "driver": "FastAccelStepper", "features": ["homing", "acceleration"]},
        {"type": "laser", "count": 2, "driver": "PWM", "features": ["analog_modulation"]},
        {"type": "led_array", "count": 1, "driver": "NeoPixel", "features": ["rgb", "pattern"]},
        {"type": "dac", "count": 1, "driver": "ESP32-DAC", "features": []},
        {"type": "digital_in", "count": 4, "driver": "GPIO", "features": ["interrupt"]}
      ],
      "interfaces": ["serial_json", "serial_msgpack", "bluetooth_hid", "can_gateway"],
      "nmt_state": "OPERATIONAL"
    },
    {
      "node_id": 16,
      "role": "slave",
      "firmware_version": "2.1.0",
      "board": "Xiao-S3-Motor",
      "devices": [
        {"type": "motor", "count": 3, "driver": "FastAccelStepper", "features": ["homing", "acceleration", "tmc_uart"]},
        {"type": "encoder", "count": 3, "driver": "ESP32Encoder", "features": []},
        {"type": "digital_in", "count": 3, "driver": "GPIO", "features": ["endstop"]}
      ],
      "interfaces": ["can"],
      "nmt_state": "OPERATIONAL"
    },
    {
      "node_id": 32,
      "role": "slave",
      "firmware_version": "2.1.0",
      "board": "Xiao-C3-Laser",
      "devices": [
        {"type": "laser", "count": 2, "driver": "PWM", "features": ["analog_modulation"]},
        {"type": "temperature", "count": 1, "driver": "DS18B20", "features": []}
      ],
      "interfaces": ["can"],
      "nmt_state": "OPERATIONAL"
    }
  ]
}
```

### How it's stored on the device

```cpp
// In flash (const), auto-generated at compile time from PinConfig + ModuleConfig
struct DeviceCapability {
    uint8_t  type;         // DeviceType enum
    uint8_t  count;        // how many of this device
    uint8_t  driver;       // DriverType enum
    uint16_t features;     // bitmask of features
};

struct NodeCapabilities {
    uint8_t  node_id;
    uint8_t  role;         // MASTER, SLAVE
    uint32_t fw_version;   // packed: major.minor.patch
    uint32_t fw_hash;      // git short hash
    uint8_t  board_id;     // enum of known board types
    uint8_t  device_count;
    DeviceCapability devices[MAX_DEVICES_PER_NODE];  // max 16
};
```

On the CAN side: the Master queries each slave's capabilities via SDO read of OD index `0x1000` (device type) and `0x2F00-0x2FFF` (device capability table). This happens once at boot.

---

## 7. Endpoint Design: Keep motor/laser/led Explicit

### Decision: No generic `/dev_act` — keep domain-specific endpoints

The REST-style endpoints (`/motor_act`, `/motor_get`, `/motor_set`, `/laser_act`, `/led_act`, etc.) stay as-is. They're human-readable, self-documenting, and match how users think about the hardware.

### What changes: routing flag + unified numbering

Add an optional `route` field and use a **global enumeration** that includes node context:

```json
// Current (still works — backward compatible, assumes local GPIO):
{"task": "/motor_act", "motor": {"steppers": [{"stepperid": 0, "position": 10000}]}}

// New: explicit routing with global stepper ID
{"task": "/motor_act", "motor": {"steppers": [
  {"stepperid": 0, "position": 10000},
  {"stepperid": 3, "position": 5000, "route": "can", "node_id": 16}
]}}

// Alternative: use a numbering scheme where ID encodes routing
// IDs 0-9: local GPIO motors
// IDs 10-19: CAN node 0x10 (node_id=16) motors
// IDs 20-29: CAN node 0x14 (node_id=20) motors
// etc.
// This way the existing "stepperid" field is enough:
{"task": "/motor_act", "motor": {"steppers": [
  {"stepperid": 0, "position": 10000},
  {"stepperid": 10, "position": 5000}
]}}
// stepperid=10 → Master knows: node_id=16, sub_index=0
```

### Enumeration scheme

```
Global ID = (node_id / 0x10) * 10 + sub_index

Examples:
  Local GPIO (node_id=0):    motor 0, 1, 2, 3
  CAN node 0x10 (16):        motor 10, 11, 12, 13
  CAN node 0x14 (20):        motor 20, 21, 22, 23
  CAN node 0x20 (32):        laser 20, 21, 22, 23

Same scheme for lasers, LEDs, etc. independently:
  laser 0, 1     = local GPIO
  laser 10, 11   = CAN node 0x10
  led_array 0    = local NeoPixel
  led_array 10   = CAN node 0x10 NeoPixel
```

The Device Router maintains a lookup table mapping global IDs → `(node_id, sub_index, route_type)`.

---

## 8. Hybrid Routing: Native GPIO vs CAN

### The core challenge

Some devices are wired directly to the Master's GPIO pins (the "hybrid" case), while others sit on CAN slaves. The user shouldn't need to care — the same `/motor_act` command works for both.

### Device Router internals

```cpp
enum RouteType { ROUTE_GPIO, ROUTE_CAN };

struct DeviceRoute {
    uint8_t  global_id;     // the stepperid/laserid the user sends
    RouteType route;
    uint8_t  node_id;       // 0x00 for GPIO, 0x10+ for CAN
    uint8_t  sub_index;     // device index on that node
    Module*  local_ctrl;    // pointer to local controller (if GPIO)
};

class DeviceRouter {
    std::vector<DeviceRoute> motor_routes;
    std::vector<DeviceRoute> laser_routes;
    std::vector<DeviceRoute> led_routes;
    // ...

    void dispatch_motor(int stepperid, MotorCommand cmd, uint32_t qid) {
        DeviceRoute& r = lookup_motor(stepperid);
        if (r.route == ROUTE_GPIO) {
            // Call local FastAccelStepper directly
            r.local_ctrl->act(cmd);
            send_ack(qid);
            // Register completion callback → send_done(qid)
        } else {
            // Encode into RPDO1, transmit via CAN
            can_gateway.send_rpdo1(r.node_id, r.sub_index, cmd);
            send_ack(qid);
            // Register TPDO1 callback for this node → send_done(qid)
        }
    }
};
```

### Multi-device commands

When a command targets multiple devices (e.g. move X+Y+Z simultaneously):
- If all are GPIO: dispatch all, then wait for all completions, send single DONE
- If all are CAN on same node: send one RPDO per motor (they execute simultaneously on the slave), wait for TPDOs, send DONE
- If mixed (some GPIO, some CAN): dispatch all in parallel, collect completions, send DONE when all finish
- Timeout: if any device doesn't complete within `timeout_ms`, send ERROR with partial status

---

## 9. PS4/Gamepad Controller: Bluetooth + USB-HID via CAN

### Keep Bluetooth PS4 (unchanged)

The existing `BtController` with PS4 HID stays as-is behind the `#ifdef BTHID` compile flag. It works, it's useful for demos and manual stage control.

### New: USB-HID Gamepad via dedicated ESP32-S3

The uploaded `hid_host_joystick.ino` shows how an ESP32-S3 can act as a USB host for HID devices (PS4 DualShock, flight sticks, generic gamepads). The idea:

**Architecture:**
```
┌───────────────┐     ┌──────────────────────────┐     ┌──────────┐
│ USB Gamepad   │────▶│ ESP32-S3 (USB-HID Host)  │────▶│ CAN Bus  │
│ (PS4/Joystick)│ USB │ Reads HID reports        │ CAN │          │
└───────────────┘     │ Maps to motor/laser cmds  │     └─────┬────┘
                      │ Sends as RPDOs or to      │           │
                      │ Master for routing        │     ┌─────▼────┐
                      └──────────────────────────┘     │ Master   │
                                                       │ or Slaves│
                                                       └──────────┘
```

The ESP32-S3 HID-Host node:
- Runs the HID host stack (from uploaded code)
- Detects gamepad type (PS4 DualShock, Thrustmaster, Logitech, generic)
- Maps axes → motor velocity, buttons → laser on/off, triggers → LED intensity
- Sends mapped commands either:
  - As CAN RPDOs directly to motor/laser slaves (low latency path)
  - As CAN frames to the Master for routing (if mixed GPIO/CAN targets)
- Has its own CAN node ID (e.g. `0x40`) and shows up in the capability registry as a "gamepad input" device

**This replaces the need for Bluetooth on the Master,** freeing RAM and avoiding the BT/WiFi coexistence issues on ESP32. But the Bluetooth path stays for backward compatibility.

### Mapping config (stored in NVS on the HID-Host node)

```json
{
  "gamepad_map": {
    "left_stick_x": {"target": "motor", "id": 0, "scale": 100, "deadzone": 10},
    "left_stick_y": {"target": "motor", "id": 1, "scale": 100, "deadzone": 10},
    "right_stick_y": {"target": "motor", "id": 2, "scale": 50, "deadzone": 15},
    "L2_trigger": {"target": "laser", "id": 0, "scale": 4095},
    "R2_trigger": {"target": "laser", "id": 1, "scale": 4095},
    "cross_button": {"target": "led_array", "id": 0, "action": "toggle"},
    "dpad_up": {"target": "motor", "id": 2, "action": "jog", "steps": 100}
  }
}
```

---

## 10. Firmware Update over CAN

### Current state

The existing firmware supports CAN-based firmware updates using ISO-TP to transfer the binary. This works but is slow and has no integrity verification.

### Improved approach (CANopen-inspired)

Use SDO block transfer (CiA 301 §7.2.4.3.6) for firmware updates — it's designed for large transfers with CRC checking:

**Protocol:**
1. Master sends NMT command: put target slave in PRE-OPERATIONAL
2. Master initiates SDO block download to OD index `0x1F50` (standard CANopen "Program Data" index)
3. Transfer firmware binary in 127-frame blocks with CRC-16 per block
4. On completion: SDO write to `0x1F51` sub 1 (Program Control: start/verify)
5. Slave verifies CRC-32 of full image, writes to OTA partition, reboots
6. After reboot: slave sends NMT boot-up message with new firmware version

**Advantages over current approach:**
- Block transfer is ~3× faster than segmented SDO (less ACK overhead)
- Per-block CRC catches corruption early (no need to retransmit entire image)
- Uses standard CANopen OD indices that `python-canopen` library understands
- The Master can still be updated via USB-Serial (standard ESP-IDF OTA or esptool)

**Python-side (UC2-REST):**
```python
# Firmware update via CAN
uc2.can_update_firmware(node_id=0x10, firmware_path="slave_motor_v2.1.0.bin")
# Internally: reads binary, sends SDO block transfer, monitors progress, verifies
```

---

## 11. CAN Bus Test Protocols

### Built-in self-test commands

```json
// Test 1: CAN bus loopback on Master (no slaves needed)
{"task": "/can_test", "test": "loopback"}
// → Master puts TWAI in loopback mode, sends 10 frames, verifies reception
// Response: {"qid":..., "status":"DONE", "test":"loopback", "sent":10, "received":10, "errors":0}

// Test 2: Ping all CAN slaves
{"task": "/can_test", "test": "ping_all"}
// → Master sends NMT node-guarding request to each known node_id
// Response: {"qid":..., "status":"DONE", "test":"ping_all",
//   "nodes": [
//     {"node_id": 16, "status": "OK", "latency_us": 340, "fw_version": "2.1.0"},
//     {"node_id": 32, "status": "OK", "latency_us": 420, "fw_version": "2.1.0"},
//     {"node_id": 48, "status": "TIMEOUT"}
//   ]}

// Test 3: Ping specific slave
{"task": "/can_test", "test": "ping", "node_id": 16}
// Response: {"qid":..., "status":"DONE", "node_id":16, "latency_us":340, "nmt_state":"OPERATIONAL"}

// Test 4: Stress test (flood test)
{"task": "/can_test", "test": "stress", "node_id": 16, "frames": 1000}
// → Send 1000 RPDO/TPDO round-trips, measure latency statistics
// Response: {"qid":..., "status":"DONE", "sent":1000, "received":998, "lost":2,
//   "latency_avg_us":420, "latency_max_us":1200, "latency_min_us":280}

// Test 5: Bus error counters
{"task": "/can_test", "test": "status"}
// → Read TWAI driver error counters
// Response: {"qid":..., "tx_error_count":0, "rx_error_count":0, "bus_off":false,
//   "arb_lost":0, "bus_errors":0, "state":"RUNNING"}
```

### Slave-side self-test

Each slave also supports a test SDO at OD index `0x2FF0`:
- Write `0x01`: run GPIO self-test (toggle all outputs, read back)
- Write `0x02`: run motor self-test (move 10 steps, verify encoder if present)
- Write `0x03`: run full diagnostic (all of the above + temperature read)
- Read: returns last test result as packed struct

### Heartbeat-based health monitoring

- Each slave sends a heartbeat every `heartbeat_interval_ms` (default: 500 ms)
- Master monitors all slaves; if 3 consecutive heartbeats are missed → slave marked as OFFLINE
- Master pushes status change to Host via serial: `{"event": "node_offline", "node_id": 16}`
- When slave comes back: `{"event": "node_online", "node_id": 16, "nmt_state": "PRE_OPERATIONAL"}`

---

## 12. Work Packages — Detailed Task Lists

### WP1: Runtime Configuration System (NVS-based)

**Goal:** Replace most `#ifdef` compile switches with NVS runtime configuration so that one binary per board family covers all feature combinations without recompilation.

**Priority:** HIGH | **Phase:** 1 | **Effort:** ~1.5 weeks | **Risk:** Medium

**What the programmer does:**

1. **Define `RuntimeConfig` struct** in new file `RuntimeConfig.h`:
   - Fields for every enable/disable flag currently in `ModuleConfig.h`
   - Fields for motor count, laser count, pin overrides, CAN node ID, CAN bitrate
   - Each field has a compile-time default (so existing `#define` values become fallbacks)

2. **Create `NVSConfig` class** in `NVSConfig.h/.cpp`:
   - `load()`: read config from NVS namespace "uc2cfg", fill defaults for missing keys
   - `save(RuntimeConfig&)`: write to NVS
   - `reset()`: erase NVS namespace, reboot with defaults

3. **Modify `ModuleController::init()`**:
   - Instead of `#ifdef MOTOR_CONTROLLER`, check `config.motor_enabled`
   - Keep `#ifdef` only for chip-family and peripheral-availability guards
   - Module instantiation driven by `RuntimeConfig` values

4. **Add serial commands**:
   - `/config_get`: return current RuntimeConfig as JSON
   - `/config_set`: accept partial JSON, merge with current config, save to NVS
   - `/config_reset`: reset NVS to compiled defaults

5. **Update `platformio.ini`**: reduce build environments to ~3-4 (ESP32-HAT, Xiao-S3-Slave, Xiao-C3-Slave, ESP32-Standalone) instead of the current ~15+

6. **Test**: flash one binary, reconfigure via serial to enable/disable motors/lasers/LEDs, verify startup behavior changes correctly after reboot

**Files touched:** New `RuntimeConfig.h`, `NVSConfig.h/.cpp`. Modify `ModuleController.h/.cpp`, `ModuleConfig.h`, `platformio.ini`.


how shall we deal with the pinconfigurations that currently is handled with the python-based file for copying them at compile time? should we provide them also via the config_set and announce it via a string that then loads it from some internal header? I don't want to provide all the details (e.g. all pin configs) over serial

23:03

Architected runtime board profile selection via internal config identifiers
Architected runtime board profile selection via internal config identifiers
Great question — this is one of those design decisions where the current approach (Python script copies a header at compile time) works but creates unnecessary binary proliferation. Let me think through the trade-offs and propose a clean solution.

The core tension is: pin configs are static hardware facts (a UC2_3 board always has Motor X on GPIO 33), but the current system forces a full recompile to switch between them. Meanwhile, sending 40+ pin assignments over serial is tedious and error-prone.

The answer: compile ALL known board profiles into the binary, select by name at runtime.

Here's the concrete design:

cpp
// PinProfiles.h — ALL known boards compiled into every binary

struct PinProfile {
    const char* name;           // "UC2_2", "UC2_3", "CAN_HAT_V2", "XIAO_S3_MOTOR", ...
    const char* description;    // "UC2 Board v3 with TCA9535 I/O expander"
    
    // Motors
    int8_t motor_step[4];       // {GPIO_NUM_2, GPIO_NUM_27, GPIO_NUM_12, GPIO_NUM_22}
    int8_t motor_dir[4];        // {GPIO_NUM_33, GPIO_NUM_16, GPIO_NUM_14, GPIO_NUM_21}
    int8_t motor_enable[4];     // {GPIO_NUM_13, GPIO_NUM_13, GPIO_NUM_13, GPIO_NUM_13}
    bool   motor_en_inverted[4];
    
    // Lasers
    int8_t laser_pin[3];
    
    // LEDs
    int8_t led_pin;
    int16_t led_count;
    
    // I2C
    int8_t i2c_scl;
    int8_t i2c_sda;
    uint8_t i2c_tca_addr;
    
    // CAN
    int8_t can_tx;
    int8_t can_rx;
    
    // Endstops
    int8_t endstop[4];
    
    // DAC
    int8_t dac_pin[2];
    
    // ... etc
};

// ---- All known boards ----

static const PinProfile PROFILE_UC2_2 = {
    .name = "UC2_2",
    .description = "UC2 Board v2 (ESP32 WROOM, 4 motors, 3 lasers)",
    .motor_step = {2, 27, 12, 22},
    .motor_dir  = {33, 16, 14, 21},
    .motor_enable = {13, 13, 13, 13},
    .motor_en_inverted = {true, true, true, true},
    .laser_pin = {4, 15, -1},
    .led_pin = 17, .led_count = 25,
    .i2c_scl = 22, .i2c_sda = 21, .i2c_tca_addr = 0,
    .can_tx = -1, .can_rx = -1,  // no CAN on UC2_2
    // ...
};

static const PinProfile PROFILE_UC2_3 = {
    .name = "UC2_3",
    .description = "UC2 Board v3 (ESP32, TCA9535 I/O expander, 4 motors)",
    .motor_step = {2, 27, 12, 22},
    .motor_dir  = {33, 16, 14, 21},
    // ...
    .i2c_tca_addr = 0x27,
    // ...
};

static const PinProfile PROFILE_CAN_HAT_V2 = {
    .name = "CAN_HAT_V2",
    .description = "CAN HAT Master v2 (ESP32, 2 local motors, CAN gateway)",
    .motor_step = {2, 27, -1, -1},  // only 2 local motors
    .can_tx = 5, .can_rx = 4,
    // ...
};

static const PinProfile PROFILE_XIAO_S3_MOTOR = {
    .name = "XIAO_S3_MOTOR",
    .description = "Seeed Xiao ESP32-S3 motor slave (3 motors, CAN)",
    .motor_step = {1, 3, 5, -1},
    .motor_dir  = {2, 4, 6, -1},
    .can_tx = 43, .can_rx = 44,
    // ...
};

// Registry of all profiles
static const PinProfile* ALL_PROFILES[] = {
    &PROFILE_UC2_2,
    &PROFILE_UC2_3,
    &PROFILE_CAN_HAT_V2,
    &PROFILE_XIAO_S3_MOTOR,
    // ... add new boards here
};
static const int NUM_PROFILES = sizeof(ALL_PROFILES) / sizeof(ALL_PROFILES[0]);
The runtime selection is then trivially simple:

json
// Query available profiles:
{"task": "/pinconfig_list"}
// Response:
{"profiles": ["UC2_2", "UC2_3", "CAN_HAT_V2", "XIAO_S3_MOTOR", "XIAO_C3_MOTOR", "CNC_SHIELD_V3"]}

// Select a profile (saves to NVS, takes effect on reboot):
{"task": "/config_set", "config": {"board": "CAN_HAT_V2"}}

// Read what's active:
{"task": "/pinconfig_get"}
// Response: full pin layout of the active profile + any overrides
For custom boards or one-off pin changes, allow individual overrides stored in NVS on top of the base profile:

json
// Override just laser pin 2 (everything else stays from CAN_HAT_V2 profile):
{"task": "/pinconfig_override", "overrides": {"laser_pin_2": 25}}

// Clear all overrides (revert to clean profile):
{"task": "/pinconfig_override", "overrides": null}
```

**How the boot sequence works:**
```
1. Read NVS key "board" → e.g. "CAN_HAT_V2"
2. Find matching PinProfile in compiled registry
3. If not found → use compile-time default (from platformio.ini board_build.cmake_extra_args)
4. Read NVS key "pin_overrides" → apply any individual pin changes on top
5. Pass final PinConfig to ModuleController for initialization
What this costs in flash: each PinProfile struct is roughly 120–160 bytes. With 15 known boards, that's about 2.4 KB of flash — completely negligible on an ESP32 with 4 MB flash. The Python copy script and the per-board build environments go away.

What stays in platformio.ini: only the chip family and default board as a compile-time fallback:

ini
[env:ESP32_HAT]
board = esp32dev
build_flags = -DDEFAULT_BOARD=\"CAN_HAT_V2\"

[env:XIAO_S3]
board = seeed_xiao_esp32s3
build_flags = -DDEFAULT_BOARD=\"XIAO_S3_MOTOR\"
That's 2–4 environments instead of 15+. The Python copy script is eliminated entirely.

---

### WP2: Two-Phase ACK Serial Protocol + MsgPack

**Goal:** Add COBS framing, MsgPack encoding, and two-phase ACK/DONE response tracking to the serial interface.

**Priority:** HIGH | **Phase:** 1 | **Effort:** ~2 weeks | **Risk:** Medium

**What the programmer does:**

1. **Add COBS library** (e.g. `PacketSerial` or lightweight C implementation from https://github.com/jacquesf/COBS-c):
   - Frame format: `0x00 | HEADER | COBS(payload) | 0x00`
   - Header byte: `0x01` = JSON UTF-8, `0x02` = MsgPack binary

2. **Add MsgPack library** (`mpack` — https://github.com/ludocode/mpack, fits ESP32 well):
   - `MsgPackParser.h/.cpp`: decode MsgPack payload into same internal command struct as JSON parser
   - `MsgPackSerializer.h/.cpp`: encode response structs as MsgPack

3. **Create `SerialTransport` layer** (`SerialTransport.h/.cpp`):
   - Replaces direct `Serial.readStringUntil('\n')` in `SerialProcess`
   - Reads COBS frames from serial, dispatches based on header byte
   - Both decoders produce same `Command` struct → downstream pipeline unchanged
   - Response encoder: replies in same format as request (JSON→JSON, MsgPack→MsgPack)
   - **Backward compatibility:** if no COBS framing detected (no 0x00 delimiters), fall back to legacy newline-delimited JSON

4. **Implement two-phase QID tracking**:
   - `CommandTracker` class: maps `qid` → `{state, timestamp, callback}`
   - On parse success: immediately send ACK with qid
   - On completion (motor done, laser set, CAN TPDO received): send DONE with qid + result data
   - On timeout (configurable per device type): send ERROR with qid
   - For fire-and-forget commands (e.g. `/state_get`): ACK+DONE can be merged into single response

5. **Integrate with `feature-canpushing` branch**: merge the QID tracking improvements from that branch, adapt to the two-phase scheme

6. **Update UC2-REST Python library**:
   - Add COBS framing to `serial_transport.py`
   - Add MsgPack mode (default), JSON fallback via `format="json"` param
   - `send_command()` returns awaitable that resolves on DONE, times out with error
   - Auto-detect firmware version: if old firmware (no COBS), fall back to legacy protocol

**Files touched:** New `SerialTransport.h/.cpp`, `MsgPackParser.h/.cpp`, `MsgPackSerializer.h/.cpp`, `CommandTracker.h/.cpp`. Modify `SerialProcess.cpp`, `main.cpp`.

---

### WP3: CANopen-Lite Protocol Engine

**Goal:** Replace JSON-over-ISO-TP with PDO/SDO-based CAN communication using standard CANopen CAN-ID allocation. Build on the [`uc2_canopen_mwe`](https://github.com/youseetoo/uc2_canopen_mwe) prototype.

We can get inspiration from the project in /Users/bene/Downloads/uc2_canopen_mwe 

**Priority:** HIGH | **Phase:** 1 | **Effort:** ~3-4 weeks | **Risk:** High

**What the programmer does:**

1. **Port and extend `uc2_canopen_mwe`** into the main firmware as a library:
   - Extract the working motor CANopen code into a clean `CANopen/` directory
   - Extend beyond motors: add PDO definitions for laser, LED, DAC, sensor, heater, encoder
   - Keep the MWE's TWAI driver setup but add configurable bitrate

2. **Define Object Dictionary schema** (`ObjectDictionary.h`):
   - Standard entries: `0x1000` (device type), `0x1001` (error register), `0x1017` (heartbeat time), `0x1018` (identity object)
   - Manufacturer-specific: `0x2000-0x2FFF` for device parameters (see §5 above)
   - Capability table at `0x2F00-0x2FFF`
   - Firmware update at `0x1F50-0x1F51`
   - OD is a flat C array of `OD_Entry` structs with pointers to live controller state

3. **Implement PDO engine** (`PDOEngine.h/.cpp`):
   - Fixed mappings (not runtime-configurable like full CANopen — saves complexity)
   - `encode_rpdo(node_id, pdo_num, data)` → CAN frame
   - `decode_tpdo(can_frame)` → typed struct
   - Event-driven TPDO: slaves send TPDO only on state change (not periodic, to save bandwidth)
   - Support for all device types: motor, laser, LED, DAC, heater, encoder, temperature, digital I/O

4. **Implement SDO handler** (`SDOHandler.h/.cpp`):
   - Expedited transfer (≤ 4 bytes): single CAN frame request/response
   - Segmented transfer (> 4 bytes): multi-frame with toggle bit
   - Block transfer: for firmware update (large payloads with CRC)
   - Timeout + retry (3 attempts, configurable timeout per transfer type)

5. **Implement NMT manager** (`NMTManager.h/.cpp`):
   - Master side: send NMT commands (start, stop, pre-operational, reset), track slave states
   - Slave side: receive NMT, transition state machine, control PDO enable/disable

6. **Implement Heartbeat** (`HeartbeatManager.h/.cpp`):
   - Producer (slave): send heartbeat frame at configurable interval (OD `0x1017`)
   - Consumer (master): monitor all known slaves, detect offline/online transitions
   - Push node status changes to Host via serial

7. **Replace old `CanController`**: 
   - Remove `can_act`/`can_get` JSON-forwarding code
   - The Device Router (WP4) replaces its function

8. **Keep ISO-TP** only for SDO segmented/block transfers (rare)

**Files touched:** New `CANopen/` directory with `ObjectDictionary.h`, `PDOEngine.h/.cpp`, `SDOHandler.h/.cpp`, `NMTManager.h/.cpp`, `HeartbeatManager.h/.cpp`. Replace `CanController.h/.cpp`.

---

### WP4: Device Router & Hybrid Integration

**Goal:** Make the Device Router transparently handle GPIO-local vs CAN-remote commands using the enumeration scheme from §7.

**Priority:** HIGH | **Phase:** 2 | **Effort:** ~2 weeks | **Risk:** Medium

**What the programmer does:**

1. **Create `DeviceRouter` class** (`DeviceRouter.h/.cpp`):
   - Maintains route tables: `motor_routes[]`, `laser_routes[]`, `led_routes[]`, etc.
   - Route table is built at boot from: local `RuntimeConfig` (GPIO devices) + CAN slave capability queries
   - `dispatch_motor(global_id, cmd, qid)`: lookup → GPIO or CAN → appropriate handler
   - `dispatch_laser(global_id, cmd, qid)`: same pattern

2. **Implement enumeration scheme**:
   - Map `stepperid` 0-9 → local GPIO (from `RuntimeConfig`)
   - Map `stepperid` 10-19 → CAN node 0x10 sub 0-9
   - Map `stepperid` 20-29 → CAN node 0x14 sub 0-9
   - Same for all device types independently
   - Add `/route_table_get` command to dump the full mapping for debugging

3. **Modify existing controllers** (`MotorController`, `LaserController`, `LEDController`, etc.):
   - They no longer handle routing — they just execute for local GPIO devices
   - The JSON parser calls `DeviceRouter::dispatch_X()` instead of calling controllers directly

4. **Multi-device coordination**:
   - When `/motor_act` has multiple `steppers` entries: dispatch all, collect completions
   - Send single DONE when all complete (or ERROR if any fails/times out)
   - For time-critical scans (`stagescan`, `focusscan`): if motors are mixed GPIO+CAN, warn the user about potential sync issues

5. **Handle device hot-plug**:
   - When a new CAN slave announces via NMT boot-up, query its capabilities, add to route table
   - When a slave goes offline (heartbeat timeout), mark its routes as UNAVAILABLE
   - Report changes to Host via serial events

**Files touched:** New `DeviceRouter.h/.cpp`. Modify `MotorController`, `LaserController`, `LEDController`, `JsonParser.cpp` (dispatch path).

---

### WP5: Slave Firmware Template

**Goal:** Create a clean, minimal slave firmware that implements CANopen-lite, is configurable via NVS, and supports all device types.

**Priority:** MEDIUM | **Phase:** 2 | **Effort:** ~2-3 weeks | **Risk:** Medium

**What the programmer does:**

1. **Create `env:UC2_CAN_SLAVE` PlatformIO environment**:
   - Targets: Xiao ESP32-S3, Xiao ESP32-C3, ESP32 WROOM (with CAN transceiver)
   - Minimal dependencies: TWAI driver, FastAccelStepper (if motor), NeoPixel (if LED)
   - No WiFi, no Bluetooth, no HTTP — maximum free heap for OD + CAN queues

2. **Implement slave main loop**:
   ```
   boot → load NVS config → init TWAI → init device controllers →
   send NMT boot-up → enter PRE-OPERATIONAL →
   wait for NMT start → enter OPERATIONAL →
   loop: process CAN RX → update OD → send TPDOs on change → heartbeat
   ```

3. **Device controller abstraction**:
   - Each device type (motor, laser, LED, heater, encoder, temperature) is a module
   - Enabled/disabled via NVS `RuntimeConfig` (same struct as Master, subset used)
   - Module writes its state into the OD; PDO engine reads from OD to build frames

4. **SDO server**: handle config reads/writes (microstep, PID gains, pin mapping, etc.)

5. **NMT responder**: transition state machine on NMT commands from Master

6. **Firmware update receiver**: handle SDO block transfer to OD `0x1F50`, write to OTA partition

7. **Self-test handler**: SDO at `0x2FF0` — run GPIO test, motor test, diagnostic

8. **Test on all target boards**: verify CAN communication, motor control, laser PWM, LED patterns, heartbeat timing

**Files touched:** New `slave/` directory with its own `main.cpp`, shared `CANopen/` library, shared controller code.

---

### WP6: USB-HID Gamepad CAN Node

**Goal:** Create a standalone ESP32-S3 node that reads USB gamepads (PS4, joystick) and injects motor/laser commands onto the CAN bus.

**Priority:** MEDIUM | **Phase:** 2 | **Effort:** ~1.5 weeks | **Risk:** Low

**What the programmer does:**

1. **Create `env:UC2_HID_GAMEPAD` PlatformIO environment** for ESP32-S3:
   - Based on uploaded `hid_host_joystick.ino` and `hid_host.c`/`.h`
   - Add CAN (TWAI) driver alongside USB host

2. **Implement gamepad mapper**:
   - Detect connected HID device type (PS4 DualShock, Thrustmaster, Logitech, generic)
   - Parse HID reports using the device-specific structs from the uploaded code
   - Map axes/buttons to motor/laser/LED commands according to NVS config
   - Configurable dead zones, scaling, inversion per axis

3. **CAN output**:
   - Send mapped commands as RPDOs (for direct slave control) or special gamepad TPDO (for Master routing)
   - Node ID configurable (default `0x40`)
   - Heartbeat producer for health monitoring
   - NMT responder

4. **LED feedback**: optional NeoPixel on the HID-host board showing connection status, battery level, mapping mode

5. **Configuration via serial** (USB-CDC on S3): set mapping, calibrate axes, test buttons

**Files touched:** New `hid_gamepad/` directory, reuses `CANopen/` library.

---

### WP7: CAN Firmware Update (SDO Block Transfer)

**Goal:** Implement reliable firmware update over CAN using CANopen SDO block transfer protocol.

**Priority:** MEDIUM | **Phase:** 2 | **Effort:** ~1.5 weeks | **Risk:** Medium

**What the programmer does:**

1. **Implement SDO block transfer** in `SDOHandler.cpp`:
   - Block download (Master → Slave): send 127 CAN frames per block, CRC-16 per block
   - Block upload (Slave → Master): for reading back firmware version/hash
   - Fallback to segmented transfer if block transfer fails

2. **Implement firmware receiver** on slave (`OTAReceiver.h/.cpp`):
   - Listen for SDO writes to OD index `0x1F50` sub 1 (Program Data)
   - Buffer incoming data, write to OTA partition in chunks
   - On transfer complete: verify CRC-32 of full image
   - On SDO write to `0x1F51` sub 1 with value `0x01`: activate OTA, reboot

3. **Implement firmware sender** on Master (`CANFirmwareUpdater.h/.cpp`):
   - Serial command: `{"task": "/can_fw_update", "node_id": 16, "file": "firmware.bin"}`
   - Read binary from SPIFFS/LittleFS (uploaded via serial) or receive chunks from Host via serial
   - Execute SDO block transfer, report progress to Host
   - Wait for slave reboot, verify new firmware version via capability query

4. **Python-side (UC2-REST)**:
   - `uc2.update_slave_firmware(node_id=0x10, binary_path="slave_v2.1.0.bin")`
   - Streams binary to Master via serial, Master forwards via CAN
   - Progress callback for UI integration

**Files touched:** Extend `SDOHandler.cpp`, new `OTAReceiver.h/.cpp` (slave), `CANFirmwareUpdater.h/.cpp` (master).

---

### WP8: Capability Registry & Discovery

**Goal:** Implement queryable capability table so the Host can discover what every node offers.

**Priority:** MEDIUM | **Phase:** 2 | **Effort:** ~1 week | **Risk:** Low

**What the programmer does:**

1. **Define capability data structures** (`NodeCapabilities.h`):
   - `DeviceCapability` struct: type, count, driver, features bitmask
   - `NodeCapabilities` struct: node_id, role, firmware info, device list
   - Auto-generate from `RuntimeConfig` at boot

2. **Implement serial endpoint** `/capabilities_get`:
   - Returns JSON with full node tree (Master + all discovered slaves)
   - Slave capabilities are cached from boot-time SDO queries

3. **Implement CAN-side capability exchange**:
   - On slave boot-up: Master queries OD `0x2F00-0x2FFF` via SDO to get capability table
   - Cache in Master's memory; invalidate and re-query on slave reboot

4. **Integrate with UC2-REST Python library**:
   - `uc2.get_capabilities()` → returns structured Python dict
   - Auto-discovery on connect: Python driver knows what's available
   - Generate human-readable summary for debugging

**Files touched:** New `NodeCapabilities.h`, modify `CANopen/NMTManager.cpp` (discovery), modify serial parser.

---

### WP9: Integration Testing, Documentation & Migration

**Goal:** End-to-end validation, CAN test protocols, documentation, migration guide.

**Priority:** HIGH | **Phase:** 3 | **Effort:** ~2 weeks | **Risk:** Low

**What the programmer does:**

1. **Implement CAN test commands** (from §11):
   - `/can_test` with modes: `loopback`, `ping_all`, `ping`, `stress`, `status`
   - Slave-side self-test via SDO at `0x2FF0`

2. **Test matrix** (manual, document results):
   - Master-only (all GPIO, no CAN slaves)
   - Master + 1 motor slave
   - Master + 3 slaves (motor, laser, mixed)
   - Hybrid (some devices GPIO, some CAN)
   - Gamepad node + Master + slaves
   - Firmware update over CAN (with CRC verification)
   - Legacy JSON backward compatibility (old Python scripts still work)

3. **Performance benchmarks**:
   - Motor command latency: JSON-ISO-TP vs PDO (measure round-trip)
   - Serial throughput: JSON vs MsgPack for bulk motor/laser commands
   - CAN bus utilization during stage scanning at max speed

4. **Documentation**:
   - Update `README.md` with new architecture overview
   - Update `DOC_Firmware.md` with new serial protocol, CAN protocol, endpoints
   - Create `MIGRATION.md`: step-by-step guide for users upgrading from v1
   - Create `CAN_PROTOCOL.md`: full PDO/SDO reference
   - Create `DEVELOPER.md`: how to add a new device type (create controller, add OD entries, add PDO mapping)

5. **WebSerial flasher update**:
   - Update board definitions for new binary variants
   - Add runtime config panel (send `/config_set` after flash)
   - Add CAN test panel (run `/can_test` commands)

**Files touched:** Test scripts, documentation files, WebSerial flasher HTML/JS.

---

## 13. Dataflow Diagrams

### Diagram 1: Complete Command Flow — Host to Actuator

```
                    ┌─────────────────────────────────────────────────────────┐
                    │                    HOST (Raspberry Pi)                  │
                    │                                                         │
                    │  Python sends:                                          │
                    │  {"task":"/motor_act", "qid":42,                       │
                    │   "motor":{"steppers":[{"stepperid":10,"position":5000}]}}│
                    └────────────────────────┬────────────────────────────────┘
                                             │
                              ┌──────────────▼──────────────┐
                              │ USB Serial (COBS-framed)    │
                              │ Header: 0x01 (JSON)         │
                              │    or   0x02 (MsgPack)      │
                              └──────────────┬──────────────┘
                                             │
┌────────────────────────────────────────────▼───────────────────────────────────────┐
│                         MASTER ESP32 (CAN-HAT)                                    │
│                                                                                    │
│  ┌──────────────────────┐                                                          │
│  │ 1. SerialTransport   │ Detect COBS frame, check header byte                    │
│  │    COBS decode       │ Decode JSON or MsgPack                                  │
│  │    JSON/MsgPack parse│ Produce internal Command struct                         │
│  └──────────┬───────────┘                                                          │
│             │                                                                      │
│  ┌──────────▼───────────┐                                                          │
│  │ 2. CommandTracker    │ Register qid=42                                          │
│  │    Send ACK to Host  │──────▶ {"qid":42,"status":"ACK"}                        │
│  └──────────┬───────────┘                                                          │
│             │                                                                      │
│  ┌──────────▼───────────┐                                                          │
│  │ 3. JSON Parser       │ Route: task="/motor_act"                                │
│  │    Extract endpoint  │ Extract: stepperid=10, position=5000                    │
│  └──────────┬───────────┘                                                          │
│             │                                                                      │
│  ┌──────────▼───────────────────────────────────────────────────┐                  │
│  │ 4. DeviceRouter.dispatch_motor(global_id=10, cmd, qid=42)   │                  │
│  │                                                               │                  │
│  │    Lookup: stepperid=10 → node_id=0x10, sub=0, route=CAN    │                  │
│  │                                                               │                  │
│  │    ┌───────────┐ stepperid 0-9     ┌────────────┐            │                  │
│  │    │ if GPIO:  │──────────────────▶│ Local Ctrl │            │                  │
│  │    │ route=GPIO│                   │ FastAccel  │            │                  │
│  │    └───────────┘                   └─────┬──────┘            │                  │
│  │                                          │ (complete)        │                  │
│  │    ┌───────────┐ stepperid 10+     ┌─────▼──────┐            │                  │
│  │    │ if CAN:   │──────────────────▶│ CAN Gwy    │            │                  │
│  │    │ route=CAN │                   │ RPDO1 build│            │                  │
│  │    └───────────┘                   └─────┬──────┘            │                  │
│  └──────────────────────────────────────────┼───────────────────┘                  │
│                                             │                                      │
└─────────────────────────────────────────────┼──────────────────────────────────────┘
                                              │
                               ┌──────────────▼──────────────┐
                               │ CAN Bus Frame               │
                               │ ID: 0x210 (RPDO1 + node 16) │
                               │ Data: [sub=0, flags=0x01,   │
                               │   pos=5000(LE), spd=1000]   │
                               │ Total: 1 frame, 8 bytes     │
                               └──────────────┬──────────────┘
                                              │
┌─────────────────────────────────────────────▼──────────────────────────────────────┐
│                         SLAVE ESP32 (CAN node 0x10)                                │
│                                                                                    │
│  ┌──────────────────────┐                                                          │
│  │ 5. CAN RX Handler    │ Decode RPDO1: sub=0, pos=5000, speed=1000              │
│  │    Write to OD       │ Update OD[0x2000].target_position = 5000               │
│  └──────────┬───────────┘                                                          │
│             │                                                                      │
│  ┌──────────▼───────────┐                                                          │
│  │ 6. Motor Controller  │ FastAccelStepper.moveTo(5000)                           │
│  │    Execute command   │ ... motor moving ...                                    │
│  └──────────┬───────────┘                                                          │
│             │ (target reached)                                                     │
│  ┌──────────▼───────────┐                                                          │
│  │ 7. Update OD         │ OD[0x2001].actual_position = 5000                       │
│  │    Send TPDO1        │ OD[0x2004].status = IDLE                                │
│  └──────────┬───────────┘                                                          │
│             │                                                                      │
└─────────────┼──────────────────────────────────────────────────────────────────────┘
              │
              ▼
┌──────────────────────────────┐
│ CAN Bus Frame                │
│ ID: 0x190 (TPDO1 + node 16) │
│ Data: [sub=0, status=IDLE,   │
│   pos=5000(LE), spd=0]      │
└──────────────┬───────────────┘
               │
┌──────────────▼───────────────────────────────────────────────────────────────────┐
│                         MASTER ESP32                                             │
│  ┌──────────────────────┐                                                        │
│  │ 8. CAN RX: TPDO1    │ Decode: node=0x10 motor done                          │
│  │    Match to qid=42  │ Lookup pending command tracker                         │
│  │    Send DONE to Host│──────▶ {"qid":42,"status":"DONE",                      │
│  └──────────────────────┘        "motor":{"steppers":[{"stepperid":10,           │
│                                   "position":5000,"isbusy":false}]}}             │
└──────────────────────────────────────────────────────────────────────────────────┘
```

### Diagram 2: Native GPIO Command (Laser — immediate)

```
Host                          Master
  │                              │
  │──/laser_act qid=43 id=0────▶│
  │  val=2048                    │
  │                              │ Parse OK
  │◀──ACK qid=43────────────────│
  │                              │ DeviceRouter: id=0 → GPIO
  │                              │ LaserController: set PWM ch0 = 2048
  │                              │ (instant — no motor-like wait)
  │◀──DONE qid=43───────────────│
  │  {"laser":{"id":0,           │
  │   "value":2048}}             │
  │                              │
```

### Diagram 3: Multi-Motor Coordinated Move (Mixed GPIO + CAN)

```
Host                    Master                     Slave 0x10
  │                        │                           │
  │──/motor_act qid=50────▶│                           │
  │  steppers: [           │                           │
  │   {id:0, pos:1000},   │ Parse OK                  │
  │   {id:10, pos:2000}]  │                           │
  │                        │                           │
  │◀──ACK qid=50──────────│                           │
  │                        │                           │
  │                        │ DeviceRouter:             │
  │                        │  id=0 → GPIO (local)     │
  │                        │  id=10 → CAN node 0x10   │
  │                        │                           │
  │                        │──RPDO1 (node16,sub0)────▶│
  │                        │  pos=2000                 │ execute...
  │                        │                           │
  │                        │ Local: FastAccel(0)       │
  │                        │  moveTo(1000)             │
  │                        │  ... motor 0 moving ...   │
  │                        │                           │ ... motor moving ...
  │                        │                           │
  │                        │ motor 0 done              │
  │                        │ (1 of 2 complete)         │
  │                        │                           │
  │                        │◀──TPDO1 (pos reached)────│
  │                        │ motor 10 done             │
  │                        │ (2 of 2 complete)         │
  │                        │                           │
  │◀──DONE qid=50─────────│                           │
  │  both motors report    │                           │
  │  final positions       │                           │
  │                        │                           │
```

### Diagram 4: Gamepad to Motor (CAN-only path)

```
┌──────────┐    USB     ┌─────────────────┐    CAN     ┌──────────┐
│ PS4      │───────────▶│ ESP32-S3        │───────────▶│ Motor    │
│ DualShock│ HID reports│ HID-Host Node   │ RPDO1      │ Slave    │
│          │            │ node_id=0x40    │            │ node=0x10│
└──────────┘            │                 │            └──────────┘
                        │ Map:            │
                        │ leftY → motor 0 │
                        │ rightY → motor 2│
                        │ L2 → laser 0    │
                        └─────────────────┘

Every ~20ms (50 Hz):
  HID report received → parse DS4 struct → apply deadzone/scale →
  if axis changed: send RPDO to target motor/laser node

No Master involvement needed for CAN-only targets.
For GPIO targets on Master: send special gamepad TPDO → Master routes to GPIO.
```

### Diagram 5: Firmware Update over CAN

```
Host (Python)                Master                    Slave 0x10
  │                            │                          │
  │──/can_fw_update node=16──▶│                          │
  │  (binary streamed)        │                          │
  │                            │──NMT: PRE-OPERATIONAL──▶│
  │                            │                          │ disable PDOs
  │                            │                          │
  │◀──ACK (upload starting)───│                          │
  │                            │                          │
  │  [binary chunk 1]────────▶│──SDO block download─────▶│
  │  [binary chunk 2]────────▶│  (127 frames/block)      │ write to OTA
  │  [binary chunk 3]────────▶│  CRC-16 per block        │ partition
  │  ...                       │  ...                     │
  │  [final chunk]────────────▶│──SDO block final────────▶│
  │                            │                          │ verify CRC-32
  │                            │──SDO write 0x1F51=0x01──▶│
  │                            │                          │ activate + reboot
  │                            │                          │
  │                            │  (3 sec timeout)         │
  │                            │                          │
  │                            │◀──NMT boot-up───────────│ (new firmware)
  │                            │                          │
  │                            │──SDO read 0x1018────────▶│ (verify version)
  │                            │◀──"2.1.0"───────────────│
  │                            │                          │
  │◀──DONE (update OK)────────│                          │
  │  new_version: "2.1.0"     │                          │
```

---

## Phase Summary & Timeline

| Phase | Work Packages | Duration | What Ships |
|-------|--------------|----------|------------|
| **Phase 1** | WP1 (NVS Config), WP2 (Serial+MsgPack+QID), WP3 (CANopen-lite) | 6-8 weeks | Runtime config, dual serial, CANopen core. Legacy JSON still works. |
| **Phase 2** | WP4 (Device Router), WP5 (Slave Template), WP6 (Gamepad Node), WP7 (CAN FW Update), WP8 (Capabilities) | 6-8 weeks | Full hybrid routing, slave firmware, gamepad, OTA, discovery. |
| **Phase 3** | WP9 (Testing + Docs) | 2-3 weeks | Validated, documented, migration guide, WebSerial updates. |

**Total estimated effort: 14-19 weeks** for one experienced firmware developer familiar with ESP32 + CAN.

---

## Summary of Key Differences from v1 Plan

| Item | v1 Plan | v2 Plan (this document) |
|------|---------|-------------------------|
| Compile switches | Remove most | **Keep for hardware; move feature flags to NVS runtime** |
| Endpoints | New generic `/dev_act` | **Keep `/motor_act`, `/laser_act` etc.** with routing flag |
| Numbering | `(node, type, sub)` tuples | **Global ID scheme**: 0-9 local, 10-19 CAN node 16, etc. |
| WiFi removal | Remove code | **Keep behind #ifdef, just disable in default builds** |
| PS4/Bluetooth | Remove | **Keep Bluetooth PS4 + add new USB-HID gamepad CAN node** |
| CANopen | Lite subset | **Lite subset but with full CAN-ID compliance** for interop |
| ACK model | Single response | **Two-phase: ACK (received) + DONE (complete)** with QID |
| FW update | "smarter" ISO-TP | **SDO block transfer** with per-block CRC |
| Capabilities | Device table dump | **Full queryable capability registry** (EDS-lite) |
| CAN testing | Not specified | **Built-in test suite**: loopback, ping, stress, self-test |

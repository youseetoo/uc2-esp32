# UC2 CANopen Network Architecture

How the Raspberry Pi (running **ImSwitch**) drives the UC2 microscope over a
shared **CANopen** bus, the two physical ways the Pi reaches that bus, and the
two control regimes — **StageScan / performance mode** (the HAT owns
time-critical timing) and **sequential / normal mode** (ImSwitch owns per-frame
timing).

Source of truth for the labels below:

- Firmware: `main/src/canopen/` (`DeviceRouter`, `RoutingTable`, `CANopenModule`),
  `main/src/motor/StageScan.cpp`, `main/src/bt/` (PS4), `main/src/gpio_can/`,
  `main/src/config/RuntimeConfig.h`, `main/src/canopen/UC2_OD_Indices.h`.
- Host: ImSwitch `ExperimentController.py` + `experiment_controller/experiment_performance_mode.py`,
  `positioners/ESP32StageManager.py`.

---

## 1. System topology — two ways onto one CAN bus

Everything ultimately talks on **one shared CANopen bus**. The Pi can reach it
**(A) indirectly** through the ESP32 CAN-HAT acting as a serial→CAN gateway, or
**(B) directly** through an **MCP2515** controller on the HAT wired to the Pi's
own SPI/GPIO. Both the ESP32 (via its native TWAI controller) and the MCP2515
sit on the *same* CANH/CANL as every slave.

```mermaid
flowchart TB
    subgraph HOST["🖥️ Raspberry Pi — Host"]
        IMSW["ImSwitch backend · ExperimentController<br/>FastAPI REST API"]
        REST["UC2-REST · serial JSON client"]
        PYCAN["python-canopen · socketcan (can0)"]
        IMSW --> REST
        IMSW --> PYCAN
    end

    subgraph HAT["🎩 ESP32 CAN-HAT — NodeRole::CAN_MASTER"]
        SER["SerialProcess<br/>JSON command queue"]
        ROUTER["DeviceRouter + RoutingTable<br/>LOCAL · REMOTE · OFF"]
        STAGE["StageScan engine<br/>(time-critical, on MCU)"]
        BT["BtController / HidController → MotorGamePad<br/>PS4 Bluetooth (BT-classic HID)"]
        LOCALIO["Local GPIO drivers<br/>camera-trigger out · native axes"]
        CONODE["CANopenNode stack · TWAI<br/>NMT master · SDO client · TPDO/RPDO"]
        SER --> ROUTER
        BT --> ROUTER
        ROUTER --> STAGE
        ROUTER --> LOCALIO
        ROUTER --> CONODE
        STAGE --> CONODE
        STAGE --> LOCALIO
    end

    MCP["🔌 MCP2515 CAN controller<br/>on HAT · wired to Pi SPI + INT-GPIO"]
    CANBUS{{"🚌 CANopen bus · 1 Mbit/s — shared CANH/CANL backbone"}}

    subgraph SLAVES["🛰️ CAN slaves — NodeRole::CAN_SLAVE"]
        M1["Motor driver(s)<br/>OD 0x2000 · step/dir · endstops"]
        L1["Laser TTL driver(s)<br/>OD 0x2100 · PWM / TTL"]
        LED1["LED matrix / illumination<br/>OD 0x2200 · NeoPixel"]
        G1["GPIO trigger / safety<br/>OD 0x2300·0x2310 · E-stop, collision ADC"]
        GAL["Galvo / scanner<br/>OD 0x2600"]
        JOY["PS4 USB-host bridge node<br/>OD 0x2400 · joystick (optional)"]
    end

    REST  ==>|"Path A — USB-Serial (JSON tasks)"| SER
    PYCAN ==>|"Path B — SPI (direct CANopen)"| MCP

    CONODE --- CANBUS
    MCP --- CANBUS
    CANBUS --- M1
    CANBUS --- L1
    CANBUS --- LED1
    CANBUS --- G1
    CANBUS --- GAL
    CANBUS --- JOY

    classDef host fill:#e3f2fd,stroke:#1565c0,color:#0d47a1;
    classDef hat  fill:#fff3e0,stroke:#e65100,color:#e65100;
    classDef bus  fill:#e8f5e9,stroke:#2e7d32,color:#1b5e20;
    classDef slv  fill:#f3e5f5,stroke:#6a1b9a,color:#4a148c;
    class HOST host
    class HAT hat
    class SLAVES slv
    class CANBUS bus
    class MCP bus
```

**Path A — serial gateway (default).** The HAT is the CANopen master.
`UC2-REST` sends JSON tasks (`/motor_act`, `/laser_act`, …) over USB-serial;
`DeviceRouter` decides per logical device whether to run it on the HAT's own
GPIO (`LOCAL`) or forward it to a slave as a CANopen **SDO** (`REMOTE`).

**Path B — direct MCP2515 (Pi is CANopen master).** The Pi drives `can0`
through the MCP2515 with `python-canopen`, issuing SDO/PDO/NMT itself. The
ESP32's serial bridge is bypassed for device I/O; the HAT can still be powered
for PS4 Bluetooth and local-only jobs. Off-the-shelf CANopen tooling
(`python-canopen`, analyzers) decodes the same traffic.

---

## 2. Input / output interfaces — hardware & software

```mermaid
flowchart LR
    subgraph SW["💾 Software interfaces"]
        direction TB
        EP["REST endpoints (FastAPI)<br/>/ExperimentController/startWellplateExperiment<br/>startFastStageScanAcquisition · stopExperiment<br/>home_all_axes · getWorkflowStatus"]
        TASKS["Serial JSON tasks<br/>/motor_act · /motor_get · /laser_act<br/>/led_act · /home_act · /digitalout_act<br/>/state_act · /ota_start · /route_set·/route_get"]
        OBJ["CANopen objects<br/>SDO read/write (commands+config)<br/>TPDO/RPDO (fast cached state)<br/>NMT + heartbeat (lifecycle)"]
        EP --> TASKS --> OBJ
    end

    subgraph HW["🔧 Hardware interfaces"]
        direction TB
        PiHW["Pi: USB host · SPI0 + INT GPIO (MCP2515)"]
        HatHW["HAT/ESP32: USB-device (serial) · TWAI CANH/CANL<br/>BT 2.4 GHz radio (PS4) · GPIO"]
        TrigHW["Outputs: CAMERA_TRIGGER_PIN pulse → camera<br/>step/dir → drivers · laser PWM/TTL · LED data"]
        InHW["Inputs: endstops · E-stop · collision ADC · joystick"]
        PiHW --> HatHW --> TrigHW
        HatHW --> InHW
    end

    SW -. "commands & config (down)" .-> HW
    HW -. "state, frames, events (up)" .-> SW

    classDef sw fill:#e1f5fe,stroke:#0277bd,color:#01579b;
    classDef hw fill:#fbe9e7,stroke:#d84315,color:#bf360c;
    class SW sw
    class HW hw
```

| Direction | Software (host ↔ firmware) | Hardware |
|---|---|---|
| **Host → device (commands)** | REST call → JSON task over serial → SDO write to OD index | USB-serial bytes; SPI frames to MCP2515; CAN frames on bus |
| **Device → host (state)** | TPDO/heartbeat → `RemoteSlaveState` cache → `/motor_get`, async notifications | CAN frames; endstop/E-stop levels; collision ADC |
| **Trigger / acquisition** | hardware trigger (fast path) **or** software `++{"cam":1}--` over serial | `CAMERA_TRIGGER_PIN` GPIO pulse → camera; camera USB/CSI → ImSwitch |
| **Manual control** | — | PS4 → BT-HID → `MotorGamePad`; or PS4 → USB-host bridge → CAN joystick node |

---

## 3. StageScan / performance mode — the HAT owns timing

`performanceMode=True` and hardware triggering. ImSwitch sends the scan
**parameters once**; the ESP32 `StageScan` engine then runs the whole grid on
the MCU — moving motors, settling, switching illumination, and pulsing the
camera trigger with deterministic timing. ImSwitch only listens to the camera
and writes frames (OME-Zarr/TIFF). Per-frame LED-matrix patterns (DPC) cannot
ride this fast path and force a fallback to mode §4.

```mermaid
sequenceDiagram
    autonumber
    participant U as ImSwitch (host)
    participant S as Serial bridge
    participant H as ESP32 HAT · StageScan
    participant B as CAN bus
    participant SL as Motor / Laser / LED slaves
    participant C as Camera

    U->>S: startWellplateExperiment(performanceMode=True)
    U->>S: motor_act · stage-scan params<br/>(xStart, xStep, nX/nY/nZ, illum, speed, accel)
    S->>H: one JSON command → StageScanningData
    Note over H: HAT takes over time-critical control

    loop for every grid point (on MCU)
        H->>H: move axis (LOCAL GPIO)
        H-->>B: or SDO 0x2000 target/speed (REMOTE)
        B-->>SL: move motor slave
        H->>H: settle (delayTimePreTrigger)
        H->>SL: set laser/LED (LOCAL PWM / SDO 0x2100·0x2200)
        H-)C: CAMERA_TRIGGER_PIN pulse (hardware)
        C-->>U: frame (USB/CSI)
        U->>U: writer thread → OME-Zarr/TIFF
        H->>SL: illumination off
    end

    H->>S: {"qid":N,"state":"done"}
    S->>U: scan complete
    Note over U,C: ImSwitch only listens + stores frames
```

---

## 4. Sequential / normal mode — ImSwitch owns timing

`performanceMode=False`. The `WorkflowManager` walks the tile/Z/channel list
**one step at a time over serial** (software trigger). The host commands each
move, sets illumination, snaps and saves a frame, then turns illumination off —
full per-frame control, at the cost of serial round-trip latency. Each device
command is still routed `LOCAL` or `REMOTE` by `DeviceRouter` on the HAT.

```mermaid
sequenceDiagram
    autonumber
    participant U as ImSwitch · WorkflowManager (host)
    participant S as Serial bridge
    participant H as ESP32 HAT · DeviceRouter
    participant B as CAN bus
    participant SL as Motor / Laser / LED slaves
    participant C as Camera

    U->>S: startWellplateExperiment(performanceMode=False)
    Note over U: build workflowSteps (per tile · Z · channel)

    loop every workflow step (host-paced)
        U->>S: /motor_act move (abs)
        S->>H: route MOTOR id
        alt LOCAL axis
            H->>H: drive step/dir GPIO
        else REMOTE axis
            H-->>B: SDO 0x2000 pos/speed → wait
            B-->>SL: move motor slave
        end
        H->>S: {"qid":N,"state":"done"}
        S->>U: move complete

        U->>S: /laser_act or /led_act (set channel)
        H->>SL: LOCAL PWM / SDO 0x2100·0x2200
        U->>C: software snap (++{cam:1}--)
        C-->>U: frame
        U->>U: save_frame_ome
        U->>S: illumination off
    end
    U->>U: workflow done → optional Ashlar stitching
```

---

## 5. Command & data-flow reference

**Routing (`DeviceRouter` + `RoutingTable`).** One row per logical device
(`MOTOR`, `LASER`, `LED`, `GALVO`, `HOME`, `TMC`, `DAC`, `AIN`, `DIN`) →
`LOCAL` (HAT GPIO), `REMOTE` (CAN node + sub-axis), or `OFF`. Built at boot from
`pinConfig` + `runtimeConfig.canRole`; editable live via `/route_set`.

**Two CAN transport classes:**
- **SDO** — addressed, acknowledged read/write of an OD index → commands & config
  (target position, laser PWM, homing, OTA, reboot).
- **TPDO/RPDO + heartbeat** — periodic, broadcast → fast state (actual position,
  status word, E-stop, collision ADC) cached on the master in `RemoteSlaveState`
  so `/motor_get` needs zero bus round-trips.

**PS4 paths (both supported):**
- **Bluetooth** → BT-HID on the HAT → `MotorGamePad` → `DeviceRouter` (axes go
  LOCAL or REMOTE just like any motor command).
- **USB-host bridge** (optional XIAO-S3 node) → publishes joystick onto CAN
  (OD 0x2400) → master's `JoystickController` consumes it identically.

**Object-dictionary base indices** (`UC2_OD_Indices.h`):

| Base | Device | Key sub-objects |
|---|---|---|
| `0x2000` | Motor | target/actual pos, speed, command/status word, enable, accel, min/max |
| `0x2010` | Homing | command, speed, direction, endstop polarity/release |
| `0x2020` | TMC | microsteps, RMS current, StallGuard, CoolStep |
| `0x2030` | Hard-limit | command, enabled, polarity |
| `0x2100` | Laser | PWM value, max, frequency, despeckle, safety state |
| `0x2200` | LED array | mode, brightness, colour, pixel count, layout |
| `0x2300` | Digital I/O | input state, **output command**, change mask (E-stop, triggers) |
| `0x2310` | Analog I/O | analog input, filtered, DAC out (collision ADC) |
| `0x2400` | Joystick | axis, buttons, speed multiplier, deadzone |
| `0x2500` | System | firmware version, board name, uptime, **reboot** |
| `0x2600` | Galvo | target/actual pos, scan speed, steps/line, X/Y start |

**Mode selection at a glance:**

| | StageScan / performance (§3) | Sequential / normal (§4) |
|---|---|---|
| Owns timing | ESP32 HAT (on MCU) | ImSwitch host |
| Trigger | hardware (`CAMERA_TRIGGER_PIN`) | software (`++{cam:1}--`) |
| Serial traffic | params once, then quiet | one round-trip per step |
| Host role | listen to camera + store | command every move/snap |
| Use when | fast XY raster, simple illumination | DPC/LED-matrix patterns, per-frame logic |

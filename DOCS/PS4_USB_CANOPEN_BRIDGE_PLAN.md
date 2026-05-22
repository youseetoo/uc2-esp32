# PS4 → USB-Host → CANopen Bridge for UC2

A plan for replacing the existing Bluetooth `PSController` joystick path with a
USB-OTG HID host running on a **dedicated ESP32-S3** that publishes joystick
state onto the same CANopen bus the rest of the UC2 satellites already speak.

The goal is that the `MotorGamePad` / `JoystickController` code on the master
node receives joystick data exactly as it does today — it just doesn't care
whether the source was BT-HID, USB-HID, or a future analog stick.

---

## 1. Why a separate firmware / repo?

**Decision: Option A — a new PlatformIO env inside this repo.**

Reasons:

- The CANopen object dictionary (`OD.h`, `UC2_OD_Indices.h`,
  `uc2_canopen_registry.yaml`) is the single source of truth and is regenerated
  by `tools/canopen/regenerate_all.py`. Two divergent copies will desync the
  first time the registry changes.
- The bridge node only needs a tiny subset of UC2 modules: `CAN_CONTROLLER_CANOPEN`
  + a new `JOYSTICK_PROVIDER` module. All motor / laser / LED / galvo modules
  should be `#ifdef`’d out via a new `pindef`.
- The TWAI driver init, NMT/heartbeat handling, and SDO/PDO infrastructure in
  `main/src/canopen/CANopenModule.cpp` are already battle-tested. Reusing them
  saves a lot of bring-up time.

Concrete layout:

```
uc2-ESP/
  platformio.ini                    # add new env: [env:uc2_ps4_usbhost_s3]
  main/src/joystick/                # new module
    JoystickUsbHost.{cpp,h}         # USB Host + HID parsing + DS4 decode
    JoystickRouter.{cpp,h}          # NEW — maps DS4 events → DeviceRouter calls
                                    # (motor / laser / led / ...) over CANopen
  main/config/uc2_ps4_usbhost_s3/PinConfig.h   # CAN_TX=3, CAN_RX=2 (XIAO S3)
```

---

## 2. Hardware target

- **Seeed XIAO ESP32-S3** — same board family already used for the
  `seeed_xiao_esp32s3_can_slave_motor` / `_laser` / `_illumination` /
  `_led` envs. The S3 USB peripheral can run in Host mode (the original ESP32
  and C3 cannot).
- **CAN pinout — identical to the existing XIAO CAN slaves:**
  `CAN_TX = GPIO 3 (D2)`, `CAN_RX = GPIO 2 (D1)`
  (see `main/config/seeed_xiao_esp32s3_can_slave_motor/PinConfig.h`). The
  bridge connects to the same CAN bus as the motor/laser/LED slaves and the
  master — same transceiver wiring, same termination, same baudrate.
- USB: the S3 USB-OTG PHY is fixed at GPIO 19/20. Wire a USB-A receptacle
  with VBUS/GND straight through to those pins.
- Power: many DS4 clones brown out at 4.5 V; budget headroom on the 5 V rail.

---

## 3. USB-Host HID — reference code

The Espressif reference at
`/Users/bene/Downloads/ESP32_USB_Host_HID-main/ESP32_USB_Host_HID-ARDUINO/examples/hid_host_joystick/hid_host_joystick.ino`
already contains a DS4 decoder (search for `DS4GamepadReport_t`, lines
~370–402). That struct decodes the 64-byte generic HID report into:

- `leftXAxis / leftYAxis / rightXAxis / rightYAxis` (uint8, 128 = centred)
- `dPad` (4-bit), `button1/2/3` (Triangle/Circle/Cross/Square, L1/R1/L2/R2/...)
- `L2Axis / R2Axis` (analog triggers)
- IMU (gyroX/Y/Z, accelX/Y/Z) — useful future bonus

**Adapt that file as `JoystickUsbHost.cpp`:**

1. Move `usb_lib_task` and `hid_host_task` into the module.
2. In the `case 64:` branch, instead of `printf(...)`, build a struct
   `Ds4State_t` and post it onto a FreeRTOS queue.
3. A second consumer task (`JoystickCanopenPublisher`) reads the queue, maps
   the DS4 fields to the existing UC2 events.

Notes / gotchas:

- DS4 must be in **USB-wired** mode (a freshly-paired-over-BT DS4 sometimes
  refuses to enumerate over USB until it gets a Set-Report). The reference
  example already handles `set_protocol` for boot-class devices; for the
  generic class you usually don’t need it.
- Reset state on `HID_HOST_INTERFACE_EVENT_DISCONNECTED` so the master sees a
  clean zero-state (otherwise the last stick value would keep driving motors).

---

## 4. Mapping DS4 → existing UC2 joystick semantics

Today `MotorGamePad::xyza_changed_event(x,y,z,a)` expects **int16 in the
range −32768..32767** with a `kOffset = 1025` dead-zone (see
`main/src/motor/MotorGamePad.cpp:12`). The DS4 sends **uint8 centred at 128**.

Bridge mapping (do this on the bridge node, NOT on the master):

```
int16_t scale(uint8_t v) { return (int16_t)(((int)v - 128) * 256); }

x = scale(ds4.leftXAxis)        // → Stepper::X
y = scale(ds4.leftYAxis)        // → Stepper::Y
z = scale(ds4.rightYAxis)       // → Stepper::Z
a = scale(ds4.rightXAxis)       // → Stepper::A   (or leave at 0 on CAN_HAT_Master, see MotorGamePad.cpp:120-127)
```

Triggers / shoulder buttons → `singlestep_event`:

```
l1 = ds4.button2 & 0x01
r1 = ds4.button2 & 0x02
l2 = ds4.button2 & 0x04
r2 = ds4.button2 & 0x08
options = ds4.button2 & 0x20   // Options button → fine/coarse toggle
```

---

## 5. Architecture: where does the gamepad logic live?

There are two viable shapes for this system. The earlier draft of this doc
described **Pattern A**; the chosen approach is **Pattern B**.

### Pattern A — “bridge republishes joystick state, master interprets it”

The bridge writes raw axis/button values into `0x2400 / 0x2401`. A consumer
on the master node reads RPDOs and calls
`MotorGamePad::xyza_changed_event(...)`. This is the minimum-protocol option
and is fine for *motors only*, but to also drive laser power, LED matrix
patterns, etc., we’d have to mirror the entire gamepad-callback layer onto
every actuator node. That’s a lot of duplicated state.

### Pattern B — **chosen** — “bridge IS the gamepad-callback layer, sends commands directly to actuator slaves”

The bridge runs the *same* `MotorGamePad`-style callback code that the master
runs today and emits **the same JSON commands** through `DeviceRouter`.
`DeviceRouter::handleMotorAct / handleLaserAct / handleLedAct / ...` already
consults `RoutingTable` and either executes locally *or* serializes the
request and sends it as an SDO write to the responsible slave node
(`main/src/canopen/DeviceRouter.cpp` — the REMOTE path is active whenever
`CAN_CONTROLLER_CANOPEN` is defined).

This means the bridge can call **exactly the same helpers**
`MotorGamePad.cpp` already calls — `startAxis`, `stopAxis`, `doSingleStep` —
without any code changes to those helpers, and the bytes go straight to the
motor slave that owns that axis. No new master-side consumer is needed.

```
DS4 stick → JoystickUsbHost (USB Host task)
          → JoystickRouter   (gamepad callbacks: xyza_changed_event, ...)
          → MotorGamePad::startAxis/stopAxis/doSingleStep   (unchanged)
          → DeviceRouter::handleMotorAct(jsonDoc)           (unchanged)
              ├─ LOCAL?  → no — bridge has no motor controller
              └─ REMOTE  → SDO write to motor slave's OD 0x2000…  (CANopen)
```

### What the bridge can drive

The same indirection works for every actuator that already has a
`handle*Act` entry in `DeviceRouter`:

| Gamepad input          | Bridge calls                                        | Reaches slave via                  |
| ---------------------- | --------------------------------------------------- | ---------------------------------- |
| Left stick X/Y         | `MotorGamePad::xyza_changed_event(x,y,0,0)`         | `handleMotorAct` → motor slave     |
| Right stick Y          | `MotorGamePad::xyza_changed_event(_,_,z,_)`         | `handleMotorAct` → motor slave (Z) |
| R1 / R2 / L1 / L2      | `MotorGamePad::singlestep_event(...)`               | `handleMotorAct` → motor slave (Z) |
| Options                | `MotorGamePad::options_changed_event(1)` (fine/coarse) | local state only                |
| Triangle (toggle laser)| new `LaserGamePad::toggle()` → `handleLaserAct({"laser":{"LASERid":1,"LASERval":...}})` | `handleLaserAct` → laser slave |
| Square (cycle pattern) | new `LedGamePad::next_pattern()` → `handleLedAct({"ledarr":{"led":{"LEDArrMode":"single", ...}}})` | `handleLedAct` → LED slave  |
| D-pad ↑/↓ (intensity)  | `LaserGamePad::adjust_power(±)` → `handleLaserAct({"laser":{"LASERid":n,"LASERval":v}})` | `handleLaserAct` → laser slave |
| Circle / Cross         | application-defined (home, stop, snapshot, …)       | matching `handle*Act`              |

The `*GamePad` helpers should live in the bridge’s `main/src/joystick/`
directory (not in `main/src/motor/`, `…/laser/`, `…/led/`) so they don’t leak
into the master build. They mirror the style of the existing
`MotorGamePad.cpp` — small, stateful, callback-driven.

### Why this is the right trade-off

- **One place owns gamepad semantics** (fine/coarse mode, debounce, axis
  inversion, dead-zone) — the bridge. The master doesn’t need to know a
  gamepad exists.
- **Zero protocol invention**: we reuse the JSON-over-SDO command path that
  already works for the REST API and for every other satellite-targeting
  command.
- **Adding a new gamepad-driven feature = adding one `handle*Act` JSON
  builder in the bridge.** No firmware change on the master or on the
  actuator slave.
- **The OD slots `0x2400 / 0x2401` are still useful** as *telemetry* (the
  GUI / master can subscribe to see raw stick state) but are no longer the
  control path. Keep them; map them on a low-priority TPDO.

### Trade-offs to be aware of

- **Latency** of the JSON-over-SDO path is higher than a single RPDO frame
  (~a few ms per command vs sub-ms). For 100 Hz joystick → motor speed
  updates this is fine; for sub-millisecond closed-loop control it would not
  be. Coalesce in the bridge (only send when value crosses a threshold).
- **SDO is point-to-point**; a “stop all axes” broadcast must be N sends.
  Acceptable at the scale we operate.
- **The bridge depends on `RoutingTable` being correctly configured** so
  that `DeviceRouter` knows which node owns the motor / laser / LED. This is
  already required for the REST master and the GUI, so no new state.

---

## 6. CANopen object dictionary — minimal changes needed

Because Pattern B uses the existing `DeviceRouter` JSON-over-SDO path, the
actuator slaves don’t need any new OD entries — they already accept the
`motor_act` / `laser_act` / `ledarr_act` commands. The joystick entries
already in the registry (`tools/canopen/uc2_canopen_registry.yaml:640`):

| Index   | Sub | Name                        | Type | PDO    |
| ------- | --- | --------------------------- | ---- | ------ |
| 0x2400  | 1..4 | joystick_axis (X,Y,Z,R)    | I16  | tpdo4  |
| 0x2401  | 0   | joystick_buttons (bitmask)  | U16  | tpdo4  |
| 0x2402  | 0   | joystick_speed_multiplier   | U8   | sdo    |
| 0x2403  | 0   | joystick_deadzone           | U16  | sdo    |

…become **telemetry only**. The bridge still writes them on every USB report
so the GUI / master can see raw stick state, but the control path is JSON
commands via `DeviceRouter`.

Optional additions (only if useful for the GUI):

- `0x2404 joystick_triggers` (U16, packed L2/R2)
- `0x2405 joystick_dpad` (U8)

Edit `uc2_canopen_registry.yaml`, then
`python tools/canopen/regenerate_all.py`.

---

## 7. Suggested file plan for the bridge firmware

```
main/src/joystick/JoystickUsbHost.{h,cpp}
        // USB Host + HID + DS4 decode + FreeRTOS queue → JoystickRouter

main/src/joystick/JoystickRouter.{h,cpp}
        // Gamepad-callback layer (lives on the bridge, NOT on master).
        // - xyza_changed_event(x,y,z,a)        → MotorGamePad helpers
        // - options_changed_event(pressed)     → fine/coarse local state
        // - singlestep_event(...)              → MotorGamePad::doSingleStep
        // - laser_button_event(btn, val)       → builds {"laser":{...}} JSON
        //                                        and calls DeviceRouter::handleLaserAct
        // - led_button_event(btn, val)         → builds {"ledarr":{...}} JSON
        //                                        and calls DeviceRouter::handleLedAct
        // - galvo_event / home_event / …       → matching DeviceRouter::handle*Act
        // Also writes telemetry to OD 0x2400/0x2401/0x2404 for the GUI.

main/config/uc2_ps4_usbhost_s3/PinConfig.h
        // Mirror seeed_xiao_esp32s3_can_slave_motor pins:
        //   CAN_TX = GPIO_NUM_3 (D2)
        //   CAN_RX = GPIO_NUM_2 (D1)
        // No motor / laser / LED pins — none of those controllers compile in.

platformio.ini
        // [env:uc2_ps4_usbhost_s3]
        //   board       = seeed_xiao_esp32s3
        //   build_flags = -D CAN_CONTROLLER_CANOPEN
        //                 -D JOYSTICK_USBHOST_PROVIDER
        //                 -D PINDEF=uc2_ps4_usbhost_s3
        //   No MOTOR_CONTROLLER / LASER_CONTROLLER / LED_CONTROLLER /
        //   HOME_MOTOR / GALVO_CONTROLLER / TMC_CONTROLLER defines —
        //   the bridge has no local actuators.
```

Module init in `main/main.cpp` (already gated by `JOYSTICK_USBHOST_PROVIDER`):

```cpp
#ifdef JOYSTICK_USBHOST_PROVIDER
  JoystickUsbHost::begin();
  JoystickRouter::begin();
#endif
```

The bridge node calls `CANopenModule::begin()` like every other satellite and
picks a free node-id (e.g. **0x2A**). Crucially: because the bridge issues
*outgoing* commands, it acts as an **SDO client** toward the motor / laser /
LED slaves. `DeviceRouter`’s REMOTE path already handles this when
`CAN_CONTROLLER_CANOPEN` is defined — confirm with `RoutingTable` that the
target node-ids are correct.

> **Important note about `MotorGamePad.cpp`:** the file currently lives under
> `main/src/motor/` and `#include`s `FocusMotor.h`. To reuse its
> `xyza_changed_event` / `singlestep_event` / `options_changed_event` logic on
> the bridge (which has no `FocusMotor` compiled in), split it so its
> *callback-and-curve* core is reusable but only the `FocusMotor::getData()`
> access is `#ifdef MOTOR_CONTROLLER`’d. The JSON it already builds and hands
> to `DeviceRouter::handleMotorAct(...)` works unchanged.

---

## 8. End-to-end data flow

```
   ┌──────────┐ USB HID  ┌─────────────────┐  Q  ┌──────────────────────────────────┐
   │   DS4    │ ───────► │ JoystickUsbHost │ ──► │           JoystickRouter         │
   └──────────┘          └─────────────────┘     │  (gamepad callbacks, fine/coarse,│
                                                 │   debounce, dead-zone, ...)      │
                                                 └───┬──────────────┬──────────────┬┘
                                                     │              │              │
                                  MotorGamePad::startAxis    LaserGamePad::set   LedGamePad::pattern
                                     /stopAxis/doSingleStep         │              │
                                                     │              │              │
                                                     ▼              ▼              ▼
                                          ┌──────────────────────────────────────────┐
                                          │ DeviceRouter::handleMotorAct / Laser /   │
                                          │ Led / ... — already REMOTE-aware via     │
                                          │ RoutingTable.                            │
                                          └─────────────┬────────────────────────────┘
                                                        │ SDO write (JSON-over-CANopen)
                                                        ▼
                                          ┌─────────────────────────┬─────────────┐
                                          │  Motor slave (0x10..)   │ Laser slave │  LED slave
                                          │  acts on motor_act JSON │ (0x14..)    │  (0x18..)
                                          └─────────────────────────┴─────────────┘

       (in parallel, low-priority telemetry:)
       JoystickRouter → OD 0x2400/0x2401/0x2404 → TPDO4 → GUI / master can subscribe
```

---

## 9. Instructions for a Claude Code session to implement this

Paste this block into a fresh Claude Code session inside the `uc2-ESP` repo:

> **Task**: implement a USB-HID → CANopen gamepad bridge for a Seeed XIAO
> ESP32-S3. The bridge runs the gamepad-callback layer locally and emits
> motor / laser / LED commands as JSON over `DeviceRouter` (Pattern B in
> `docs/PS4_USB_CANOPEN_BRIDGE_PLAN.md`). Read that doc first.
>
> **Step 1 — vendor the HID driver.**
> Copy `lib/esp_usb_host_hid/` from the Espressif component
> (https://components.espressif.com/components/espressif/usb_host_hid) into
> `lib/`. Confirm the example at
> `examples/hid_host_joystick/hid_host_joystick.ino` compiles for
> `seeed_xiao_esp32s3` standalone before integrating.
>
> **Step 2 — new pindef + env.**
> Copy `main/config/seeed_xiao_esp32s3_can_slave_motor/PinConfig.h` to
> `main/config/uc2_ps4_usbhost_s3/PinConfig.h` (CAN_TX=GPIO3, CAN_RX=GPIO2
> stay the same; USB PHY is fixed at GPIO19/20). Strip motor pins. Add
> `[env:uc2_ps4_usbhost_s3]` to `platformio.ini` with build_flags from
> section 7. Do **not** define `MOTOR_CONTROLLER` / `LASER_CONTROLLER` /
> `LED_CONTROLLER` / `HOME_MOTOR` / `GALVO_CONTROLLER` / `TMC_CONTROLLER`.
>
> **Step 3 — implement `JoystickUsbHost`.**
> Port the reference `.ino`. Replace the `case 64:` `printf` with a queue
> push of a `Ds4State_t` struct (axes, buttons, triggers, dpad, plus a
> `valid` bool cleared on disconnect).
>
> **Step 4 — refactor `MotorGamePad` so it’s reusable on the bridge.**
> Split `main/src/motor/MotorGamePad.cpp` so the curve / dead-zone /
> fine-coarse / inactivity-timeout logic is portable. The current file
> reaches into `FocusMotor::getData()` (for `joystickDirectionInverted`) and
> calls `DeviceRouter::handleMotorAct` — keep the JSON-builder + DeviceRouter
> path, but `#ifdef MOTOR_CONTROLLER` the `FocusMotor` access, falling back
> to a constant or a bridge-local config.
>
> **Step 5 — implement `JoystickRouter`.**
> New file in `main/src/joystick/`. Owns the gamepad-callback layer:
> - `xyza_changed_event(x,y,z,a)` → existing `MotorGamePad` helpers.
> - `singlestep_event(...)` → existing `MotorGamePad::doSingleStep`.
> - `options_changed_event(pressed)` → fine/coarse toggle (bridge-local).
> - **NEW** `laser_event(buttons)` → build
>   `{"laser":{"LASERid":n,"LASERval":v}}` and call
>   `DeviceRouter::handleLaserAct(doc)`. Mirror the JSON shape of the REST
>   `/laser_act` endpoint.
> - **NEW** `led_event(buttons)` → build `{"ledarr":{"led":{...}}}` and
>   call `DeviceRouter::handleLedAct(doc)` for pattern cycling /
>   brightness.
> - Add hooks for `home_event`, `galvo_event` later.
> Also writes telemetry to OD `0x2400/0x2401` on every report.
>
> **Step 6 — RoutingTable check.**
> On bridge boot, verify `RoutingTable` has REMOTE entries for `motor`,
> `laser`, `led`. Log a warning if any is missing; the bridge has no LOCAL
> handlers and a missing route would silently drop commands.
>
> **Step 7 — (optional) extend the OD telemetry.**
> If triggers/DPad telemetry is wanted, add `0x2404 joystick_triggers` and
> `0x2405 joystick_dpad` to `tools/canopen/uc2_canopen_registry.yaml` and
> run `python tools/canopen/regenerate_all.py`.
>
> **Step 8 — remove the BT path on the master (optional, last).**
> Once the bridge is verified end-to-end, gate `lib/PSController` behind
> `#ifdef PS_CONTROLLER_BT` and disable it in the default master env. The
> master no longer needs any joystick-aware code — the bridge sends fully
> formed `motor_act` / `laser_act` / `ledarr_act` commands.
>
> **Step 9 — manual test plan.**
> 1. Flash the bridge only, master powered off. With `candump can0`:
>    moving the left stick → see SDO writes targeted at the motor slave
>    node-id; pressing Triangle → SDO writes at the laser slave node-id.
> 2. Power the motor + laser + LED slaves. Confirm motors move, laser
>    toggles, LEDs change patterns directly from gamepad input — without
>    the master interpreting anything.
> 3. Unplug DS4 → the bridge sends one final zero-axis `motor_act`, motors
>    stop within `MotorGamePad`’s `INACTIVITY_TIMEOUT` fallback.
>
> **Acceptance**: with master firmware unmodified (or with BT path
> disabled), DS4 over USB drives motor, laser, and LED slaves directly via
> the bridge over CANopen, using only existing `DeviceRouter` paths.

---

## 10. Risks / open questions

- **DS4 over USB sometimes ships only the simplified 64-byte report; clones
  vary.** Build a small `case length:` dispatch like the reference does and
  log unknown report sizes during bring-up.
- **JSON-over-SDO command rate.** Each command is a multi-segment SDO
  transfer — budget a few ms per command. Bridge must throttle/coalesce
  joystick-driven motor speed updates (only send on threshold crossing).
- **PDO budget for telemetry.** 4×I16 fills one PDO. Add only what the GUI
  actually needs to subscribe to.
- **`MotorGamePad` coupling.** It currently depends on `FocusMotor` and
  `pinConfig`. The refactor in Step 4 is the only invasive change to
  existing code; keep it minimal and behind `#ifdef MOTOR_CONTROLLER`.
- **VBUS / power.** A 5 V LDO is often not enough for a DS4 + WS2812 LED
  bar; budget separately.
- **Future-proofing.** A future Xbox / Switch Pro bridge is a drop-in
  replacement of `JoystickUsbHost` only — `JoystickRouter` and the
  DeviceRouter path stay identical.

# UC2 CANopen Migration — Work Packages v2 (post PR-119)

## Where we are right now

**PR #119** (`feature/runtime-config`) is in flight and contains everything from the
original PR-1 through PR-5 in one branch:

| Original PR | What it added | Status in #119 |
|------------|---------------|----------------|
| PR-1 | RuntimeConfig + NVS | Done |
| PR-2 | Rename `can_controller` → `can_transport` | Done |
| PR-3 | CANopenNode library in `lib/ESP32_CanOpenNode/` | Done |
| PR-4 | `CANopenModule` slave (TWAI + OD↔Module sync) | Done |
| PR-5 | `DeviceRouter` master (JSON → SDO bridge) | Done |

**What works end-to-end today:**

- Three build environments compile: `UC2_canopen_master`, `UC2_canopen_slave`, `UC2_3_CAN_HAT_Master_v2`
- Master receives `motor_act` JSON over serial
- DeviceRouter writes target position, speed, and ctrl flags via SDO to the slave
- Slave's `syncRpdoToModules()` decodes the OD entries and calls `FocusMotor::startStepper()`
- Motor moves
- Slave's `syncModulesToTpdo()` writes actual position back into the OD
- Master can read it via `motor_get`

**What is still wrong / temporary:**

The motor command currently uses a five-write workaround that splits `int32_t` position
across two `uint16` OD entries because the trainer OD doesn't have a native `int32` slot:

```cpp
// CURRENT (temporary)
//   0x6401:01 = posHi  (uint16, bits 31..16)
//   0x6401:02 = posLo  (uint16, bits 15..0)
//   0x6401:03 = speed  (uint16, clamped)
//   0x6401:04 = ctrl   (uint16, bit0=isAbs, bit1=isStop)
//   0x6200:01 = trigger (uint8, bit0=1 to start move)
```

This is fragile, limits speed to 65535, drops acceleration entirely, and uses CiA 401
generic-I/O indices instead of UC2-specific ones. Everything from here onwards rebuilds
on a proper foundation.

## End goal

After all remaining PRs are merged, the firmware satisfies these properties:

1. **Single source of truth.** All OD entries are defined in `tools/uc2_canopen_registry.yaml`.
   The EDS, OD.h/OD.c, C++ index constants, Python constants, and documentation are all
   regenerated from it by `tools/regenerate_all.py`. To add a new hardware feature,
   you edit one YAML file and run one script.

2. **Every parameter exposed via CANopen.** Motor: target position, actual position, speed,
   acceleration, jerk, isabs, enable, soft limits, status word, command word, homing params,
   TMC config, encoder feedback. Laser: PWM value per channel, frequency, resolution,
   despeckle, safety state. LED: mode, brightness, uniform colour, full pixel array via
   SDO block transfer, built-in pattern selection. Galvo: target XY, scan parameters,
   command word, status. Plus PID, joystick, digital/analog I/O, system state, OTA.
   No JSON-only fields remain.

3. **No magic numbers in C++.** Code uses `UC2_OD::MOTOR_TARGET_POSITION` from a
   generated header — never `0x6401` or `0x2000` literals.

4. **Slaves push state actively.** Motor position and status are TPDO-mapped and broadcast
   on change (event-driven, not polled). The master receives these without sending any SDO
   request. The Pi gets immediate notification of moves completing or endstops triggering.

5. **OTA via CANopen.** Firmware updates flow through SDO block transfer to OD index 0x2F00.
   No serial reflashing required for satellite nodes.

6. **Old transport gone.** `can_transport.cpp`, the ISO-TP library, the dozen
   `CAN_RECEIVE_*` and `CAN_SEND_COMMANDS` flags, and the `can_controller_stubs.cpp`
   shim are all deleted. The CAN codebase is roughly 1000 lines of UC2 code on top of
   the unmodified CANopenNode library.

7. **Pi can talk CANopen directly.** Via the Waveshare USB-CAN adapter and `python-canopen`,
   bypassing the master ESP32 entirely. Same EDS, same OD entries, same node-IDs.
   Useful for debugging, scripting, and diagnostics.

8. **Scales to new hardware.** Adding a new module type (e.g. a temperature controller)
   requires: 4 lines in the registry YAML, regenerating the artifacts, and writing one
   `syncRpdoToModules` branch. No EDS hand-editing, no OD.h hand-editing, no scattered
   `#ifdef` additions.

---

## Architecture: motor command flow end-to-end

This is the data path a single `motor_act` command traces from the Pi to the stepper
driver pins. Every other module (laser, LED, galvo, OTA) follows the same shape — only
the OD indices and the destination function call change.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  Raspberry Pi  /  ImSwitch  /  python-canopen                               │
│  {"task":"/motor_act","motor":{"steppers":[                                 │
│    {"stepperid":1,"position":1000,"speed":15000,"isabs":1,"accel":100000}   │
│  ]}}                                                                        │
└─────────────────────────────────┬───────────────────────────────────────────┘
                                  │  USB serial, JSON
                                  ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  Master ESP32  (NodeRole::CAN_MASTER, node-id 1)                            │
│  ┌─────────────────────────┐    ┌─────────────────────────────────────────┐ │
│  │ SerialProcess           │───▶│ DeviceRouter::handleMotorAct            │ │
│  │ jsonProcessor() switch  │    │ stepperid → nodeId, build SDO writes    │ │
│  └─────────────────────────┘    └─────────────────┬───────────────────────┘ │
│                                                   ▼                         │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ CANopenModule::writeSDO  (4 calls per axis)                         │    │
│  │  • UC2_OD::MOTOR_TARGET_POSITION, sub=axis, int32                   │    │
│  │  • UC2_OD::MOTOR_SPEED,           sub=axis, uint32                  │    │
│  │  • UC2_OD::MOTOR_ACCELERATION,    sub=axis, uint32                  │    │
│  │  • UC2_OD::MOTOR_COMMAND_WORD,    sub=0,    uint8 = 0x02 (move_abs) │    │
│  │   ↓ CO_SDOclient_setup → CO_SDOclientDownloadInitiate → ...         │    │
│  └─────────────────────────────────┬───────────────────────────────────┘    │
└────────────────────────────────────┼────────────────────────────────────────┘
                                     │  CAN bus  500 kbit/s
                                     │  4× SDO frames ≈ 4 ms total
                                     ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│            CAN bus  (TWAI / SocketCAN / Waveshare USB-CAN)                  │
│  COB-IDs: 0x60B (SDO req to node 11), 0x58B (SDO resp), 0x18B (TPDO1),      │
│           0x70B (heartbeat from node 11), 0x000 (NMT broadcast)             │
└─────────────────────────────────┬───────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  Slave ESP32-S3  (NodeRole::CAN_SLAVE, node-id 11 = X axis)                 │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ CO_interruptTask  →  CO_SDOserver_process                           │    │
│  │ Decodes SDO frames, writes values directly into OD_RAM              │    │
│  └─────────────────────────────────┬───────────────────────────────────┘    │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ OD_RAM  (object dictionary mirror, generated from registry)         │    │
│  │  x2000_motor_target_position[0] = 1000                              │    │
│  │  x2002_motor_speed[0]           = 15000                             │    │
│  │  x2006_motor_acceleration[0]    = 100000                            │    │
│  │  x2003_motor_command_word       = 0x02   ← trigger                  │    │
│  └─────────────────────────────────┬───────────────────────────────────┘    │
│                                    │  CO_tmrTask polls every 1 ms           │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ CANopenModule::syncRpdoToModules                                    │    │
│  │  if (cmd != prevCmd && cmd != 0) {                                  │    │
│  │    Stepper s;                                                       │    │
│  │    s.targetPosition = OD_RAM.x2000_motor_target_position[axis];     │    │
│  │    s.speed          = OD_RAM.x2002_motor_speed[axis];               │    │
│  │    s.acceleration   = OD_RAM.x2006_motor_acceleration[axis];        │    │
│  │    s.isabs          = OD_RAM.x2007_motor_is_absolute;               │    │
│  │    focusMotor->startStepper(s);  // existing code, untouched        │    │
│  │    OD_RAM.x2003_motor_command_word = 0;  // clear trigger           │    │
│  │  }                                                                  │    │
│  └─────────────────────────────────┬───────────────────────────────────┘    │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ FocusMotor::startStepper  (FastAccelStepper drives DIR/STEP pins)   │    │
│  │ Motor turns                                                         │    │
│  └─────────────────────────────────┬───────────────────────────────────┘    │
│                                    │                                        │
│                                    │  Position changes during motion        │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ CANopenModule::syncModulesToTpdo  (also called from CO_tmrTask)     │    │
│  │  OD_RAM.x2001_motor_actual_position[axis] = motor->getPos();        │    │
│  │  OD_RAM.x2004_motor_status_word[axis]     = isRunning|isHomed<<1;   │    │
│  │  ↓                                                                  │    │
│  │ CO_TPDOsend  (TPDO1 = 0x18B, event-driven, 5 ms inhibit time)       │    │
│  └─────────────────────────────────┬───────────────────────────────────┘    │
└────────────────────────────────────┼────────────────────────────────────────┘
                                     │  TPDO1 frame on CAN bus  (8 bytes)
                                     │  Slave PUSHES, master never asks
                                     ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  Master ESP32  (RPDO consumer for TPDO1 from node 11)                       │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ CANopenNode RPDO callback writes received values into local OD_RAM  │    │
│  │ master_remoteSlaveCache[11].position[0] = 1000  (now reached)       │    │
│  │ master_remoteSlaveCache[11].lastUpdateMs = millis()                 │    │
│  └─────────────────────────────────┬───────────────────────────────────┘    │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │ When status_word transitions running→done:                          │    │
│  │ SerialProcess emits unsolicited  {"event":"motor_complete",         │    │
│  │                                   "stepperid":1,"position":1000}    │    │
│  │ to the Pi over USB serial — no polling, fully push-driven           │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Key things to notice in this diagram:**

- **Three transport types**, each used for what it's best at. **SDO** carries master→slave
  commands (reliable, acknowledged, but slow — one frame per parameter). **PDO** carries
  high-frequency state (fire-and-forget, 8 bytes per frame, no overhead). **Serial JSON**
  is the Pi-to-master interface and never crosses the CAN bus.

- **OD_RAM is the integration point.** The slave's existing `FocusMotor` code doesn't know
  CANopen exists. It reads from a `Stepper` struct, same as today. The new
  `syncRpdoToModules()` populates that struct from `OD_RAM`. The existing
  `syncModulesToTpdo()` writes motor position back into `OD_RAM`. Everything else is
  CANopenNode's job.

- **No magic numbers.** Every reference to an OD index uses `UC2_OD::MOTOR_TARGET_POSITION`
  from the auto-generated `UC2_OD_Indices.h`. The compiler catches typos. Renaming an
  entry in the registry YAML and regenerating updates every consumer at once.

- **The slave actively pushes state** via TPDO1. The master never sends an SDO read for
  motor position — that data arrives unsolicited every time the value changes (subject to
  the 5 ms inhibit time) and once per 100 ms as a heartbeat refresh. The master maintains
  a local mirror of every slave's state in `master_remoteSlaveCache[]`, indexed by
  node-id, so `/motor_get` from the Pi returns instantly with no CAN traffic at all.

- **Same path works from the Pi directly.** Replace "Master ESP32" in the top half with
  "Pi running python-canopen via Waveshare USB-CAN" and the entire bottom half is
  unchanged. Same EDS, same OD indices, same node-IDs. PR-13 enables this.

---

## Reference assets (provide to Claude Code in each task)

Place these in the repo before starting:

| File | Path | Purpose |
|------|------|---------|
| `uc2_canopen_registry.yaml` | `tools/uc2_canopen_registry.yaml` | Single source of truth for all OD entries |
| `regenerate_all.py` | `tools/regenerate_all.py` | Generates EDS, OD.h, C++ headers, Python constants, docs |
| `openUC2_satellite.eds` | `DOCUMENTATION/openUC2_satellite.eds` | Generated — load in CANopenEditor for visual review |
| `UC2_OD_Indices.h` | `main/src/canopen/UC2_OD_Indices.h` | Generated — C++ named constants |
| `uc2_indices.py` | `PYTHON/uc2_canopen/uc2_indices.py` | Generated — Python named constants for Waveshare scripts |

---

## PR-6: Generator infrastructure + drop-in registry

**Branch:** `feature/canopen-registry`
**Depends on:** PR #119 (current state of `feature/runtime-config`)
**Estimated scope:** ~5 new files, no firmware behaviour change yet
**Why first in this batch:** every later PR consumes generated output; we need the
generator before we can regenerate anything.

### Claude Code prompt

```
Goal: Add the parameter registry + generator scripts as the foundation for all later
CANopen work. This PR does NOT change firmware behaviour — it only adds tooling and
generated files. The motor still moves via the split-uint16 workaround after this PR.

Steps:

1. Copy these files into the repo:
   - tools/uc2_canopen_registry.yaml      (provided — single source of truth)
   - tools/regenerate_all.py              (provided — generator)

2. Run the generator once:
   cd tools && python regenerate_all.py

   This creates:
   - DOCUMENTATION/openUC2_satellite.eds
   - DOCUMENTATION/UC2_CANopen_Parameters.md
   - main/src/canopen/UC2_OD_Indices.h
   - PYTHON/uc2_canopen/uc2_indices.py
   - lib/uc2_od/OD.h          (skeleton only — full OD.c table comes in PR-7)
   - lib/uc2_od/OD.c          (stub only — replaced in PR-7)

3. Verify the generated UC2_OD_Indices.h compiles by including it in CANopenModule.cpp:
   #include "UC2_OD_Indices.h"
   But do NOT use any of the constants yet — that's PR-7. Just verify the header compiles.

4. Add a CI check to .github/workflows/ (or extend existing workflow) that runs:
   python tools/regenerate_all.py
   git diff --exit-code
   This catches drift between the registry and the generated files.

5. Add tools/README.md explaining:
   - The registry is the single source of truth
   - Workflow for adding a new parameter:
     a) Edit uc2_canopen_registry.yaml
     b) Run python tools/regenerate_all.py
     c) Commit the YAML and ALL generated files together
   - How to add a new module type (motor, laser, etc.)
   - How CANopenEditor fits in (visual review of the generated EDS, optional re-export)

6. Add a Makefile target or pio script:
   pio run -t canopen-regen
   that invokes the generator. Document it in the main README.

DO NOT in this PR:
- Touch CANopenModule.cpp logic
- Touch DeviceRouter.cpp logic
- Touch the trainer OD in lib/ESP32_CanOpenNode/src/OD.h or OD.c
- Change platformio.ini build environments (yet)

Verification:
- All build environments still compile (UC2_canopen_master, UC2_canopen_slave,
  UC2_3_CAN_HAT_Master_v2, UC2_2)
- python tools/regenerate_all.py exits 0
- Generated files are committed
- DOCUMENTATION/openUC2_satellite.eds opens without errors in CANopenEditor

Notes:
- The registry contains 13 modules with ~90 OD entries covering motor, homing, TMC,
  laser, LED, digital/analog I/O, encoder, joystick, system, galvo, PID, OTA.
- This is a foundation PR — the payoff comes in PR-7 when the slave actually uses these
  entries.
```

---

## PR-7: Replace trainer OD with native UC2 OD

**Branch:** `feature/canopen-uc2-od`
**Depends on:** PR-6
**Estimated scope:** ~50 lines changed in `CANopenModule.cpp`, full OD.c replacement
**Critical:** this is the PR that removes the split-uint16 workaround and switches to
native int32 motor positions.

### Claude Code prompt

```
Goal: Replace the trainer's CiA 401 OD (0x6000/0x6200/0x6401) with the native UC2 OD
(0x2000-0x2F03) generated from the registry. The motor will then receive its target
position as a single int32 SDO write instead of the split-uint16 workaround.

This PR has TWO sub-steps because we don't have a fully working OD.c generator yet:

Step A — generate OD.c from CANopenEditor (manual, one time):

1. Open DOCUMENTATION/openUC2_satellite.eds in CANopenEditor
2. File → Export → Network configuration → CANopenNode v4
3. Save the exported OD.h and OD.c
4. Move them to lib/uc2_od/OD.h and lib/uc2_od/OD.c
   (overwriting the stubs from PR-6)
5. Verify lib/uc2_od/OD.h declares OD_RAM_t with all expected fields, e.g.:
   int32_t  x2000_motor_target_position[4];
   int32_t  x2001_motor_actual_position[4];
   uint32_t x2002_motor_speed[4];
   uint8_t  x2003_motor_command_word;
   uint8_t  x2004_motor_status_word[4];
   ... etc

Step B — switch CANopenModule and DeviceRouter to use the new OD:

1. In lib/ESP32_CanOpenNode/CMakeLists.txt (or wherever the library SRCS live),
   REMOVE the old trainer OD.c from the build:
   - lib/ESP32_CanOpenNode/src/OD.c   ← remove from sources
   ADD the new UC2 OD:
   - lib/uc2_od/OD.c                  ← add to sources
   And update the include path so #include "OD.h" picks up lib/uc2_od/OD.h.

2. In main/src/canopen/CANopenModule.cpp:

   Replace syncRpdoToModules() — remove the split-uint16 decoding.
   The new version uses ONE include and ONE namespace:

   #include "UC2_OD_Indices.h"   // generated by tools/regenerate_all.py
   #include "OD.h"               // generated by CANopenEditor

   void CANopenModule::syncRpdoToModules() {
       if (runtimeConfig.canRole != NodeRole::CAN_SLAVE) return;

       // --- Motor ---
       static uint8_t prevCmd = 0;
       uint8_t cmd = OD_RAM.x2003_motor_command_word;
       if (cmd != prevCmd) {
           prevCmd = cmd;
           // For each axis bit set in the command word, dispatch to FocusMotor
           for (uint8_t axis = 0; axis < 4; axis++) {
               if (!(cmd & (1 << axis))) continue;
               int32_t  target  = OD_RAM.x2000_motor_target_position[axis];
               uint32_t speed   = OD_RAM.x2002_motor_speed[axis];
               uint32_t accel   = OD_RAM.x2006_motor_acceleration[axis];
               uint8_t  isAbs   = OD_RAM.x2007_motor_is_absolute[axis];
               bool     isStop  = (cmd & 0x10) != 0;

               if (isStop) {
                   focusMotor->stopStepper(static_cast<Stepper>(axis));
               } else {
                   MotorData* d = focusMotor->getData()[static_cast<Stepper>(axis)];
                   d->targetPosition = target;
                   d->speed          = speed;
                   d->acceleration   = accel;
                   d->isabs          = isAbs;
                   focusMotor->startStepper(static_cast<Stepper>(axis));
               }
           }
           OD_RAM.x2003_motor_command_word = 0;  // clear after processing
       }

       // --- Motor enable (per axis) ---
       static uint8_t prevEnable[4] = {0xFF, 0xFF, 0xFF, 0xFF};
       for (uint8_t axis = 0; axis < 4; axis++) {
           uint8_t en = OD_RAM.x2005_motor_enable[axis];
           if (en != prevEnable[axis]) {
               prevEnable[axis] = en;
               // FocusMotor::setEnable or equivalent
               focusMotor->setEnable(static_cast<Stepper>(axis), en != 0);
           }
       }

       // --- Homing trigger ---
       static uint8_t prevHome[4] = {0};
       for (uint8_t axis = 0; axis < 4; axis++) {
           uint8_t homeCmd = OD_RAM.x2010_homing_command[axis];
           if (homeCmd != prevHome[axis] && homeCmd == 1) {
               prevHome[axis] = homeCmd;
               // Pull homing params from OD and start
               HomeData h;
               h.speed              = OD_RAM.x2011_homing_speed[axis];
               h.direction          = OD_RAM.x2012_homing_direction[axis];
               h.timeout            = OD_RAM.x2013_homing_timeout[axis];
               h.endposrelease      = OD_RAM.x2014_homing_endstop_release[axis];
               homeMotor->startHome(static_cast<Stepper>(axis), h);
               OD_RAM.x2010_homing_command[axis] = 0;
           }
       }

       // --- Laser (illumination nodes) ---
       static uint16_t prevLaser[4] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
       for (uint8_t ch = 0; ch < 4; ch++) {
           uint16_t v = OD_RAM.x2100_laser_pwm_value[ch];
           if (v != prevLaser[ch]) {
               prevLaser[ch] = v;
               laserController->setLaserVal(ch, v);
           }
       }

       // --- LED mode ---
       static uint8_t prevLedMode = 0xFF;
       if (OD_RAM.x2200_led_array_mode != prevLedMode) {
           prevLedMode = OD_RAM.x2200_led_array_mode;
           ledController->setMode(prevLedMode,
               OD_RAM.x2201_led_brightness,
               OD_RAM.x2202_led_uniform_colour);
       }
   }

   Replace syncModulesToTpdo() — push module state back into OD_RAM:

   void CANopenModule::syncModulesToTpdo() {
       if (runtimeConfig.canRole != NodeRole::CAN_SLAVE) return;

       // --- Motor actual position + status ---
       for (uint8_t axis = 0; axis < 4; axis++) {
           OD_RAM.x2001_motor_actual_position[axis] =
               focusMotor->getCurrentPosition(static_cast<Stepper>(axis));
           uint8_t status = 0;
           if (focusMotor->isRunning(static_cast<Stepper>(axis))) status |= 0x01;
           if (focusMotor->isHomed(static_cast<Stepper>(axis)))   status |= 0x02;
           OD_RAM.x2004_motor_status_word[axis] = status;
       }

       // --- Endstops + analog inputs ---
       OD_RAM.x2300_digital_input_state[0] = digitalRead(pinConfig.DIGITAL_IN_1);
       OD_RAM.x2300_digital_input_state[1] = digitalRead(pinConfig.DIGITAL_IN_2);

       // --- Encoder ---
       // (only if encoder module is enabled in this build)

       // --- System ---
       OD_RAM.x2503_uptime_seconds = millis() / 1000;
       OD_RAM.x2504_free_heap_bytes = esp_get_free_heap_size();
   }

3. In main/src/canopen/DeviceRouter.cpp:

   Replace handleMotorAct() — use native OD entries via UC2_OD_Indices.h:

   #include "UC2_OD_Indices.h"

   cJSON* DeviceRouter::handleMotorAct(cJSON* doc) {
       cJSON* motor = cJSON_GetObjectItem(doc, "motor");
       if (!motor) return nullptr;
       cJSON* steppers = cJSON_GetObjectItem(motor, "steppers");
       if (!steppers) return nullptr;

       int n = cJSON_GetArraySize(steppers);
       uint8_t cmdMask = 0;       // accumulated command bits
       uint8_t targetNode = 0;    // we assume one node per call

       for (int i = 0; i < n; i++) {
           cJSON* s = cJSON_GetArrayItem(steppers, i);
           int stepperid = cJSON_GetObjectItem(s, "stepperid")->valueint;
           uint8_t nodeId = stepperIdToNodeId(stepperid);
           targetNode = nodeId;
           uint8_t axis = stepperIdToAxisOnNode(stepperid);

           int32_t  pos   = jsonInt(s, "position", 0);
           uint32_t speed = jsonInt(s, "speed", 15000);
           uint32_t accel = jsonInt(s, "accel", 100000);
           uint8_t  isabs = jsonInt(s, "isabs", 0);

           CANopenModule::writeSDO(nodeId, UC2_OD::MOTOR_TARGET_POSITION, axis+1,
                                   (uint8_t*)&pos, 4);
           CANopenModule::writeSDO(nodeId, UC2_OD::MOTOR_SPEED, axis+1,
                                   (uint8_t*)&speed, 4);
           CANopenModule::writeSDO(nodeId, UC2_OD::MOTOR_ACCELERATION, axis+1,
                                   (uint8_t*)&accel, 4);
           CANopenModule::writeSDO(nodeId, UC2_OD::MOTOR_IS_ABSOLUTE, axis+1,
                                   &isabs, 1);

           cmdMask |= (1 << axis);
       }

       // Write the command word LAST — this is the trigger
       CANopenModule::writeSDO(targetNode, UC2_OD::MOTOR_COMMAND_WORD, 0,
                               &cmdMask, 1);

       cJSON* resp = cJSON_CreateObject();
       cJSON_AddNumberToObject(resp, "return", 1);
       return resp;
   }

   Replace handleMotorGet() — use native int32 OD reads:

   cJSON* DeviceRouter::handleMotorGet(cJSON* doc) {
       cJSON* resp = cJSON_CreateObject();
       cJSON* steppers = cJSON_AddArrayToObject(resp, "steppers");

       for (int stepperid = 0; stepperid < 4; stepperid++) {
           uint8_t nodeId = stepperIdToNodeId(stepperid);
           uint8_t axis   = stepperIdToAxisOnNode(stepperid);

           int32_t pos = 0;
           uint8_t status = 0;
           size_t  rd = 0;

           CANopenModule::readSDO(nodeId, UC2_OD::MOTOR_ACTUAL_POSITION, axis+1,
                                  (uint8_t*)&pos, 4, &rd);
           CANopenModule::readSDO(nodeId, UC2_OD::MOTOR_STATUS_WORD, axis+1,
                                  &status, 1, &rd);

           cJSON* s = cJSON_CreateObject();
           cJSON_AddNumberToObject(s, "stepperid", stepperid);
           cJSON_AddNumberToObject(s, "position",  pos);
           cJSON_AddBoolToObject  (s, "isRunning", (status & 0x01) != 0);
           cJSON_AddBoolToObject  (s, "isHomed",   (status & 0x02) != 0);
           cJSON_AddItemToArray(steppers, s);
       }

       cJSON_AddNumberToObject(resp, "return", 1);
       return resp;
   }

4. ALSO add helper handlers for the modules that previously had no DeviceRouter coverage:
   - handleLaserAct  (writes UC2_OD::LASER_PWM_VALUE)
   - handleLedAct    (writes mode/brightness/colour, see PR-9 for full pattern support)
   - handleHomeAct   (writes homing params + sets command word)
   - handleStateGet  (reads UC2_OD::FIRMWARE_VERSION_STRING, BOARD_NAME, UPTIME, FREE_HEAP)
   - handleTmcAct    (writes UC2_OD::TMC_*)

5. Remove the old workaround comments and the posHi/posLo arithmetic everywhere.

6. SDO node-id mapping refinement — `stepperIdToNodeId` and a new
   `stepperIdToAxisOnNode` should be data-driven from a lookup table:

   struct StepperRoute { uint8_t nodeId; uint8_t axis; };
   constexpr StepperRoute STEPPER_ROUTES[4] = {
       {UC2_NODE::MOTOR_A, 0},   // stepperid 0 → A axis on node 14
       {UC2_NODE::MOTOR_X, 0},   // stepperid 1 → X axis on node 11
       {UC2_NODE::MOTOR_Y, 0},   // stepperid 2 → Y axis on node 12
       {UC2_NODE::MOTOR_Z, 0},   // stepperid 3 → Z axis on node 13
   };

   This mapping comes from runtimeConfig — the user can override it via NVS to
   support multi-axis-per-node configurations.

DO NOT in this PR:
- Touch CANopenNode library internals
- Add OTA (PR-11)
- Implement TPDO push (PR-8)
- Implement LED bulk pixel transfer (PR-9)
- Delete can_transport.cpp (PR-12)

Verification:
- Master + slave compile and link
- Send {"task":"/motor_act","motor":{"steppers":[{"stepperid":1,"position":1000,"speed":15000}]}}
  Motor moves 1000 steps. NO split-uint16 logging on the slave.
- Send {"task":"/motor_get"}
  Returns {"steppers":[{"stepperid":1,"position":1000,"isRunning":false,"isHomed":false}]}
- Acceleration is now propagated end-to-end (verify by setting accel=10000 vs accel=200000
  and observing the move profile).
- grep -r "posHi\|posLo\|0x6401\|0x6200.*trigger" main/src/canopen/   should return empty
```

---

## PR-8: Slave-pushed state via TPDO (no more polling)

**Branch:** `feature/canopen-tpdo-push`
**Depends on:** PR-7
**Estimated scope:** ~30 lines in `CANopenModule.cpp`, ~50 lines in `DeviceRouter.cpp`

This PR answers your direct question: **yes, slaves can actively push state to the master.**
That's exactly what TPDOs are designed for. The slave doesn't wait for the master to ask
— it broadcasts a CAN frame whenever the data changes (event-driven) or at a fixed
interval (timer-driven). The master subscribes by configuring its RPDOs to receive on the
slave's COB-ID.

### Claude Code prompt

```
Goal: Stop polling motor state via SDO. Instead, the slave PUSHES motor position +
status updates via TPDO1 whenever they change. The master maintains a local cache that
the Pi can query via /motor_get without any CAN traffic.

How TPDOs work in CANopenNode:
- The slave's CO_process_TPDO() at every CO_tmrTask cycle (1ms) checks each TPDO for
  changes and transmits if needed
- Event-driven mode (transmission_type = 254): transmits whenever any mapped variable
  changes, respecting inhibit_time_ms (minimum gap)
- Plus a fallback event_timer_ms (max stale time) — guarantees a refresh even if nothing
  changes
- The CANopenNode library does ALL of this automatically once the OD entries are
  PDO-mapped. Our job is to (a) ensure the OD has TPDO mappings, (b) update OD_RAM
  whenever the source value changes.

Steps:

1. The TPDO mappings are already declared in the registry YAML (tpdo1 maps motor
   actual_position[1] + status_word[1]). Verify the generated OD.c sets up
   TPDO 0x1800 with COB-ID 0x180+nodeId and the mapping at 0x1A00.

   If your CANopenEditor export doesn't have this, set it manually:
   - 0x1800:01 (COB-ID)         = 0x180 + node_id
   - 0x1800:02 (transmission)   = 254 (event-driven)
   - 0x1800:03 (inhibit time)   = 5  (5ms minimum gap)
   - 0x1800:05 (event timer)    = 100 (100ms fallback)
   - 0x1A00:00 (number of mapped) = 2
   - 0x1A00:01 (object 1)       = 0x20010120  (0x2001:01 = 32 bits)
   - 0x1A00:02 (object 2)       = 0x20040108  (0x2004:01 = 8 bits)

2. In CANopenModule.cpp syncModulesToTpdo() — make sure motor position writes happen
   on EVERY tick, but use the change-detection wrapper from CANopenNode to flag changes:

   // CANopenNode v4 provides OD_requestTPDO() to manually request a transmission
   // when an unmapped event happens. For mapped variables, just write the value.
   // The library detects changes between cycles.

   for (uint8_t axis = 0; axis < 4; axis++) {
       int32_t newPos = focusMotor->getCurrentPosition(static_cast<Stepper>(axis));
       OD_RAM.x2001_motor_actual_position[axis] = newPos;
       // CANopenNode auto-detects the change and sends TPDO1 if mapped & event-driven
   }

3. On the master side, the RPDO consumer is symmetric. CANopenNode automatically
   receives the slave's TPDO frame and writes the values into the MASTER's local OD_RAM.
   This means: if the master is configured with an RPDO that mirrors the slave's TPDO1,
   then on the master OD_RAM.x2001_motor_actual_position[axis] becomes a CACHED value
   that updates whenever the slave broadcasts.

   For this we need a per-slave RPDO on the master. CANopenNode masters typically
   handle this by adding one RPDO per monitored slave node, with COB-IDs offset by
   node-id. Add a helper:

   void CANopenModule::addRemoteSlaveTracking(uint8_t slaveNodeId) {
       // Configure a master-side RPDO to receive TPDO1 from slaveNodeId
       // COB-ID = 0x180 + slaveNodeId
       // Map into a local mirror struct keyed by node-id
   }

   Call this in setup() for each known slave node-id from runtimeConfig.

4. Maintain a master-side mirror cache:

   struct RemoteSlaveState {
       int32_t  motorPosition[4];
       uint8_t  motorStatus[4];
       uint8_t  endstopState[2];
       uint16_t analogIn[2];
       uint32_t lastUpdateMs;       // millis() of last TPDO received
       bool     online;
   };
   static RemoteSlaveState remoteSlaves[128];  // index = node-id

   Update this whenever an RPDO arrives. CANopenNode lets you register a callback
   on RPDO reception, OR you poll OD_RAM in the master's CO_tmrTask.

5. Rewrite handleMotorGet() to read from the cache instead of issuing SDO reads:

   cJSON* DeviceRouter::handleMotorGet(cJSON* doc) {
       cJSON* resp = cJSON_CreateObject();
       cJSON* steppers = cJSON_AddArrayToObject(resp, "steppers");

       for (int stepperid = 0; stepperid < 4; stepperid++) {
           uint8_t nodeId = stepperIdToNodeId(stepperid);
           uint8_t axis   = stepperIdToAxisOnNode(stepperid);

           // NO SDO call — read from local cache
           const RemoteSlaveState& cache = remoteSlaves[nodeId];
           int32_t pos    = cache.motorPosition[axis];
           uint8_t status = cache.motorStatus[axis];
           uint32_t age   = millis() - cache.lastUpdateMs;

           cJSON* s = cJSON_CreateObject();
           cJSON_AddNumberToObject(s, "stepperid", stepperid);
           cJSON_AddNumberToObject(s, "position",  pos);
           cJSON_AddBoolToObject  (s, "isRunning", (status & 0x01) != 0);
           cJSON_AddBoolToObject  (s, "isHomed",   (status & 0x02) != 0);
           cJSON_AddNumberToObject(s, "age_ms",    age);
           cJSON_AddBoolToObject  (s, "online",    cache.online && age < 1000);
           cJSON_AddItemToArray(steppers, s);
       }

       cJSON_AddNumberToObject(resp, "return", 1);
       return resp;
   }

6. Add an unsolicited notification path: when TPDO1 arrives at the master and the
   motor status word transitions from "running" to "not running", send a JSON
   notification to the Pi over serial:

   {"event":"motor_complete","stepperid":1,"position":1000}

   This is the equivalent of QID completion reporting but driven by the OD instead of
   custom CAN messages. Wire it through the existing QidRegistry::reportActionDone() so
   the Pi gets the same notification format it already expects.

DO NOT in this PR:
- Implement OTA (PR-11)
- Touch the master→slave (RPDO) path
- Touch laser/LED/galvo TPDOs (PR-9, PR-10)

Verification:
- Run candump on the bus while the slave is running
  Expected: 0x18B (TPDO1 from node 11) frames every ~100ms even when idle, more
  frequent during a move
- Stop the slave's motor mid-move via /motor_act with stop bit set
- Master should receive a TPDO almost immediately (within inhibit time = 5ms) with
  status_word bit 0 cleared
- /motor_get on master returns immediately (cached, no SDO)
- Pi sees an unsolicited "motor_complete" event when a move finishes
```

---

## PR-9: Laser + LED full coverage (including pattern support and pixel arrays)

**Branch:** `feature/canopen-illumination`
**Depends on:** PR-7 (PR-8 optional but recommended)
**Estimated scope:** ~150 lines in CANopenModule, DeviceRouter, LedController extensions

### Claude Code prompt

```
Goal: All laser channels controllable via PDO/SDO. LED supports full pattern set
(uniform colour, half-array, ring, custom pixel array via SDO block transfer).

Steps:

1. Laser side — already mostly done in PR-7. Verify these work:
   /laser_act with channel + value → SDO write to UC2_OD::LASER_PWM_VALUE[channel]
   /laser_set for frequency / resolution / despeckle → SDO writes to other OD entries
   Safety state read-back via UC2_OD::LASER_SAFETY_STATE (TPDO3-mapped)

2. In CANopenModule::syncRpdoToModules() add laser handling:

   for (uint8_t ch = 0; ch < 4; ch++) {
       uint16_t v = OD_RAM.x2100_laser_pwm_value[ch];
       if (v != lastLaserVal[ch]) {
           lastLaserVal[ch] = v;
           laserController->setLaserVal(ch, v);
       }
   }

3. LED simple modes (current):
   In syncRpdoToModules():
   if (OD_RAM.x2200_led_array_mode != lastLedMode) {
       lastLedMode = OD_RAM.x2200_led_array_mode;
       ledController->setMode(
           OD_RAM.x2200_led_array_mode,
           OD_RAM.x2201_led_brightness,
           OD_RAM.x2202_led_uniform_colour);
   }

4. LED pattern selection (built-in animations):
   if (OD_RAM.x2220_led_pattern_id != lastPatternId) {
       lastPatternId = OD_RAM.x2220_led_pattern_id;
       ledController->setPattern(lastPatternId,
           OD_RAM.x2221_led_pattern_speed);
   }

   Patterns to implement in LedController:
   - 0 = none (use mode/uniform colour)
   - 1 = rainbow
   - 2 = breathe
   - 3 = chase
   - 4 = fire
   - 5 = sparkle
   - 6 = heatmap
   - 7 = spiral

5. LED full pixel array via SDO segmented transfer:

   The OD entry UC2_OD::LED_PIXEL_DATA (0x2210) is type DOMAIN. CANopenNode v4
   handles segmented SDO transfers automatically — when the master writes more than
   7 bytes to a DOMAIN entry, it switches to segmented mode (or block mode for >889 bytes).

   On the slave side, we register a write callback:

   static OD_extension_t ledPixelExt;
   static uint8_t ledPixelBuffer[3 * MAX_LEDS];   // 3 bytes per pixel (RGB)
   static size_t  ledPixelBytesReceived = 0;

   static ODR_t onLedPixelWrite(OD_stream_t* stream, const void* buf,
                                 OD_size_t count, OD_size_t* countWritten) {
       if (ledPixelBytesReceived + count > sizeof(ledPixelBuffer)) {
           return ODR_OUT_OF_MEM;
       }
       memcpy(ledPixelBuffer + ledPixelBytesReceived, buf, count);
       ledPixelBytesReceived += count;
       *countWritten = count;

       // If this was the last segment, apply to the LED strip
       if (stream->dataOffset + count >= stream->dataLength) {
           uint16_t pixelCount = ledPixelBytesReceived / 3;
           ledController->setPixels(ledPixelBuffer, pixelCount);
           ledPixelBytesReceived = 0;
       }
       return ODR_OK;
   }

   In CANopenModule::setup() after CO_CANopenInit():
   OD_entry_t* entry = OD_find(OD, UC2_OD::LED_PIXEL_DATA);
   ledPixelExt.object = nullptr;
   ledPixelExt.read   = nullptr;
   ledPixelExt.write  = onLedPixelWrite;
   OD_extension_init(entry, &ledPixelExt);

6. Master side — handleLedAct() in DeviceRouter:

   cJSON* DeviceRouter::handleLedAct(cJSON* doc) {
       cJSON* led = cJSON_GetObjectItem(doc, "led");
       if (!led) return nullptr;

       uint8_t nodeId = UC2_NODE::LED;

       // Mode-based fill
       cJSON* mode = cJSON_GetObjectItem(led, "LEDArrMode");
       if (mode) {
           uint8_t m = mode->valueint;
           CANopenModule::writeSDO(nodeId, UC2_OD::LED_ARRAY_MODE, 0, &m, 1);
       }

       // Brightness
       cJSON* br = cJSON_GetObjectItem(led, "brightness");
       if (br) {
           uint8_t b = br->valueint;
           CANopenModule::writeSDO(nodeId, UC2_OD::LED_BRIGHTNESS, 0, &b, 1);
       }

       // Per-pixel array — bulk transfer
       cJSON* arr = cJSON_GetObjectItem(led, "led_array");
       if (arr) {
           int n = cJSON_GetArraySize(arr);
           uint8_t* buf = (uint8_t*)malloc(n * 3);
           for (int i = 0; i < n; i++) {
               cJSON* px = cJSON_GetArrayItem(arr, i);
               buf[i*3+0] = jsonInt(px, "r", 0);
               buf[i*3+1] = jsonInt(px, "g", 0);
               buf[i*3+2] = jsonInt(px, "b", 0);
           }
           // SDO domain write (segmented)
           CANopenModule::writeSDODomain(nodeId, UC2_OD::LED_PIXEL_DATA, 0,
                                        buf, n * 3);
           free(buf);
       }

       cJSON* resp = cJSON_CreateObject();
       cJSON_AddNumberToObject(resp, "return", 1);
       return resp;
   }

7. Add CANopenModule::writeSDODomain() helper that wraps CO_SDOclientDownloadInitiate
   for arbitrary-length data. The MWE has examples of this.

DO NOT in this PR:
- OTA (separate)
- Galvo (PR-10)

Verification:
- {"task":"/laser_act","laser":{"channel":0,"value":500}} → laser at half power
- {"task":"/led_act","led":{"LEDArrMode":1,"brightness":200}} → all LEDs white
- {"task":"/led_act","led":{"led_array":[{"id":0,"r":255,"g":0,"b":0}, ...]}}
  → individual pixels light up correctly
- candump shows segmented SDO frames during pixel array transfer
```

---

## PR-10: Galvo / scanner support

**Branch:** `feature/canopen-galvo`
**Depends on:** PR-7
**Estimated scope:** ~200 lines, new GalvoController integration

### Claude Code prompt

```
Goal: Galvo nodes (node-id 30+) accept target XY positions via RPDO4 and run line/raster
scans autonomously based on parameters set via SDO.

Steps:

1. The OD entries 0x2600-0x260F are already in the registry. Verify they're in OD.h
   after running tools/regenerate_all.py + CANopenEditor export.

2. In main/src/scanner/GalvoController.cpp (existing file or new):
   - Implement setTarget(axis, value) — writes DAC channel
   - Implement startLineScan(), startRasterScan(), stop()
   - State machine driven by command word

3. In CANopenModule::syncRpdoToModules() add galvo handling:

   if (runtimeConfig.galvo) {
       // Real-time XY target via RPDO4 (every CAN frame = ~250us latency)
       int32_t tx = OD_RAM.x2600_galvo_target_position[0];
       int32_t ty = OD_RAM.x2600_galvo_target_position[1];
       if (tx != lastGalvoX || ty != lastGalvoY) {
           lastGalvoX = tx; lastGalvoY = ty;
           galvoController->setTarget(0, tx);
           galvoController->setTarget(1, ty);
       }

       // Command word
       uint8_t cmd = OD_RAM.x2602_galvo_command_word;
       if (cmd != lastGalvoCmd) {
           lastGalvoCmd = cmd;
           switch (cmd) {
               case 0: galvoController->stop(); break;
               case 1: /* goto already handled above */ break;
               case 2: galvoController->startLineScan(
                           OD_RAM.x260B_galvo_x_start,
                           OD_RAM.x260E_galvo_y_step,
                           OD_RAM.x2604_galvo_scan_speed);
                       break;
               case 3: galvoController->startRasterScan(
                           OD_RAM.x260B_galvo_x_start, OD_RAM.x260C_galvo_y_start,
                           OD_RAM.x260D_galvo_x_step, OD_RAM.x260E_galvo_y_step,
                           OD_RAM.x2605_galvo_n_steps_line,
                           OD_RAM.x2606_galvo_n_steps_pixel,
                           OD_RAM.x2604_galvo_scan_speed);
                       break;
               case 5: galvoController->stop(); break;
           }
           OD_RAM.x2602_galvo_command_word = 0;
       }
   }

4. In syncModulesToTpdo() update galvo status:

   if (runtimeConfig.galvo) {
       OD_RAM.x2601_galvo_actual_position[0] = galvoController->getActual(0);
       OD_RAM.x2601_galvo_actual_position[1] = galvoController->getActual(1);
       uint8_t status = 0;
       if (galvoController->isMoving())     status |= 0x01;
       if (galvoController->isScanActive()) status |= 0x02;
       if (galvoController->scanComplete()) status |= 0x04;
       OD_RAM.x2603_galvo_status_word = status;
   }

5. DeviceRouter handlers — handleGalvoAct():

   cJSON* DeviceRouter::handleGalvoAct(cJSON* doc) {
       cJSON* g = cJSON_GetObjectItem(doc, "galvo");
       if (!g) return nullptr;

       uint8_t nodeId = UC2_NODE::GALVO;

       cJSON* tx = cJSON_GetObjectItem(g, "x");
       cJSON* ty = cJSON_GetObjectItem(g, "y");
       if (tx) {
           int32_t v = tx->valueint;
           CANopenModule::writeSDO(nodeId, UC2_OD::GALVO_TARGET_POSITION, 1,
                                   (uint8_t*)&v, 4);
       }
       if (ty) {
           int32_t v = ty->valueint;
           CANopenModule::writeSDO(nodeId, UC2_OD::GALVO_TARGET_POSITION, 2,
                                   (uint8_t*)&v, 4);
       }

       cJSON* scan = cJSON_GetObjectItem(g, "scan");
       if (scan) {
           // Push all scan parameters via SDO, then issue command
           writeIntSDO(nodeId, UC2_OD::GALVO_X_START,        jsonInt(scan, "xStart", 0));
           writeIntSDO(nodeId, UC2_OD::GALVO_Y_START,        jsonInt(scan, "yStart", 0));
           writeIntSDO(nodeId, UC2_OD::GALVO_X_STEP,         jsonInt(scan, "xStep", 500));
           writeIntSDO(nodeId, UC2_OD::GALVO_Y_STEP,         jsonInt(scan, "yStep", 500));
           writeIntSDO(nodeId, UC2_OD::GALVO_N_STEPS_LINE,   jsonInt(scan, "nX", 100));
           writeIntSDO(nodeId, UC2_OD::GALVO_N_STEPS_PIXEL,  jsonInt(scan, "nY", 100));
           writeIntSDO(nodeId, UC2_OD::GALVO_SCAN_SPEED,     jsonInt(scan, "speed", 1000));

           uint8_t cmd = 3;  // raster scan
           CANopenModule::writeSDO(nodeId, UC2_OD::GALVO_COMMAND_WORD, 0, &cmd, 1);
       }

       cJSON* resp = cJSON_CreateObject();
       cJSON_AddNumberToObject(resp, "return", 1);
       return resp;
   }

6. The existing /motor_act stagescan command — route it through the same galvo handler
   if the node has galvo enabled, OR keep stagescan as a motor-controller command on
   nodes that have both motor and galvo.

Verification:
- /galvo_act with x/y → galvo mirrors move within ~250us (RPDO4 latency)
- /galvo_act with scan params → raster scan runs autonomously
- /galvo_get returns current position + scan status from cache (TPDO4 push)
```

---

## PR-11: OTA via CANopen SDO block transfer

**Branch:** `feature/canopen-ota`
**Depends on:** PR-7 (PR-9 helpful for SDO domain experience)
**Estimated scope:** ~300 lines, new CanOpenOTA module
**Safety-critical:** test on a sacrificial board first.

### Claude Code prompt

```
Goal: Master can flash a slave's firmware over CAN by streaming a binary into the slave's
OTA partition via SDO segmented or block transfer to UC2_OD::OTA_FIRMWARE_DATA (0x2F00).

[Same content as the existing PR-8 OTA prompt in claude_code_tasks_canopen_integration.md
with these refinements:]

1. Use UC2_OD::OTA_FIRMWARE_DATA, UC2_OD::OTA_FIRMWARE_SIZE, UC2_OD::OTA_FIRMWARE_CRC32,
   UC2_OD::OTA_STATUS, UC2_OD::OTA_BYTES_RECEIVED, UC2_OD::OTA_ERROR_CODE
   from the generated UC2_OD_Indices.h — NO magic numbers.

2. The 0x2F00 entry must be a DOMAIN type with an OD write extension callback that
   streams to esp_ota_write(). See PR-9 for the LED_PIXEL_DATA pattern — same approach.

3. Master side: read firmware binary from serial (or SD card) in 512-byte chunks, feed
   to CO_SDOclientDownload() in a loop. CANopenNode handles the segmented or block
   transfer protocol automatically.

4. Status reporting: OD_RAM.x2F03_ota_status is TPDO3-mapped, so the master automatically
   sees progress updates without polling.

5. Verify CRC32 against UC2_OD::OTA_FIRMWARE_CRC32 before calling esp_ota_set_boot_partition.

6. Add Python CLI tool tools/uc2_ota_can.py that uses python-canopen + Waveshare USB-CAN
   to flash a slave directly without going through the master ESP32:

   python tools/uc2_ota_can.py --node 11 --binary firmware.bin

   This is incredibly useful for development — you can flash any slave from your laptop.

DO NOT delete the old binary OTA serial path until PR-12.
```

---

## PR-12: Remove old transport, stubs, dead flags

**Branch:** `cleanup/remove-old-can-transport`
**Depends on:** PR-7 through PR-11
**Estimated scope:** ~2500 lines deleted, ~100 lines wrapped

### Claude Code prompt

```
Goal: Delete every trace of the old ISO-TP based CAN transport. After this PR, the only
CAN code is CANopenModule + DeviceRouter + CanOpenOTA + the unmodified CANopenNode library.

Files to DELETE (use git rm):
- main/src/can/can_transport.cpp                    (~1700 lines)
- main/src/can/can_transport.h
- main/src/can/can_messagetype.h
- main/src/can/can_controller_stubs.cpp             (the no-op shim)
- main/src/can/iso-tp-twai/                         (entire directory)
- lib/ESP-CAN-ISO-TP-TWAI/                          (if present as local copy)

Build flags to REMOVE from platformio.ini and from any #ifdef blocks in source files:
- CAN_SEND_COMMANDS
- CAN_RECEIVE_MOTOR
- CAN_RECEIVE_LASER
- CAN_RECEIVE_LED
- CAN_RECEIVE_HOME
- CAN_RECEIVE_TMC
- CAN_RECEIVE_DAC
- CAN_SLAVE_MOTOR
- CAN_BUS_ENABLED  (if it only gates the old transport)

Build flags to KEEP:
- CAN_CONTROLLER_CANOPEN  (single switch — enables CANopenModule)
- NODE_ROLE               (1=master, 2=slave) but consider replacing with
                          runtime check on runtimeConfig.canRole instead
- UC2_NODE_ID             (default node-id; NVS overrides)

Build environments to REMOVE:
- env:UC2_3_CAN_HAT_Master                  → replaced by env:UC2_canopen_master
- env:UC2_3_CAN_HAT_Master_v2               → replaced by env:UC2_canopen_master
- env:seeed_xiao_esp32s3_can_slave_motor    → replaced by env:UC2_canopen_slave
- env:seeed_xiao_esp32s3_can_slave_*        → all replaced by env:UC2_canopen_slave
- env:seeed_xiao_esp32c3_can_slave_motor    → replaced by env:UC2_canopen_slave_c3

Search-and-destroy for dead #ifdefs:

  grep -rn "CAN_SEND_COMMANDS\|CAN_RECEIVE_\|CAN_SLAVE_MOTOR" main/

For each hit, remove the entire #ifdef/#else/#endif block (keeping the body that was
the active one — usually the master or slave path).

For the can_controller_stubs.cpp removal: every controller file (FocusMotor.cpp,
LaserController.cpp, etc.) that previously called can_controller::sendFrame() should
have its CAN call wrapped in #ifndef CAN_CONTROLLER_CANOPEN. After PR-12, those wraps
become unconditional removal — the controllers no longer have ANY direct CAN code.
The OD↔Module sync in CANopenModule.cpp handles all CAN-side state.

Specifically check:
- main/src/dial/DialController.cpp — did you finish migrating it? (PR-7 mentions it)
  If not, add the migration here.
- main/src/motor/FocusMotor.cpp — should not call can_controller::* anywhere
- main/src/laser/LaserController.cpp — same
- main/src/led/LedController.cpp — same

Web flasher:
- Update youseetoo.github.io firmware list to point at the new env names
- Add a docs note explaining the env rename

Verification:
- All remaining environments compile:
    pio run -e UC2_2                    (standalone)
    pio run -e UC2_3                    (standalone)
    pio run -e UC2_4                    (standalone)
    pio run -e UC2_canopen_master       (CAN master)
    pio run -e UC2_canopen_slave        (CAN slave)

- Final grep — should return ZERO matches in main/src/:
    grep -rn "ISO.TP\|can_transport\|can_controller\|CanIsoTp\|CAN_SEND_COMMANDS\|CAN_RECEIVE_MOTOR" main/src/

- Functional smoke test:
    Master serial: {"task":"/can_get"}     → returns CANopen status
    Master serial: {"task":"/motor_act",...}  → motor moves
    Master serial: {"task":"/laser_act",...}  → laser fires
    Master serial: {"task":"/led_act",...}    → LEDs update
    Master serial: {"task":"/galvo_act",...}  → galvo scans
    Master serial: {"task":"/ota_start",...}  → slave reflashes

- Code line count:
    wc -l main/src/canopen/*.cpp main/src/canopen/*.h
    Should be roughly:
      CANopenModule.cpp     ~500
      CANopenModule.h        ~80
      DeviceRouter.cpp      ~400
      DeviceRouter.h         ~60
      CanOpenOTA.cpp        ~250
      CanOpenOTA.h           ~50
      UC2_OD_Indices.h     ~150 (generated)
    Total UC2-authored CAN code: ~1500 lines, down from ~2500+ in can_transport.cpp alone.
```

---

## PR-13: Waveshare USB-CAN integration + Pi-direct CANopen

**Branch:** `feature/waveshare-canopen`
**Depends on:** PR-7 onwards (anywhere after the registry exists)
**Estimated scope:** Pi-side Python only — no firmware change

### Claude Code prompt

```
Goal: Pi can talk CANopen directly to satellite nodes via the Waveshare USB-CAN adapter,
bypassing the master ESP32. Useful for diagnostics, scripting, and field service.

Steps:

1. Install python-canopen and Waveshare drivers:
   PYTHON/uc2_canopen/requirements.txt:
     canopen>=2.2.0
     python-can>=4.0.0
     pyserial>=3.5

2. PYTHON/uc2_canopen/uc2_client.py — high-level wrapper:

   import canopen
   from .uc2_indices import OD, NODE, OD_NAMES

   class UC2CanClient:
       def __init__(self, channel='can0', bustype='socketcan', bitrate=500000,
                    eds_path='DOCUMENTATION/openUC2_satellite.eds'):
           self.network = canopen.Network()
           self.network.connect(channel=channel, bustype=bustype, bitrate=bitrate)
           self.eds_path = eds_path
           self.nodes = {}

       def add_node(self, node_id):
           node = self.network.add_node(node_id, self.eds_path)
           self.nodes[node_id] = node
           return node

       def move_motor(self, axis, target, speed=15000, accel=100000, isabs=True):
           node_id = NODE.MOTOR_X if axis == 1 else NODE.MOTOR_Y if axis == 2 else NODE.MOTOR_Z
           if node_id not in self.nodes:
               self.add_node(node_id)
           node = self.nodes[node_id]
           node.sdo[OD.MOTOR_TARGET_POSITION][1].raw = target
           node.sdo[OD.MOTOR_SPEED][1].raw = speed
           node.sdo[OD.MOTOR_ACCELERATION][1].raw = accel
           node.sdo[OD.MOTOR_IS_ABSOLUTE][1].raw = 1 if isabs else 0
           node.sdo[OD.MOTOR_COMMAND_WORD].raw = 0x01  # axis 0 trigger

       def get_motor_position(self, axis):
           # ... reads OD.MOTOR_ACTUAL_POSITION

       def set_laser(self, channel, value):
           # ... writes OD.LASER_PWM_VALUE

       def set_led_uniform(self, r, g, b, brightness=128):
           node = self.nodes.get(NODE.LED) or self.add_node(NODE.LED)
           node.sdo[OD.LED_BRIGHTNESS].raw = brightness
           colour = (r << 16) | (g << 8) | b
           node.sdo[OD.LED_UNIFORM_COLOUR].raw = colour
           node.sdo[OD.LED_ARRAY_MODE].raw = 1  # uniform on

       def set_led_pixels(self, pixel_list):
           node = self.nodes.get(NODE.LED) or self.add_node(NODE.LED)
           buf = bytearray()
           for px in pixel_list:
               buf += bytes([px['r'], px['g'], px['b']])
           node.sdo.download(OD.LED_PIXEL_DATA, 0, bytes(buf))

       def subscribe_motor_state(self, callback):
           # Hook TPDO1 (motor actual position + status) and call back on every update
           for node_id, node in self.nodes.items():
               node.tpdo[1].add_callback(lambda msg: callback(node_id, msg))

3. PYTHON/uc2_canopen/cli.py — command-line tool:

   uc2-can list-nodes
   uc2-can move --axis x --pos 1000 --speed 15000
   uc2-can monitor --node 11           # live TPDO updates
   uc2-can sdo-read 11 0x2001 1
   uc2-can sdo-write 11 0x2100 1 500
   uc2-can flash --node 11 firmware.bin
   uc2-can scan-bus                    # discover live nodes

4. Waveshare-specific config — the USB-CAN-A adapter exposes a serial-CAN bridge.
   On Linux, use the kernel's slcand driver to make it look like socketcan:

   sudo slcand -o -c -s6 /dev/ttyUSB0 can0
   sudo ip link set can0 up

   On macOS / Windows, use python-can's serial backend directly:
   bus = can.interface.Bus(bustype='slcan', channel='/dev/tty.usbserial-XXX', bitrate=500000)

   Document both setups in PYTHON/uc2_canopen/README.md.

5. Add example scripts:
   - examples/move_motor.py
   - examples/blink_led.py
   - examples/galvo_raster.py
   - examples/monitor_all.py    (subscribes to TPDOs from every node)

6. Optional: build a small Tkinter or web UI that uses uc2_client.py to provide a
   GUI for sending commands and monitoring TPDOs. Useful as a diagnostic tool when
   ImSwitch isn't running.

DO NOT in this PR:
- Modify any firmware code
- Change UC2-REST or ImSwitch — this is an alternative path, not a replacement

Verification:
- python -m uc2_canopen.cli scan-bus    finds all live nodes on the CAN bus
- python -m uc2_canopen.cli move --axis x --pos 1000   moves the X axis
- TPDO subscription receives motor position updates without polling
```

---

## Merge order

```
main (after PR #119 merges)
│
├── PR-6:  feature/canopen-registry          ← Generator infrastructure
│   │
│   ├── PR-7:  feature/canopen-uc2-od         ← Native UC2 OD, motor works without workaround
│   │   │
│   │   ├── PR-8:  feature/canopen-tpdo-push  ← Slaves push state actively (TPDO)
│   │   │
│   │   ├── PR-9:  feature/canopen-illumination  ← Laser + LED full coverage
│   │   │
│   │   ├── PR-10: feature/canopen-galvo      ← Galvo / scanner
│   │   │
│   │   ├── PR-11: feature/canopen-ota        ← Firmware update via SDO block
│   │   │
│   │   └── PR-13: feature/waveshare-canopen  ← Pi-direct CANopen via USB-CAN (parallel)
│   │
│   └── PR-12: cleanup/remove-old-can-transport ← After PR-7..11 merged
```

**PR-6 and PR-7 are the gating pair** — until those are merged, every later PR has nothing
to build on. Get them landed quickly. Then PRs 8-11 can be developed in parallel by
different people (or by Claude Code in separate sessions). PR-12 is the final cleanup
and must wait until everything else is verified working.

---

## What about the long term — generating a fully standalone OD?

Today the registry generator emits a stub `OD.c` and you run CANopenEditor manually to
get the full table. The reason is that CANopenNode v4's OD descriptor format is detailed
(per-entry pointers into RAM, type codes, attribute flags, sub-index tables) and writing
a complete generator takes time.

The migration path away from CANopenEditor:

1. **Phase 1 (now):** Registry → EDS → CANopenEditor → OD.h / OD.c (semi-automatic)
2. **Phase 2 (after PR-7):** Extend `regenerate_all.py` to emit a fully working `OD.c`
   that matches the format CANopenEditor produces. The format is documented in the
   CANopenNode source headers. Once this works, CANopenEditor becomes optional (used only
   for visual review of the EDS).
3. **Phase 3 (long term):** Drop the trainer/CANopenEditor dependency entirely. The
   generator is the canonical tool. New hardware → edit YAML → run generator → done.

The generator script has a `generate_od_c()` function with a TODO marker — that's the
extension point. When you're ready, the work is roughly 200-300 lines of additional
template code to emit the full OD descriptor table. It's a self-contained task you could
hand to Claude Code as a future PR-14.

---

## Quick recap of your specific questions

**Q: Should we have a registry file for OD indices instead of magic numbers?**

Yes, and the infrastructure already exists in this output set:

| File | What it is |
|------|------------|
| `tools/uc2_canopen_registry.yaml` | Single source of truth — every OD entry, with name, type, default, PDO mapping, sub-indices, C++ field hint |
| `tools/regenerate_all.py` | Reads the YAML, generates the EDS, OD.h/OD.c, C++ constants header, Python constants, and markdown reference doc |
| `main/src/canopen/UC2_OD_Indices.h` | Generated C++ header — `namespace UC2_OD { constexpr uint16_t MOTOR_TARGET_POSITION = 0x2000; ... }` |
| `PYTHON/uc2_canopen/uc2_indices.py` | Generated Python module — same names, same indices, used by the Waveshare scripts |
| `DOCUMENTATION/UC2_CANopen_Parameters.md` | Generated human-readable reference — one table per module |

Concrete example of the change in C++:

```cpp
// BEFORE (current state in PR #119):
size_t readSize;
uint8_t isRunning = 0;
CANopenModule::readSDO(nodeId, 0x6000, 0x01, &isRunning, 1, &readSize);

// AFTER (PR-6 onwards):
#include "UC2_OD_Indices.h"
size_t readSize;
uint8_t status = 0;
CANopenModule::readSDO(nodeId,
    UC2_OD::MOTOR_STATUS_WORD,    // 0x2004 — typo-proof, IDE-completable
    axis + 1,                     // sub-index = 1-based axis
    &status, sizeof(status), &readSize);
bool isRunning = (status & 0x01) != 0;
```

The benefit isn't just the named constant. It's that **adding a new parameter is one
YAML edit + one script run** — no hand-editing of the EDS, no hand-editing of OD.h, no
scattered #ifdef adjustments, no risk of the four representations drifting apart.

---

**Q: Can a slave actively push state to the master?**

Yes — that's exactly what TPDO is designed for, and it's how all status updates should
work. The slave's `CO_tmrTask` runs every 1 ms, checks if any TPDO-mapped OD entry has
changed, and if so transmits a CAN frame immediately (subject to the inhibit time, which
prevents busy-bus floods).

The master's CANopenNode stack is configured with matching RPDOs that consume those
frames automatically — when a TPDO arrives, the master's local `OD_RAM` is updated
without any application code running. This means the master maintains a live mirror of
every slave's state, and `/motor_get` from the Pi reads that mirror with zero CAN
traffic.

PR-8 wires this end-to-end. After PR-8:
- Motor position changes propagate slave→master in ~5 ms (one TPDO frame)
- Endstop triggers fire as TPDO2 within 5 ms of GPIO change
- Encoder positions stream as TPDO3 every 50 ms
- The master never polls anything via SDO during normal operation
- When a move finishes, the master detects the status word transition and emits an
  unsolicited `{"event":"motor_complete"}` JSON to the Pi over serial

The CANopenNode v4 mechanism that makes this work is the `OD_extension_t.flagsPDO`
bitfield: when application code clears a flag bit corresponding to a mapped variable,
the library treats it as "value changed" and queues a TPDO transmission on the next
`CO_process_TPDO()` call. For most cases you don't even need to touch the flags — just
write the new value to `OD_RAM` and the library detects the change automatically.

---

**Q: Should we have a single Python interface to define parameters and derive everything?**

That's exactly what `tools/uc2_canopen_registry.yaml` + `tools/regenerate_all.py`
provides. One YAML file defines every parameter, and one Python script produces:

1. The CiA 306 EDS file (`openUC2_satellite.eds`) for CANopenEditor and `python-canopen`
2. The CANopenNode `OD.h` and `OD.c` source files for the firmware
3. The C++ named constants header (`UC2_OD_Indices.h`)
4. The Python named constants module (`uc2_indices.py`)
5. The human-readable parameter reference (`UC2_CANopen_Parameters.md`)
6. (Optional) A JSON schema for validating ImSwitch / UC2-REST commands

A CI check runs `python tools/regenerate_all.py && git diff --exit-code` to catch any
drift — if someone hand-edits a generated file, CI fails until they update the YAML and
regenerate.

**Adding new hardware** (e.g. a temperature controller for a heated stage) is one entry:

```yaml
temperature:
  base_index: 0x2900
  description: "Heated stage temperature controller"
  cpp_class: "TempController"
  entries:
    - index: 0x2900
      name: setpoint
      type: I16             # 0.1 °C resolution, signed
      access: rw
      default: 250          # 25.0 °C
      unit: "0.1 degC"
      description: "Target temperature"
      pdo: rpdo3            # mapped into a master→slave PDO
    - index: 0x2901
      name: actual
      type: I16
      access: ro
      default: 0
      unit: "0.1 degC"
      pdo: tpdo3            # mapped into a slave→master PDO
```

Run `python tools/regenerate_all.py`, then add 3 lines to `syncRpdoToModules` and
`syncModulesToTpdo` to call your new `TempController`. Done. No EDS hand-editing, no
OD.h hand-editing, no platformio.ini changes, no PR to multiple files scattered across
the codebase.

---

**Q: Long term — can we generate the OD fully automatically without CANopenEditor?**

Yes. The migration is staged across three phases:

| Phase | OD source | Tooling needed | Status |
|-------|-----------|----------------|--------|
| Phase 1 — now | YAML → EDS → CANopenEditor → OD.h/OD.c (manual export step) | CANopenEditor | PR-6 |
| Phase 2 — soon | YAML → EDS + OD.h/OD.c directly (generator emits both) | Just `regenerate_all.py` | PR-14 (post-cleanup) |
| Phase 3 — long term | YAML → everything, drop CANopenEditor entirely | Just `regenerate_all.py` | done after PR-14 |

The current `regenerate_all.py` already emits a stub `OD.c` with a TODO marker. To
finish the OD.c generator we need ~250 lines of additional template code that emit the
CANopenNode v4 OD descriptor table format (per-entry pointers, type codes, attribute
flags, sub-index sub-tables). The format is documented in CANopenNode's source headers.
This is a self-contained PR you can hand to Claude Code as PR-14 once the rest of the
migration is stable.

The CANopenEditor dependency in Phase 1 is intentional — it lets us validate the YAML
output visually and round-trip-edit if needed. Once the generator is proven correct over
several PRs, CANopenEditor becomes optional.

---

**Q: USB-CAN Waveshare integration — Pi talks CANopen directly?**

PR-13 covers this. The Pi runs `python-canopen` against the same EDS file the firmware
uses, so parameter names and indices match exactly. The same `UC2_OD::MOTOR_TARGET_POSITION`
constant from the C++ header exists as `OD.MOTOR_TARGET_POSITION` in the Python module.

Two operating modes:

1. **Master ESP32 in the loop** (current production setup): Pi → USB serial → master
   ESP32 → CAN bus → slaves. The master translates JSON to SDO. This is what PR-1..12
   builds. Use this when ImSwitch is the primary controller.

2. **Pi direct via Waveshare USB-CAN** (PR-13): Pi → USB CAN adapter → CAN bus →
   slaves. The master ESP32 is bypassed entirely (or absent). Use this for diagnostics,
   field service, scripting, and standalone operation without ImSwitch.

Both modes use the same EDS, the same node-IDs, and the same OD entries. A move command
issued from `python-canopen` is indistinguishable on the bus from one issued by the
master ESP32 — slaves don't care which master sent the SDO write.

The Waveshare USB-CAN-A appears as a serial port. On Linux you bring it up as
`socketcan` via the `slcand` daemon; on macOS/Windows you use `python-can`'s `slcan`
backend directly. PR-13 includes example scripts and a CLI tool (`uc2-can move
--axis x --pos 1000 --speed 15000`) that exercises the full OD without any firmware in
the loop.

---

**Q: Will every parameter be exposed via CANopen at the end?**

Yes. The end-state requirement is explicit: **after PR-12, every parameter that the
existing JSON API exposes is also exposable via CANopen, and there are no JSON-only
fields.** That includes:

| Module | Parameters covered |
|--------|-------------------|
| Motor | target/actual position, speed, acceleration, jerk, isabs, enable, soft limits (min/max), command word, status word, microstep config, current limit, StallGuard threshold, encoder feedback |
| Homing | speed, direction, timeout, endstop release distance, endstop polarity, command, completion status |
| Laser | PWM value (per channel), max value, frequency, resolution, despeckle period/amplitude, safety state |
| LED | array mode, brightness, uniform colour, full pixel array (via SDO block transfer), pattern ID, pattern speed, layout dimensions |
| Galvo | target X/Y, actual X/Y, scan parameters (nStepsLine, nStepsPixel, dStepsLine, dStepsPixel, dwell time), command word, status word, scan mode |
| Joystick | axis values, button states, deadzone, scaling |
| I/O | digital inputs (endstops, triggers), digital outputs (camera trigger, lasers shutter), analog inputs, DAC outputs |
| System | firmware version, board ID, uptime, free heap, CAN error counts, NMT state, module enable bitmask |
| OTA | firmware data (DOMAIN), firmware size, CRC32, status, abort flag |
| TMC | microsteps, RMS current, StallGuard, CoolStep, blanking time, off time |

Anything that today flows through `/motor_act`, `/laser_act`, `/led_act`, `/home_act`,
`/tmc_act`, `/galvo_act`, `/dial_act`, `/state_get`, `/config_get`, `/digital*`,
`/analog*` ends up as one or more entries in the registry YAML and therefore in the
generated OD. The JSON API stays as the user-facing interface (so existing ImSwitch code
keeps working) but every JSON command gets translated to SDO writes by `DeviceRouter` —
there is no parallel JSON path that bypasses the OD.

---

## After PR-12: what the firmware looks like

```
main/src/
├── canopen/
│   ├── CANopenModule.h            ← TWAI init + FreeRTOS tasks + OD↔Module sync
│   ├── CANopenModule.cpp
│   ├── DeviceRouter.h             ← Master-side JSON → SDO bridge
│   ├── DeviceRouter.cpp
│   ├── CanOpenOTA.h               ← OTA via SDO block transfer
│   ├── CanOpenOTA.cpp
│   ├── UC2_OD_Indices.h           ← AUTO-GENERATED named constants
│   └── RemoteSlaveCache.h         ← Master's mirror of slave TPDOs
│
├── motor/   ← unchanged
├── laser/   ← unchanged
├── led/     ← unchanged
├── home/    ← unchanged
├── tmc/     ← unchanged
└── scanner/ ← unchanged (galvo)

lib/
├── ESP32_CanOpenNode/             ← Third-party, unmodified, the trusted CANopen stack
└── uc2_od/
    ├── OD.h                       ← AUTO-GENERATED from registry
    └── OD.c                       ← AUTO-GENERATED from registry

tools/
├── uc2_canopen_registry.yaml      ← SINGLE SOURCE OF TRUTH
├── regenerate_all.py              ← Run after every YAML edit
└── README.md

DOCUMENTATION/
├── openUC2_satellite.eds          ← AUTO-GENERATED, load in CANopenEditor for review
└── UC2_CANopen_Parameters.md      ← AUTO-GENERATED reference

PYTHON/uc2_canopen/
├── uc2_indices.py                 ← AUTO-GENERATED constants
├── uc2_client.py                  ← High-level wrapper for python-canopen
├── cli.py                         ← Command-line tool
├── examples/
└── README.md
```

The generator script and the registry YAML are the only places you ever edit when
adding new hardware. Everything else is downstream and rebuilds automatically.

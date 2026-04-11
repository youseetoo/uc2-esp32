# UC2 CANopen Migration — Work Packages v2 (post PR-119 at cf9d896)

## Where we are right now

**PR #119** (`feature/runtime-config`) at commit `cf9d896` contains everything from the
original PR-1 through PR-7, the registry + generator, a first pass at OTA, and a design
note calling out the routing problem that PR-7.5 is going to solve:

| PR | What it added | Status in commit `cf9d896` |
|----|---------------|----------------------------|
| PR-1 | RuntimeConfig + NVS | Done |
| PR-2 | Rename `can_controller` → `can_transport` | Done |
| PR-3 | CANopenNode library in `lib/ESP32_CanOpenNode/` | Done |
| PR-4 | `CANopenModule` slave (TWAI + OD↔Module sync) | Done |
| PR-5 | `DeviceRouter` master (JSON → SDO bridge) | Done |
| PR-6 | Registry YAML + generator + generated artifacts | Done (in `tools/canopen/`) |
| PR-7 | Native UC2 OD (replaces trainer OD, removes encoded-uint16 workaround) | Done (in `lib/uc2_od/`) |
| PR-11 | OTA via CANopen — first pass | Partial — `CanOtaHandler.cpp`, `CanOtaStreaming.cpp`, `tools/ota/*.py` present; needs integration test + cleanup |

### Concretely in the repo at `cf9d896`

Directory layout that has appeared on the branch:

```
tools/canopen/
  ├── regenerate_all.py          ← generator
  ├── uc2_canopen_registry.yaml  ← single source of truth
  └── canopen_targets.py         ← target paths for generated files

.github/workflows/
  └── canopen-registry-check.yml ← CI drift guard (runs regenerate_all.py + git diff --exit-code)

lib/uc2_od/
  ├── OD.c                       ← the native UC2 OD (replaces trainer OD)
  ├── OD.h
  └── library.json

lib/ESP32_CanOpenNode/src/
  ├── OD.c                       ← stub / delegation to lib/uc2_od
  ├── OD_trainer.h               ← old trainer OD, kept as reference only
  ├── OD_OLD_C.TXT               ← old OD.c renamed as archive
  └── OD_OLD_H.TXT               ← old OD.h renamed as archive

main/src/canopen/
  ├── CANopenModule.cpp / .h     ← TWAI + FreeRTOS tasks + OD↔Module sync
  ├── DeviceRouter.cpp / .h      ← master-side JSON → SDO bridge
  └── UC2_OD_Indices.h           ← generated C++ named constants

main/src/can/
  ├── CanOtaHandler.cpp          ← PR-11 first pass: SDO domain callback
  ├── CanOtaStreaming.cpp        ← PR-11 first pass: master-side streaming
  ├── can_transport.cpp          ← OLD, still present, PR-12 will delete
  ├── can_messagetype.h          ← OLD, PR-12 will delete
  ├── can_controller_stubs.cpp   ← OLD shim, PR-12 will delete
  └── iso-tp-twai/               ← OLD, PR-12 will delete

main/config/
  ├── UC2_canopen_master/        ← NEW build environment config
  └── UC2_canopen_slave/         ← NEW build environment config

tools/ota/
  ├── can_ota_simple.py
  ├── can_ota_streaming.py
  ├── can_ota_test.py
  └── test_binary_sync.py

DOCUMENTATION/
  ├── openUC2_satellite.eds      ← OLD, from create_eds.py (≈53 mfr objects)
  ├── openUC2_satellite_new.eds  ← NEW, from regenerate_all.py (≈90 mfr objects)
  ├── create_eds.py              ← OLD standalone generator, SUPERSEDED
  └── UC2_CANopen_Parameters.md  ← generated reference
```

**What works end-to-end today:**

- `UC2_canopen_master`, `UC2_canopen_slave`, and `UC2_3_CAN_HAT_Master_v2` compile
- Master receives `motor_act` JSON, `DeviceRouter` writes target position, speed,
  acceleration, and control word as single SDO writes (no split-uint16)
- Slave's `syncRpdoToModules()` decodes the UC2 OD entries (`0x2000` range) and calls
  `FocusMotor::startStepper()` with the full parameter set
- Slave's `syncModulesToTpdo()` writes actual position back into the OD
- Master reads it via `motor_get` (still polling — TPDO push lands in PR-8)
- `canMotorAxis` runtime parameter lets a slave choose which local axis it owns
- TWAI bus-off recovery works without driver reinstall on ESP32-S3
- The registry-drift CI check catches any YAML edit that isn't accompanied by
  regenerated EDS/OD.h/Indices
- First pass at CANopen OTA exists but is not yet the clean PR-11 described below

### Two EDS files — version drift issue

**You have two EDS files on the branch and they disagree on content.**

| File | Generator | Approx. size | Source of truth? |
|------|-----------|--------------|------------------|
| `DOCUMENTATION/openUC2_satellite.eds` | `DOCUMENTATION/create_eds.py` (old hand-written) | ~2900 lines, 53 manufacturer objects | NO — delete after PR-7.5 |
| `DOCUMENTATION/openUC2_satellite_new.eds` | `tools/canopen/regenerate_all.py` (registry-driven) | ~3738 lines, 90 manufacturer objects | **YES — this is canonical** |

The new file is bigger because it adds motor soft limits (`0x2008`/`0x2009`/`0x200A`
jerk), endstop polarity (`0x2015`), TMC CoolStep semin/semax/blank/toff
(`0x2024`-`0x2027`), laser frequency/resolution/despeckle/safety
(`0x2103`-`0x2106`), LED layout/pattern/pixel-data (`0x2205`/`0x2210`/`0x2220`/`0x2221`),
digital out (`0x2302`), analog channels (`0x2311`), encoder velocity
(`0x2341`/`0x2342`), joystick deadzone (`0x2403`), system uptime/heap/board
(`0x2506`/`0x2507`), galvo extra (`0x2609`-`0x260F`), full PID block
(`0x2700`-`0x2705`), OTA abort (`0x2F04`/`0x2F05`).

The new file *removes* the old temperature sensor `0x2330` (relocated).

**Resolution:** the registry YAML is the source of truth. The new EDS is correct. The
old EDS and the old `create_eds.py` are both dead and should be removed as part of
PR-7.5 cleanup. See the "Cleanup tasks" section under PR-7.5.

**What is still wrong — the architectural issue you called out:**

> **The module controller flags are device-role-agnostic.** A board compiled with
> `-DLASER_CONTROLLER=1` runs `LaserController.cpp` regardless of whether that hardware
> actually lives on this node or on a remote CAN slave. The routing decision (local
> GPIO vs forward over CAN) is buried inside `setLaserVal()` as a chain of `#ifdef`
> guards. On a pure CAN master/gateway board the routing block is never entered, the
> code falls through to the GPIO path, `getLaserPin()` returns `-1`, and the command
> silently no-ops. Worse, `CANopenModule::loop()` calls `LaserController::setLaserVal()`
> directly on the master with the master's empty OD entries — causing spurious
> local-hardware calls on a board that should be routing via SDO instead.

This is a **conceptual issue**, not a single-line bug. The design note at the bottom of
`claude_code_tasks_canopen_v2.md` in the repo spells it out in full. A new work
package — **PR-7.5: Module backend abstraction** — has been added below to fix it before
any further controllers are wired into the OD. PR-9 (laser/LED) and PR-10 (galvo)
**must wait for PR-7.5** because they would otherwise inherit the same broken pattern.

The race-condition / infinite-loop bug where `syncRpdoToModules()` re-armed the laser
pending flag on every cycle has already been fixed in commit `cf9d896`. That stops the
spurious continuous `setLaserVal` calls on boot, but it does not address the deeper
routing problem — the laser can still be commanded twice (once by `laser_act` direct,
once by the CANopen loop when it sees the OD pending flag).

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

| File | Path on branch | Purpose |
|------|---------------|---------|
| `uc2_canopen_registry.yaml` | `tools/canopen/uc2_canopen_registry.yaml` | Single source of truth for all OD entries |
| `regenerate_all.py` | `tools/canopen/regenerate_all.py` | Generates EDS, OD.h, C++ headers, Python constants, docs |
| `canopen_targets.py` | `tools/canopen/canopen_targets.py` | Target paths — tells the generator where each output file goes |
| `openUC2_satellite.eds` | `DOCUMENTATION/openUC2_satellite_new.eds` (currently side-by-side with the old one) | Generated by `regenerate_all.py` from the registry. Canonical EDS. |
| `UC2_CANopen_Parameters.md` | `DOCUMENTATION/UC2_CANopen_Parameters.md` | Generated human-readable reference |
| `UC2_OD_Indices.h` | `main/src/canopen/UC2_OD_Indices.h` | Generated — C++ named constants |
| `uc2_indices.py` | `PYTHON/uc2_canopen/uc2_indices.py` | Generated — Python named constants for Waveshare scripts |
| `OD.c` / `OD.h` | `lib/uc2_od/OD.c`, `lib/uc2_od/OD.h` | Native UC2 OD replacing the trainer OD |
| CI drift guard | `.github/workflows/canopen-registry-check.yml` | Ensures YAML + generated files never drift apart |

> **EDS version drift.** PR #119 currently contains TWO EDS files that disagree:
>
> | File | Source | Mfr objects | Verdict |
> |------|--------|-------------|---------|
> | `openUC2_satellite.eds` | old `DOCUMENTATION/create_eds.py` | ~53 | obsolete, delete |
> | `openUC2_satellite_new.eds` | `tools/canopen/regenerate_all.py` | ~90 | canonical, rename to `openUC2_satellite.eds` after cleanup |
>
> The new file adds soft limits, jerk, additional TMC tuning (CoolStep semin/semax,
> blank time, toff), laser frequency/resolution/safety state, LED layout/pattern/
> pixel-data, encoder velocity, joystick deadzone, system uptime/heap/board ID,
> additional galvo modes, the full PID block (`0x2700`-`0x2705`), and OTA abort flags
> (`0x2F04`-`0x2F05`). The old file uses CiA 401 indices in a few places
> (`0x6000`/`0x6200`/`0x6401`) that never belonged there.
>
> **Cleanup action (part of PR-7.5):** delete `DOCUMENTATION/create_eds.py` and
> `DOCUMENTATION/openUC2_satellite.eds`, rename `openUC2_satellite_new.eds` to
> `openUC2_satellite.eds`, and update any references (including this document and
> the CI check's path glob).

---

## PR-6: Generator infrastructure + drop-in registry  ✅ DONE

**Branch:** merged into `feature/runtime-config` — see commits `bc55b1b`, `7252962`
**Status:** Landed in PR #119 at commit `7252962` ("Regenerate CANopen registry and add CI")

### What landed

- `tools/canopen/uc2_canopen_registry.yaml` — registry YAML in a `canopen/` subdirectory
  (the sub-dir is fine; update any docs that reference `tools/uc2_canopen_registry.yaml`)
- `tools/canopen/regenerate_all.py` — generator
- `tools/canopen/canopen_targets.py` — target path resolver
- `.github/workflows/canopen-registry-check.yml` — CI drift guard
- Generated: `DOCUMENTATION/openUC2_satellite_new.eds` (**note:** `_new` suffix, PR-7.5
  renames this to remove the suffix)
- Generated: `DOCUMENTATION/UC2_CANopen_Parameters.md`
- Generated: `main/src/canopen/UC2_OD_Indices.h`
- Generated: `PYTHON/uc2_canopen/uc2_indices.py`
- `tools/README.md` documenting the pipeline

### What still needs cleanup from PR-6 (moved to PR-7.5)

- `DOCUMENTATION/create_eds.py` — still present, obsolete, needs deletion
- `DOCUMENTATION/openUC2_satellite.eds` — old 53-object version, needs deletion
- `PYTHON/canopen/create_eds.py` — duplicate obsolete generator, also needs deletion
- `openUC2_satellite_new.eds` → rename to `openUC2_satellite.eds` after deleting the old

### Original prompt (kept for reference only — do not rerun)

<details>
<summary>Click to expand</summary>

```
Goal: Add the parameter registry + generator scripts as the foundation for all later
CANopen work. This PR does NOT change firmware behaviour — it only adds tooling and
generated files. The motor still moves via the split-uint16 workaround after this PR.

Steps:

1. DELETE the obsolete EDS generator and its output:
   - DOCUMENTATION/create_eds.py        ← obsolete standalone generator
   - DOCUMENTATION/openUC2_satellite.eds ← obsolete EDS (53 mfr objects, hand-coded)
   These are replaced by the registry-driven pipeline below. The registry generator
   produces a strict superset of the old EDS (90 mfr objects vs 53) and uses different
   index allocations, so leaving the old file in place would cause confusion.

2. Copy these files into the repo:
   - tools/uc2_canopen_registry.yaml      (provided — single source of truth)
   - tools/regenerate_all.py              (provided — generator)

3. Run the generator once:
   cd tools && python regenerate_all.py

   This creates:
   - DOCUMENTATION/openUC2_satellite.eds  (3738 lines, 90 mfr objects)
   - DOCUMENTATION/UC2_CANopen_Parameters.md
   - main/src/canopen/UC2_OD_Indices.h
   - PYTHON/uc2_canopen/uc2_indices.py
   - lib/uc2_od/OD.h          (skeleton only — full OD.c table comes in PR-7)
   - lib/uc2_od/OD.c          (stub only — replaced in PR-7)

4. Verify the generated UC2_OD_Indices.h compiles by including it in CANopenModule.cpp:
   #include "UC2_OD_Indices.h"
   But do NOT use any of the constants yet — that's PR-7. Just verify the header compiles.

5. Add a CI check to .github/workflows/ (or extend existing workflow) that runs:
   python tools/regenerate_all.py
   git diff --exit-code
   This catches drift between the registry and the generated files.

6. Add tools/README.md explaining:
   - The registry is the single source of truth
   - Workflow for adding a new parameter:
     a) Edit uc2_canopen_registry.yaml
     b) Run python tools/regenerate_all.py
     c) Commit the YAML and ALL generated files together
   - How to add a new module type (motor, laser, etc.)
   - How CANopenEditor fits in (visual review of the generated EDS, optional re-export)

7. Add a Makefile target or pio script:
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
- The old DOCUMENTATION/create_eds.py is deleted from the tree

Notes:
- The registry contains 13 modules with ~90 OD entries covering motor, homing, TMC,
  laser, LED, digital/analog I/O, encoder, joystick, system, galvo, PID, OTA.
- This is a foundation PR — the payoff comes in PR-7 when the slave actually uses these
  entries.
```

</details>

---

## PR-7: Replace trainer OD with native UC2 OD  ✅ DONE

> **Status:** Merged into `feature/runtime-config` (PR #119). The encoded-uint16
> workaround is gone. Motor commands are now native int32 SDO writes against UC2 OD
> indices (`UC2_OD::MOTOR_TARGET_POSITION` etc.). Acceleration is wired through. The
> `canMotorAxis` runtime parameter selects which local axis a slave owns.
>
> The original task prompt is preserved below for traceability and so future hardware
> can follow the same pattern. **Skip ahead to PR-7.5 for the next work item.**

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

## PR-7.5: Module backend abstraction (fix the routing tangle)

**Branch:** `refactor/module-backends`
**Depends on:** PR-7 (done)
**Blocks:** PR-9 (illumination), PR-10 (galvo) — these would otherwise be built on the
broken routing pattern
**Estimated scope:** ~600 lines added (backend interfaces + 3 implementations per module),
~200 lines deleted (`#ifdef` chains inside controllers), no behaviour change for the
standalone `UC2_3` build

### Why this PR exists

The conceptual issue documented at the bottom of this file: build flags like
`-DLASER_CONTROLLER=1` are device-role-agnostic. The same compilation unit
(`LaserController.cpp`) is built for the standalone, pure-CAN-master, and CAN-hybrid
environments. The decision "drive a local GPIO" vs "send an SDO to a remote slave" lives
inside `setLaserVal()` as a chain of `#ifdef` guards. On a pure CAN master the routing
block is never entered, the code falls through to the GPIO path, `getLaserPin()` returns
`-1`, and the command silently no-ops.

The fix is to split each controller into two halves: a thin **front-end** (parses JSON,
validates parameters, tracks QIDs) and a swappable **backend** that knows how to actually
make the hardware move. The backend is selected once at boot time based on the
combination of `pinConfig` and `runtimeConfig.canRole`. After that, the controller code
no longer cares whether the hardware is on this board or a CAN slave on the other side
of the bus — it just calls `backend->setChannel(...)` and the backend does the right
thing.

This is also the right shape for adding new transports later (I2C expander, Ethernet,
Pi-direct via Waveshare). The front-end doesn't change. You add a new backend
implementation and select it in the factory.

### Three options considered, Option C (backend interface) chosen

| Option | What it does | Pros | Cons |
|--------|-------------|------|------|
| **A** — `-DCAN_MASTER_ONLY=1` flag | Add another build flag, guard GPIO paths with it | Cheap, ~2 hours of work | Adds yet another flag to the matrix; doesn't help with future transports |
| **B** — Runtime check inside each controller | Each `act()` checks `runtimeConfig.canRole` and forwards via DeviceRouter | No new flag | Couples every controller to DeviceRouter and runtimeConfig; the routing logic is duplicated in every controller; hard to test |
| **C** — Backend abstraction (this PR) | `LaserBackend` interface with `LocalGPIOLaserBackend`, `CANopenLaserBackend`, `HybridLaserBackend`. `LaserController::init()` constructs the right backend based on pinConfig + canRole. `act()` always calls `backend->setChannel()`. | Clean separation; easy to test in isolation; drops in new transports without touching front-end code; eliminates 200+ lines of `#ifdef` from controllers | Most refactoring up front (~600 lines net); requires changing every controller class signature |

Option C is the right answer long-term. The other two are stopgaps that will need to be
ripped out later anyway when the next transport gets added.

### Claude Code prompt

```
Goal: Eliminate the conflation between "what hardware exists on this node" and "how
commands reach that hardware" by introducing a backend abstraction for each module
controller. After this PR, the routing decision is made ONCE at boot in a factory
function, not on every JSON command via #ifdef chains.

The motor controller is the template — laser, LED, galvo, home, dial follow the same
shape. Do motor first end-to-end, then mechanically replicate.

PHASE 1 — Motor backend interface

1. Create main/src/motor/IMotorBackend.h:

   #pragma once
   #include "MotorTypes.h"

   class IMotorBackend {
   public:
       virtual ~IMotorBackend() = default;

       // Lifecycle
       virtual void setup() = 0;
       virtual void loop()  = 0;

       // Commands
       virtual void startMove(Stepper axis, int32_t target, uint32_t speed,
                              uint32_t accel, bool isAbs) = 0;
       virtual void stopMove(Stepper axis) = 0;
       virtual void setEnable(Stepper axis, bool en) = 0;

       // State queries
       virtual int32_t getPosition(Stepper axis) const = 0;
       virtual bool    isRunning(Stepper axis)  const = 0;
       virtual bool    isHomed(Stepper axis)    const = 0;

       // Identification (for logging / diagnostics)
       virtual const char* name() const = 0;
   };

2. Create main/src/motor/backends/LocalMotorBackend.h + .cpp:
   - Wraps the existing FastAccelStepper code
   - This is what runs today on a standalone UC2_3 board or a CAN slave
   - Move the FastAccelStepper init, the run loop, the position queries from
     FocusMotor.cpp into this class
   - FocusMotor.cpp keeps its existing public API but DELEGATES to the backend
     (see Phase 3)
   - name() returns "LocalMotor"

3. Create main/src/motor/backends/CANopenMotorBackend.h + .cpp:
   - For each command, performs SDO writes via CANopenModule::writeSDO
   - Uses UC2_OD::MOTOR_TARGET_POSITION etc. from the generated header
   - getPosition() reads from the master's RemoteSlaveCache (PR-8 wires this)
     Until PR-8 lands, falls back to a polling SDO read (acceptable interim behaviour)
   - Constructor takes the slave nodeId so it knows where to forward
   - name() returns "CANopenMotor(node=NN)"

4. (Optional, for CAN_HYBRID boards): create HybridMotorBackend
   - Holds an array of per-axis backends (some Local, some CANopen)
   - Each call dispatches to the right per-axis backend based on canMotorAxis or
     pinConfig
   - This is the cleanest way to express "axis 0 is local, axes 1-3 are remote"

PHASE 2 — Backend factory

1. Create main/src/motor/MotorBackendFactory.h + .cpp:

   IMotorBackend* createMotorBackend() {
       // Decision tree based on runtimeConfig + pinConfig
       if (runtimeConfig.canRole == CAN_MASTER && !hasLocalMotorPins()) {
           // Pure master: forward everything
           return new CANopenMotorBackend(/*per-axis nodeId map*/);
       }
       if (runtimeConfig.canRole == CAN_MASTER && hasLocalMotorPins()) {
           // Hybrid master: some local, some remote
           return new HybridMotorBackend(...);
       }
       if (runtimeConfig.canRole == CAN_SLAVE) {
           // Slave: always local, the CANopenModule sync function calls our backend
           return new LocalMotorBackend(/*single axis from canMotorAxis*/);
       }
       // STANDALONE
       return new LocalMotorBackend();
   }

   The factory consults pinConfig.MOTOR_PINS to detect "do I have wiring", and
   runtimeConfig.canRole to decide the role. This is the SINGLE place where the
   compile-time-vs-runtime, local-vs-remote decision lives.

PHASE 3 — Wire the backend into FocusMotor

1. FocusMotor::setup() now does:

   void FocusMotor::setup() {
       backend = createMotorBackend();
       log_i("FocusMotor using backend: %s", backend->name());
       backend->setup();
   }

2. Every public method on FocusMotor delegates:

   void FocusMotor::startStepper(Stepper axis) {
       MotorData* d = getData()[axis];
       backend->startMove(axis, d->targetPosition, d->speed, d->acceleration, d->isabs);
   }

   int32_t FocusMotor::getCurrentPosition(Stepper axis) {
       return backend->getPosition(axis);
   }

   ... etc

3. CANopenModule::syncRpdoToModules() KEEPS calling FocusMotor::startStepper() the
   same way it does today. The difference is that on a master the backend is
   CANopenMotorBackend, so the call quietly turns into another SDO write. On a slave
   the backend is LocalMotorBackend, so the call drives the local GPIO. Same code
   path, different backend.

4. CRITICAL: CANopenModule::loop() must NOT call any module's act() function on a
   master node. The whole point of the refactor is that the master forwards via the
   backend, and the slave's CANopenNode stack has already received the SDO write and
   updated OD_RAM. Add an assert at the top of every syncRpdoToModules / syncTpdoToModules
   call: if (runtimeConfig.canRole == CAN_MASTER) return; (unless it's a hybrid that
   genuinely owns local hardware AND has received its own RPDO).

PHASE 4 — Repeat for laser, LED, galvo, home, dial, encoder

For each controller:
1. Define ILaserBackend / ILedBackend / etc.
2. Move existing code into LocalXxxBackend
3. Add CANopenXxxBackend that does SDO writes
4. Add a factory that constructs the right one based on pinConfig + canRole
5. Update the front-end controller to delegate to its backend

The pattern is mechanical. Don't try to over-engineer per-module — copy the motor
shape exactly, just rename the types.

PHASE 5 — Delete the dead routing #ifdefs

After every controller has a backend, search and destroy:
- LaserController.cpp: remove the entire shouldUseCANForLaser() helper, the CAN_HYBRID
  branch in setLaserVal(), the fallback to native GPIO. The backend is the only path.
- FocusMotor.cpp: remove any #ifdef CAN_RECEIVE_MOTOR / CAN_SEND_COMMANDS logic
- LedController.cpp: same cleanup
- Verify with: grep -rn "shouldUseCANFor\|CAN_HYBRID\|CAN_RECEIVE_LASER" main/src/
  Should return zero hits in controller files (only in legacy can_transport.cpp,
  which PR-12 will delete entirely)

DO NOT in this PR:
- Touch can_transport.cpp (PR-12 deletes it whole)
- Add new OD entries (PR-9/10 do that)
- Change the JSON API
- Touch the standalone non-CAN build paths beyond delegating through the backend

Verification:
- All four environments compile: UC2_2 (standalone, no CAN), UC2_3 (standalone),
  UC2_canopen_master (pure master), UC2_canopen_slave (slave)
- Standalone UC2_3: motor moves identically to before (regression test)
- CAN slave: motor moves identically to before (regression test)
- CAN master with no slave on the bus: /motor_act returns an error explaining "no
  backend can reach this axis" instead of silently no-opping
- CAN master with slave attached: /motor_act forwards via SDO and the slave moves
- grep -rn "LASER_CONTROLLER" main/src/laser/   appears only in #include guards and
  the constructor — never in routing logic
- The CANopenModule::loop() spurious-laser-call bug is gone: on a master, no
  setLaserVal() is ever called locally
- REGRESSION — laser double-command bug: on a standalone UC2_3 board, send
  {"task":"/laser_act","laser":{"LASERid":1,"LASERval":1000}} and instrument
  LaserBackend::setChannel() to count calls. Expected: exactly ONE call. Before
  PR-7.5, on a hybrid build this produced two calls (once via laser_act direct,
  once via the CANopen loop's pending-flag dispatch) which created the "laser turns
  on twice" symptom the user reported.
- REGRESSION — on a pure CAN master with LASER_CONTROLLER compiled in but no local
  laser pins wired, /laser_act must route via the CANopen backend and NOT fall
  through to getLaserPin() == -1 silent no-op.
```

### Cleanup tasks that ride along in this PR

PR-6 and PR-7 left some housekeeping that's easiest to do in the same branch as the
routing refactor, because they all touch the EDS / generated-file path and the
routing refactor needs the generated files in their final locations anyway.

**EDS version-drift cleanup:**

```
The branch currently has two EDS files that disagree:
  DOCUMENTATION/openUC2_satellite.eds       ← old, hand-written, ~53 mfr objects
  DOCUMENTATION/openUC2_satellite_new.eds   ← registry-generated, ~90 mfr objects

1. Delete the obsolete generator and its output:
   - rm DOCUMENTATION/create_eds.py
   - rm DOCUMENTATION/openUC2_satellite.eds
   - rm PYTHON/canopen/create_eds.py        (duplicate hand-written generator)

2. Rename the canonical EDS to drop the _new suffix:
   - git mv DOCUMENTATION/openUC2_satellite_new.eds DOCUMENTATION/openUC2_satellite.eds

3. Update tools/canopen/canopen_targets.py so the generator writes to
   DOCUMENTATION/openUC2_satellite.eds (no _new suffix). Run regenerate_all.py
   once to verify the output path is now correct and git diff --exit-code passes.

4. Update .github/workflows/canopen-registry-check.yml path globs if they
   referenced the _new filename.

5. grep the repo for any remaining references to create_eds.py or
   openUC2_satellite_new.eds and remove them (docs, CI configs, README, etc.).
```

**Dead-code cleanup (preparatory for PR-12):**

```
These files are leftovers from the pre-CANopen transport and the trainer OD. Flag
them with // DEPRECATED — PR-12 will delete comments but do NOT delete them in this
PR (PR-12 is the single commit where everything old goes away at once — keeping the
cleanup as one reviewable atomic PR is worth the small delay):

- lib/ESP32_CanOpenNode/src/OD_trainer.h   (reference-only trainer OD header)
- lib/ESP32_CanOpenNode/src/OD_OLD_C.TXT   (archived old OD source)
- lib/ESP32_CanOpenNode/src/OD_OLD_H.TXT   (archived old OD header)
- main/src/can/can_controller_stubs.cpp    (no-op stubs for legacy calls)
- main/src/can/iso-tp-twai/                (entire directory)
- main/src/can/can_messagetype.h           (custom message-type enum)

Just add header comments. PR-12 does the actual deletions.
```

**OTA file relocation (preparatory for PR-11 rework):**

```
PR-11 (OTA via CANopen) was started prematurely and its files landed in the wrong
directory. Move them to where the clean PR-11 expects them:

- git mv main/src/can/CanOtaHandler.cpp   main/src/canopen/CanOpenOTA.cpp
- git mv main/src/can/CanOtaStreaming.cpp main/src/canopen/CanOpenOTAStreaming.cpp

After the move, the files are still compiled but live next to CANopenModule and
DeviceRouter, which is where PR-11 needs them. PR-11 will then rewrite the internals
to use OD_extension_init() against the registry-generated 0x2F00 DOMAIN entry and
remove any lingering dependencies on the old can_transport.

If you prefer to leave the OTA files untouched in this PR, that's acceptable —
PR-11 can do the move as part of its own rework. Just note it in the PR description
so reviewers aren't surprised.
```

### Why PR-9 and PR-10 wait for this

Wiring laser/LED/galvo into the registry-driven OD before fixing the routing tangle
just creates more places that need to be untangled later. PR-7.5 establishes the
backend pattern with the motor (which already works end-to-end), then PR-9/10 inherit
the pattern for free.

After PR-7.5 lands, adding a new module is:

1. Define `IXxxBackend` interface
2. Implement `LocalXxxBackend` (existing code, moved)
3. Implement `CANopenXxxBackend` (new — just SDO writes against UC2_OD constants)
4. Add the factory entry
5. Add the OD entries to the registry YAML
6. Regenerate
7. Done — works on standalone, slave, master, hybrid, and Pi-direct simultaneously

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

## PR-11: OTA via CANopen SDO block transfer  ⚠️ FIRST PASS EXISTS — NEEDS REWORK

**Branch:** `feature/canopen-ota` (rework branch off `refactor/module-backends` after PR-7.5)
**Depends on:** PR-7.5 (backend abstraction) + PR-9 (SDO domain for LED pixel data) helpful
**Estimated scope:** ~300 lines, rework of existing `CanOtaHandler.cpp` / `CanOtaStreaming.cpp`

### Current state in PR #119

A first pass at OTA landed prematurely and lives in the wrong directory:

- `main/src/can/CanOtaHandler.cpp`     (should be `main/src/canopen/CanOpenOTA.cpp`)
- `main/src/can/CanOtaStreaming.cpp`   (should be `main/src/canopen/CanOpenOTAStreaming.cpp`)
- `tools/ota/can_ota_simple.py`        (master-side simple transfer)
- `tools/ota/can_ota_streaming.py`     (master-side streaming)
- `tools/ota/can_ota_test.py`          (bench test)
- `tools/ota/test_binary_sync.py`      (binary sync verification)

**Review-first approach recommended** — before writing the rework prompt, read the
existing files and identify:

1. Are the SDO writes going through `CO_SDOclientDownload()` or are they bypassing
   CANopenNode and driving TWAI frames directly? (The latter would be a bad sign.)
2. Do they use magic indices (`0x2F00` literals) or `UC2_OD::OTA_FIRMWARE_DATA`?
3. Does the 0x2F00 entry on the slave have an `OD_extension_init()` callback, or is it
   still a plain RAM entry that would overflow ESP32 memory on a large firmware image?
4. Are the files still depending on any symbols from the old `can_transport.cpp`?

If the answers are mostly "bad / magic / no / yes", PR-11 is a rewrite rather than a
refinement. If they're mostly "good / named / yes / no", PR-11 becomes a smaller cleanup
that renames/moves files and tightens integration.

Either way, PR-11 should end up with files in `main/src/canopen/` and callsites using
the generated `UC2_OD::OTA_*` constants.

### Claude Code prompt

```
Goal: Master can flash a slave's firmware over CAN by streaming a binary into the slave's
OTA partition via SDO segmented or block transfer to UC2_OD::OTA_FIRMWARE_DATA (0x2F00).

Step 0 — AUDIT the existing first-pass OTA implementation. Read these files:
  main/src/can/CanOtaHandler.cpp
  main/src/can/CanOtaStreaming.cpp
  tools/ota/can_ota_simple.py
  tools/ota/can_ota_streaming.py

Record findings in a short REVIEW.md at the top of the branch:
  - What works (can flash a test slave successfully?)
  - What's wrong (magic numbers, wrong directory, direct TWAI access, etc.)
  - What needs rewriting vs what needs moving
Then decide: clean rewrite (discard and start over) or progressive fix (keep the
working parts, fix the rest).

Step 1 — move the C++ files into main/src/canopen/ so they sit next to CANopenModule:
  git mv main/src/can/CanOtaHandler.cpp    main/src/canopen/CanOpenOTA.cpp
  git mv main/src/can/CanOtaStreaming.cpp  main/src/canopen/CanOpenOTAStreaming.cpp
  Update main/CMakeLists.txt source list.
  Update the #include guards and forward declarations.

Step 2 — replace all hardcoded OTA OD indices with generated constants:
  #include "UC2_OD_Indices.h"
  Use UC2_OD::OTA_FIRMWARE_DATA, UC2_OD::OTA_FIRMWARE_SIZE, UC2_OD::OTA_FIRMWARE_CRC32,
  UC2_OD::OTA_STATUS, UC2_OD::OTA_BYTES_RECEIVED, UC2_OD::OTA_ERROR_CODE from the
  generated UC2_OD_Indices.h — NO magic numbers anywhere.

Step 3 — the 0x2F00 entry MUST be a DOMAIN type with an OD write extension callback
that streams to esp_ota_write() WITHOUT buffering the full binary in RAM:

  OD_entry_t* entry = OD_find(OD, UC2_OD::OTA_FIRMWARE_DATA);
  static OD_extension_t ext = {
      .object = nullptr,
      .read   = nullptr,                       // write-only
      .write  = CanOpenOTA::onOtaWriteChunk,   // called for each SDO data segment
  };
  OD_extension_init(entry, &ext);

  onOtaWriteChunk() calls esp_ota_write() with the incoming chunk and returns
  CO_SDO_AB_NONE on success or an SDO abort code on failure.

Step 4 — master-side streaming uses CO_SDOclientDownloadInitiate /
CO_SDOclientDownloadBufWrite / CO_SDOclientDownload in a loop, reading the binary
from serial (or SD card) in 512-byte chunks. CANopenNode handles segmented or block
transfer automatically based on server capabilities.

Step 5 — status reporting via TPDO, not polling:
UC2_OD::OTA_STATUS and UC2_OD::OTA_BYTES_RECEIVED are TPDO3-mapped in the registry.
After PR-8 lands, the master automatically sees progress updates without polling.

Step 6 — verify CRC32 against UC2_OD::OTA_FIRMWARE_CRC32 BEFORE calling
esp_ota_set_boot_partition(). If mismatch, call esp_ota_abort() and leave the active
partition untouched.

Step 7 — add Python CLI tool PYTHON/uc2_canopen/uc2_ota_can.py that uses
python-canopen + Waveshare USB-CAN to flash a slave directly without going through
the master ESP32:

  python -m uc2_canopen.uc2_ota_can --node 11 --binary firmware.bin

This is incredibly useful for development — you can flash any slave from your laptop.

Step 8 — bench test:
  - Flash slave with a partition table that has two app partitions
  - From master serial: {"task":"/ota_start","ota":{"nodeId":11,"size":1048576,"crc32":"..."}}
  - Monitor slave log for "OTA started", progress every 64KB, "OTA verified. Rebooting"
  - After reboot, slave should run the new firmware
  - Verify: power-cycle the slave during the transfer — it should come back on the
    OLD firmware, not bricked

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
main (after PR #119 merges, which includes PR-1 through PR-7)
│
├── PR-6:  feature/canopen-registry          ✅ DONE in PR #119
│   │
│   ├── PR-7:  feature/canopen-uc2-od         ✅ DONE in PR #119
│   │   │
│   │   ├── PR-7.5: refactor/module-backends  ← NEXT — fixes routing tangle, gates 9/10
│   │   │   │
│   │   │   ├── PR-8:  feature/canopen-tpdo-push     ← Slaves push state (TPDO)
│   │   │   │
│   │   │   ├── PR-9:  feature/canopen-illumination  ← Laser + LED (needs PR-7.5)
│   │   │   │
│   │   │   ├── PR-10: feature/canopen-galvo         ← Galvo / scanner (needs PR-7.5)
│   │   │   │
│   │   │   ├── PR-11: feature/canopen-ota           ← OTA via SDO block
│   │   │   │
│   │   │   └── PR-13: feature/waveshare-canopen     ← Pi-direct via USB-CAN (parallel)
│   │   │
│   │   └── PR-12: cleanup/remove-old-can-transport  ← After PR-7.5..11 merged
```

**PR-7.5 is now the gating PR.** Its job is to remove the conflation between "what
hardware exists on this node" and "how commands reach that hardware" before any further
controllers are wired into the OD. PR-9 and PR-10 explicitly depend on PR-7.5 because
adding laser/LED/galvo into the registry-driven OD before fixing the routing tangle just
creates more places that need to be untangled later.

After PR-7.5 lands, PRs 8 / 9 / 10 / 11 / 13 can be developed in parallel by different
people (or by Claude Code in separate sessions). PR-12 is the final cleanup and must wait
until everything else is verified working.

---

## What about the long term — generating a fully standalone OD?

At commit `cf9d896` the registry generator emits the EDS, the C++ indices header, the
Python module, and the markdown reference directly from the YAML. The CANopenNode
`OD.c` / `OD.h` files in `lib/uc2_od/` were generated via CANopenEditor from the
registry's EDS during PR-7 (one-time manual export step). This works but means the
round-trip is: edit YAML → regenerate → open EDS in CANopenEditor → export OD.h/OD.c
→ commit.

The migration path away from the CANopenEditor round-trip:

1. **Phase 1 (now, after PR-7):** Registry → EDS → CANopenEditor → OD.h / OD.c
   (semi-automatic, works today, what's on the branch)
2. **Phase 2 (PR-14, post-PR-12):** Extend `tools/canopen/regenerate_all.py` to emit a
   fully working `OD.c` directly from the YAML, bypassing CANopenEditor entirely. The
   CANopenNode v4 OD descriptor format is documented in the library source headers;
   writing a generator for it is ~250 lines of template Python. Once this lands,
   CANopenEditor becomes optional (useful only for visual review / eyeballing PDO
   mappings in a GUI).
3. **Phase 3 (long term):** Drop the CANopenEditor dependency entirely. The registry
   is the canonical source; the generator is the canonical tool. New hardware → edit
   YAML → run generator → commit.

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
| `tools/canopen/uc2_canopen_registry.yaml` | Single source of truth — every OD entry, with name, type, default, PDO mapping, sub-indices, C++ field hint |
| `tools/canopen/regenerate_all.py` | Reads the YAML, generates the EDS, OD.h/OD.c, C++ constants header, Python constants, and markdown reference doc |
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
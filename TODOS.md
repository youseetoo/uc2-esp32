# UC2 Firmware & Python Stack — Work Packages

Organized from the collected issues. Each WP has a priority (P0=blocking, P1=high, P2=medium, P3=nice-to-have), estimated scope, dependencies, and Claude Code instructions.

---


---

## WP3: Galvo QID Tracking (P1)

**Goal:** Wire galvo status word into QID registry so galvo commands report completion like motors do.

**Scope:** uc2-ESP firmware  
**Depends on:** WP2 (OD alignment)

### Issues
- Galvo commands are fire-and-forget — no QID completion tracking
- Slave pushes `OD_RAM.x2603_galvo_status_word` via TPDO but master ignores it
- SDO write timeout for galvo is logged as warning but not tracked

### Sub-tasks
1. In `CANopenModule.cpp` (`syncRpdoToModules_master`), read the galvo status word from the RPDO
2. Detect running→stopped transition on the galvo status word
3. Call `QidRegistry::reportActionDone(qid)` on that transition
4. In `DeviceRouter.cpp`, register a QID when dispatching galvo commands via SDO

### Claude Code Instructions
```
Working directory: /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP

Tasks:
1. Read main/src/canopen/CANopenModule.cpp — find syncRpdoToModules_master().
   Locate where motor status is read from RPDO and QID completion is reported.

2. Add equivalent logic for galvo: read OD_RAM.x2603_galvo_status_word,
   track previous value, detect bit transition from running to stopped,
   call QidRegistry::reportActionDone(qid).

3. Read main/src/canopen/DeviceRouter.cpp — find where galvo SDO commands
   are dispatched. Add QidRegistry::registerAction(qid, ...) before the
   SDO write sequence.

4. In the galvo SDO timeout warning, upgrade from log_w to also calling
   QidRegistry::reportActionError(qid) so the client gets notified of
   the failure.

5. Build: pio run -e UC2_canopen_master
```

---

## WP4: Homing Improvements (P1)

**Goal:** Blocking homing with completion feedback across CAN, plus audible beep on master.

**Scope:** uc2-ESP firmware + UC2-REST Python  
**Depends on:** WP3 (QID pattern)

### Issues
- Homing completion not reliably reported back through CAN to Python
- No audible feedback on master when homing completes
- UC2-REST `home.py` blocking mode expects `nResponses=2` but CAN path may differ

### Sub-tasks
1. **Firmware (slave):** Ensure homing completion sets a status bit in the motor status word TPDO
2. **Firmware (master):** In `syncRpdoToModules_master`, detect homing-done via motor status word and report QID completion
3. **Firmware (master):** Add short beep via buzzer/speaker when homing completes (if `PIN_BUZZER` defined)
4. **Python (UC2-REST):** Update `home.py` to support CAN-based blocking homing — listen for QID completion instead of relying on serial response count

### Claude Code Instructions
```
Working directories:
  - /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP
  - /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/UC2-REST

Firmware tasks:
1. Read main/src/home/HomeMotor.cpp — find how homing completion is
   signaled. Check if the motor status word (OD 0x2004) reflects homing
   state.

2. If not, add a homing-done bit to the status word that gets pushed
   via TPDO when homing completes.

3. In CANopenModule.cpp syncRpdoToModules_master, detect homing-done
   transition and call QidRegistry::reportActionDone.

4. Add buzzer feedback: if pinConfig.PIN_BUZZER is defined, play a
   short 200ms tone at 2kHz when homing QID completes. Use ledcWrite
   or tone() on ESP32.

Python tasks:
5. Read uc2rest/home.py — the blocking mode uses nResponses parameter.
   For CAN-connected motors, blocking should instead poll /qid_state
   until the QID reports DONE or TIMEOUT.

6. Add a home_blocking() method or modify existing home_x/y/z/a to
   detect CAN mode and use QID-based waiting instead of response counting.
```

---

## WP5: Emergency Button (P1)

**Goal:** Send JSON messages on emergency button press and release.

**Scope:** uc2-ESP firmware  
**Depends on:** None

### Sub-tasks
1. In `DigitalInController.cpp`, add edge detection (rising + falling) for `pinEmergencyExit`
2. On falling edge (button pressed): send JSON `{"emergency": {"pressed": true}}` via MessageController
3. On rising edge (button released): send JSON `{"emergency": {"pressed": false}}`
4. Optionally stop all motors immediately on press (call `FocusMotor::stopAll()`)

### Claude Code Instructions
```
Working directory: /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP

Tasks:
1. Read main/src/digitalin/DigitalInController.cpp — find the emergency
   pin handling. Currently it just logs "Emergency Exit: %i".

2. Add edge detection: store previous pin state, detect transitions.
   On press (falling edge for active-low, or configurable polarity):
   - Call FocusMotor::stopAll() to halt all motors
   - Send JSON {"emergency": {"pressed": true}} via MessageController::sendMessage()

3. On release (rising edge):
   - Send JSON {"emergency": {"pressed": false}} via MessageController

4. Read main/src/message/MessageController.cpp to understand how to
   properly format and send the JSON message through the serial/WiFi
   output path.

5. In UC2-REST/uc2rest/, add an emergency callback registration in
   a suitable module (e.g., state.py or a new emergency.py) that
   listens for the "emergency" pattern.

6. Build: pio run -e UC2_canopen_master
```

---

## WP6: I2C Cleanup + Temperature Sensors (P2)

**Goal:** Remove I2C master/slave protocol, keep TCA9535 GPIO expander and generic I2C register access. Add I2C temperature sensor support.

**Scope:** uc2-ESP firmware  
**Depends on:** None

### Sub-tasks
1. Remove I2C master/slave state-exchange code (`i2c_master.cpp` request/response protocol, `i2c_slave_dial.cpp`)
2. Keep: TCA9535 driver, I2C bus scan, generic register read/write
3. Add generic I2C endpoint: `/i2c_act` with `{"i2c": {"address": 0x48, "register": 0x00, "read_bytes": 2}}` and write equivalent
4. Add I2C temperature sensor driver (e.g., TMP102/LM75 at default 0x48) with `/temp_get` endpoint
5. Keep I2C scan (`/i2c_scan`) for device discovery

### Claude Code Instructions
```
Working directory: /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP

Tasks:
1. Read main/src/i2c/i2c_master.h and i2c_master.cpp. Identify which
   functions are master/slave protocol (requestMotorState, requestHomeState,
   requestLaserData, etc.) vs. generic I2C (scan, read register, write register).

2. Remove the master/slave request/response protocol functions. Keep:
   - I2C bus initialization
   - I2C scan functionality
   - TCA9535 driver (tca9535.h)
   - Generic Wire.beginTransmission/requestFrom patterns

3. Add a generic I2C register read/write interface:
   - readI2CRegister(address, register, numBytes) -> byte array
   - writeI2CRegister(address, register, data, numBytes)
   Expose via /i2c_act endpoint in Endpoints.

4. Add I2C temperature sensor support (LM75/TMP102 compatible):
   - Read 2-byte temperature register at address 0x48-0x4F
   - Convert to Celsius (12-bit, 0.0625 deg/LSB)
   - Expose via /temp_get endpoint
   - Add TEMPERATURE_I2C_CONTROLLER build flag

5. Remove i2c_slave_dial.cpp/h — dial communication is now via CAN.

6. Build and verify: pio run -e UC2_canopen_master
```

---

## WP7: CAN Scaling & Dynamic ID Assignment (P2)

**Goal:** Support >4 motors over CAN, auto-assign node IDs by MAC/UID.

**Scope:** uc2-ESP firmware  
**Depends on:** WP2 (OD), WP8 (unified interface)

### Issues
- Motor array hardcoded to `MOTOR_AXIS_COUNT=4` in most configs
- CAN node IDs assigned manually or via serial after boot
- Goal: identical firmware on all slave nodes, differentiated only by MAC/UID

### Sub-tasks
1. **Extend motor array:** Change default `MOTOR_AXIS_COUNT` to 8 in OD and DeviceRouter; make the CAN motor mapping dynamic (array of `{canNodeId, axisIndex}`)
2. **UID-based ID assignment:** Add OD entry for MAC/UID (e.g., 0x2506); on boot, slave reports MAC via heartbeat or LSS; master maps known MACs to node IDs via a config table
3. **Config table:** Store MAC→nodeID mapping in NVS on master; provide `/can_config` endpoint to set/get mappings
4. **Generic node discovery:** Master scans, reads UID from each node, assigns node ID via LSS or custom SDO

### Claude Code Instructions
```
Working directory: /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP

Tasks:
1. In main/config/*/PinConfig.h, find all MOTOR_AXIS_COUNT definitions.
   Change the CAN master configs to MOTOR_AXIS_COUNT=8.

2. In lib/uc2_od/OD.h, extend motor-related OD entries (0x2000-0x200B)
   from 4 subindices to 8 subindices. Update uc2_canopen_registry.yaml
   accordingly and regenerate.

3. In DeviceRouter.cpp, change the motor CAN node mapping from hardcoded
   {10,11,12,13} to a configurable array of up to 8 entries. Store in
   NVS with defaults.

4. Add OD entry 0x2506 (DEVICE_UID) as a read-only STRING[17] containing
   the ESP32 MAC address (use ESP.getEfuseMac()).

5. On master, implement auto-discovery:
   - Scan CAN bus (NMT heartbeat detection)
   - For each discovered node, SDO-read 0x2506 (UID)
   - Look up UID in NVS config table
   - If found, SDO-write to assign logical axis mapping
   - Provide /can_config endpoint to manage the UID→axis table

6. Update tools/canopen/uc2_canopen_registry.yaml and regenerate.
```

---

## WP8: Unified Laser/Motor/Device Interface (P2)

**Goal:** Make laser interface array-based like motors. Create unified enumeration pattern for all device types, both local and remote.

**Scope:** uc2-ESP firmware + UC2-REST Python  
**Depends on:** WP7 (scaling)

### Issues
- Laser uses individual scalars (`LASER_val_0`, `LASER_val_1`) while motors use `MotorData[]` array
- JSON interface differs: motors use `{"steppers": [...]}`, lasers use `{"laser": {...}}`
- No unified local-vs-remote addressing pattern

### Sub-tasks
1. **Firmware laser refactor:** Change LaserController to use `LaserData[MAX_LASERS]` struct array like MotorData
2. **JSON unification:** Laser JSON → `{"lasers": [{"laserid": 0, "value": 1000, ...}]}` (array format like steppers)
3. **Unified device enum:** Create `DeviceType` enum (MOTOR, LASER, LED, GALVO) with unified `getDevice(type, index)` accessor
4. **Remote transparency:** DeviceRouter handles local/CAN routing transparently — same JSON works for local and remote devices
5. **Python update:** UC2-REST `laser.py` updated to match new array-based JSON format

### Claude Code Instructions
```
Working directories:
  - /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP
  - /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/UC2-REST

Firmware tasks:
1. Read main/src/laser/LaserController.h. Create a LaserData struct
   similar to MotorData (fields: id, value, maxValue, despeckleAmplitude,
   despecklePeriod, isDone). Replace individual arrays with LaserData[MAX_LASERS].

2. Update the /laser_act endpoint parser to accept array format:
   {"lasers": [{"laserid": 0, "value": 1000}]} while keeping backward
   compatibility with the old {"laser": {"LASERid": 0, "LASERval": 1000}}.

3. In DeviceRouter, ensure laser commands route to CAN slaves the same
   way motor commands do — by checking if the laser index maps to a
   remote node.

Python tasks:
4. In uc2rest/laser.py, update the JSON payload construction to use the
   new array format. Keep backward compatibility by trying new format
   first, falling back to old.

5. Ensure callback parsing handles both old and new response formats.
```

---

## WP9: M5Stack Dial as CANopen Module (P2)

**Goal:** Integrate the M5Stack Dial as a proper CANopen participant (remote controller + display).

**Scope:** uc2-ESP firmware  
**Depends on:** WP7 (dynamic IDs)

### Issues
- Dial currently acts as CAN master (node 1) but should coexist with the main master
- Needs to send motor commands to slaves and display updated positions
- Should function as a "remote controller" node, not a second master

### Sub-tasks
1. Change Dial from master (node 1) to controller node (e.g., node 2) — it sends SDO commands to slaves but doesn't own the NMT master role
2. Dial reads motor position TPDOs for display updates
3. Dial sends motor move commands directly to slave nodes via SDO
4. Add laser/LED control to Dial UI (mode switching already exists)
5. Consider: Dial could also relay commands through the master if direct slave access causes conflicts

### Claude Code Instructions
```
Working directory: /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP

Tasks:
1. Read main/src/dial/DialController.h and .cpp — understand the current
   CAN command sending (sendMotorCommand, sendLaserCommand).

2. Read main/config/m5stack_dial/PinConfig.h — check NODE_ROLE and CAN_ID
   settings.

3. Change the Dial's node role from master (NODE_ROLE=1) to a non-master
   CANopen participant. It should NOT run NMT master duties but CAN send
   SDO commands.

4. Ensure the Dial receives TPDO motor status updates for display.
   In DialController, update the display with actual positions read from
   TPDOs rather than assumed positions.

5. Add a CANopen heartbeat producer on the Dial so the master knows
   it's alive.

6. Build: pio run -e m5stack_dial
```

---

## WP10: System & Infrastructure (P1)

**Goal:** Fix reboot, firmware version reporting, CI, and Python API alignment.

**Scope:** All three repos  
**Depends on:** WP1 (Python CANopen), WP2 (OD)

### Issues
- `REBOOT_COMMAND` (OD 0x2507) not working
- Firmware version not queryable via `state_get` or CAN bus scan
- GitHub Actions build broken
- `canota.py` and `can.py` in UC2-REST need updating for CANopen

### Sub-tasks

#### 10a: Fix REBOOT_COMMAND
1. Check firmware: OD 0x2507 write handler — does it actually trigger `ESP.restart()`?
2. Check timing: the 200ms delay before restart may be too short for SDO response
3. Check Python: `uc2can reboot` command sends correct SDO write

#### 10b: Firmware Version in state_get + CAN scan
1. Add `GIT_COMMIT_HASH` and `FIRMWARE_VERSION` (semver from build flag) to `state_get` response
2. Populate OD 0x2500 (`firmware_version_string`) on slave boot
3. On master `/can_scan`, SDO-read 0x2500 from each discovered node and include in scan results

#### 10c: Fix GitHub Actions
1. Check `.github/workflows/` — likely PlatformIO build matrix needs updating for new environments
2. Ensure at least `UC2_canopen_master` and `UC2_canopen_slave_motor` build in CI

#### 10d: Align UC2-REST Python API
1. Update `canota.py` to use the new CANopen OTA protocol (integrate `canopen_ota_serial.py` from uc2-ESP/tools/ota/)
2. Update `can.py` to work with CANopen (use SDO for scan results, read firmware version)
3. Both should work via serial (master) or direct CANopen (Waveshare/socketcan)

### Claude Code Instructions
```
Working directories:
  - /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP
  - /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/UC2-REST

10a — Reboot:
1. Read main/src/canopen/CANopenModule.cpp — find where x2507_reboot_command
   is checked. Verify it calls ESP.restart().
2. Add a 500ms delay before restart to ensure the SDO response is sent.
3. Test: uc2can reboot --node 11

10b — Version:
1. In main/src/state/State.cpp, add to state_get response:
   - "firmware_version": from FIRMWARE_VERSION build macro
   - "git_commit": from GIT_COMMIT_HASH macro
2. In State::setup(), write the version string to OD_RAM.x2500_firmware_version_string.
3. In DeviceRouter or the CAN scan handler, SDO-read 0x2500 from each
   discovered node and add to scan JSON response.
4. In platformio.ini, add build flags:
   -DFIRMWARE_VERSION='"0.9.0"'
   -DGIT_COMMIT_HASH='"${git_rev}"'  (use pio custom script for git hash)

10c — CI:
1. Read .github/workflows/ — fix build targets and dependency installation.

10d — Python API:
1. Read uc2rest/canota.py and uc2-ESP/tools/ota/canopen_ota_serial.py.
   Port the streaming OTA protocol from canopen_ota_serial.py into canota.py,
   supporting both serial-to-master and direct Waveshare paths.
2. Read uc2rest/can.py — update scan() and get_available_devices() to
   parse the new CANopen scan response format (including firmware version).
```

---

## Dependency Graph & Suggested Order

```
WP1 (Python CANopen) ─────┐
WP2 (OD Generation) ──────┼──> WP3 (Galvo QID) ──> WP4 (Homing)
WP5 (Emergency Button)    │
WP6 (I2C Cleanup)         │
                           ├──> WP7 (CAN Scaling) ──> WP8 (Unified Interface)
                           │                      └──> WP9 (Dial Controller)
                           └──> WP10 (System/Infra)
```

**Recommended execution order:**
1. **WP1 + WP2** (parallel) — Foundation: get Python working + fix OD pipeline
2. **WP5 + WP6** (parallel, independent) — Quick wins
3. **WP10** — System fixes (reboot, version, CI, API alignment)
4. **WP3** — Galvo QID (uses WP2 OD)
5. **WP4** — Homing improvements (uses WP3 QID pattern)
6. **WP7** — CAN scaling (uses WP2 OD)
7. **WP8** — Interface unification (uses WP7)
8. **WP9** — Dial controller (uses WP7)

---

## Quick Reference: Key Files Per Repo

### uc2-ESP (firmware)
| Area | Files |
|------|-------|
| OD (source of truth) | `tools/canopen/uc2_canopen_registry.yaml`, `tools/canopen/regenerate_all.py` |
| OD (generated) | `lib/uc2_od/OD.h`, `lib/uc2_od/OD.c` |
| CANopen module | `main/src/canopen/CANopenModule.cpp`, `DeviceRouter.cpp` |
| Motors | `main/src/motor/FocusMotor.h/.cpp` |
| Laser | `main/src/laser/LaserController.h/.cpp` |
| Homing | `main/src/home/HomeMotor.h/.cpp` |
| QID | `main/src/qid/QidRegistry.h/.cpp` |
| Galvo | `main/src/scanner/GalvoController.cpp` |
| Emergency | `main/src/digitalin/DigitalInController.cpp` |
| I2C | `main/src/i2c/i2c_master.cpp`, `tca9535.h` |
| Dial | `main/src/dial/DialController.h/.cpp`, `main/config/m5stack_dial/PinConfig.h` |
| State | `main/src/state/State.cpp` |
| OTA tool | `tools/ota/canopen_ota_serial.py` |
| Build | `platformio.ini` |

### UC2-REST-CANOPEN (Python CANopen)
| Area | Files |
|------|-------|
| Demo | `src/motor_demo.py` |
| Client API | `src/uc2canopen/client.py` |
| SDO | `src/uc2canopen/sdo.py` |
| OD indices | `src/uc2canopen/od.py` |
| Waveshare | `src/uc2canopen/waveshare_bus.py` |
| CLI | `src/uc2canopen/cli.py` |

### UC2-REST (Python serial/WiFi API)
| Area | Files |
|------|-------|
| Serial transport | `uc2rest/mserial.py`, `uc2rest/transport.py` |
| Motors | `uc2rest/motor.py` |
| Homing | `uc2rest/home.py` |
| Laser | `uc2rest/laser.py` |
| LED | `uc2rest/ledmatrix.py` |
| CAN | `uc2rest/can.py` |
| OTA | `uc2rest/canota.py` |
| State | `uc2rest/state.py` |
| Temperature | `uc2rest/temperature.py` |
| Messages | `uc2rest/message.py` |

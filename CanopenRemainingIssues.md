# CANopen integration — remaining issues and Claude Code work package

Based on a full review of the codebase at the latest upload (April 2026),
here is what's done, what's broken, and what's missing.

---

## Status summary

| Area | Status | Detail |
|------|--------|--------|
| Motor command dispatch (LOCAL/REMOTE) | ✅ Working | DeviceRouter routes per-stepper, SDO writes work |
| Motor config sub-parsers (isen, setpos, hardlimits, joystickdir) | ✅ Wired | PR-7.7 done — DeviceRouter calls MotorJsonParser::parseXxx |
| Stage scan | ✅ Wired | DeviceRouter calls parseStageScan before drive routing |
| Motor arrival notification (position in response) | ❌ Broken | Master emits `{"qid":5,"state":"done"}` but NOT `{"stepperid":N,"position":P,"isDone":1}` for remote motors |
| PS4 controller motor feedback | ❌ Broken | Same root cause — no position in the arrival message |
| Home act LOCAL/REMOTE | ✅ Working | SDO writes for homing params + command |
| Laser act LOCAL/REMOTE | ✅ Working | SDO write to LASER_PWM_VALUE |
| LED act LOCAL/REMOTE | ✅ Working | SDO writes to mode/brightness/colour |
| LED setMode/setPattern/setPixels duplication | ⚠️ Messy | Three CANopen-specific functions duplicate `execLedCommand` logic |
| Galvo act LOCAL/REMOTE | ✅ Working | SDO writes for scan params + command |
| TMC act REMOTE | ❌ Missing | handleTmcAct is LOCAL-only, no SDO path |
| TMC get REMOTE | ❌ Missing | Always reads local controller |
| State act/get via DeviceRouter | ❌ Missing | Not routed through DeviceRouter at all |
| Reboot command via CAN | ❌ Missing | OD entry exists (0x2507) but no dispatch |
| Motor enable via CAN | ❌ Missing | parseEnableMotor runs locally only, no SDO for remote enable |
| Motor setPosition via CAN | ❌ Missing | parseSetPosition runs locally only |
| Laser get REMOTE | ❌ Missing | Always reads local controller |
| LED get REMOTE | ❌ Missing | Always reads local controller |
| Dead code in SerialProcess | ✅ Cleaned | PR-7.7 removed the duplicate motor/laser/led/home/galvo/tmc branches |
| MotorJsonParser::act() | ⚠️ Deprecated but still exists | No callers on CAN builds, should be deleted |

---

## Issue 1 (CRITICAL): Motor arrival notification missing position

### Symptom
When a remote motor stops, the master emits:
```
{"qid":5,"state":"done"}
```
But does NOT emit:
```
{"steppers":[{"stepperid":1,"position":3000,"isDone":1}],"qid":5}
```

The PS4 controller path also sees only `{"stepperid":1,"isDone":0}` with
no position feedback because the same notification path is broken.

### Root cause
`syncRpdoToModules_master` (CANopenModule.cpp:1048) detects the
running→stopped transition and calls `QidRegistry::reportActionDone(qid)`,
which emits the `{"qid":N,"state":"done"}` message. But it does NOT call
`FocusMotor::sendMotorPos()` or emit its own JSON with position/isDone.

The local motor path (`FocusMotor::stopStepper` → `sendMotorPos`) emits
the full JSON with position. The remote path skips this entirely.

### Fix
In `syncRpdoToModules_master`, after detecting `wasRunning && !nowRunning`,
emit a motor position JSON that matches what the local path sends:

```cpp
if (wasRunning && !nowRunning) {
    ESP_LOGI(TAG_CO, "Slave slot %d (motor %d) DONE pos=%ld",
             slot, logicalAx, (long)newPos);

    // Emit the same JSON the local path sends via FocusMotor::sendMotorPos
    cJSON* root = cJSON_CreateObject();
    cJSON* stprs = cJSON_AddArrayToObject(root, "steppers");
    cJSON* item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "stepperid", logicalAx);
    cJSON_AddNumberToObject(item, "position", newPos);
    cJSON_AddNumberToObject(item, "isDone", 1);
    cJSON_AddItemToArray(stprs, item);
    cJSON_AddNumberToObject(root, "qid", md ? md->qid : -1);
    char* jsonStr = cJSON_PrintUnformatted(root);
    if (jsonStr) {
        SerialProcess::safeSendJsonString(jsonStr);
        free(jsonStr);
    }
    cJSON_Delete(root);

    // ALSO report via QidRegistry for the {"qid":N,"state":"done"} path
    if (md && md->qid > 0) {
        QidRegistry::reportActionDone(md->qid);
    }
}
```

This ensures ImSwitch, PS4 controller handling, and any other consumer
that watches for `isDone` + `position` gets the full message.

---

## Issue 2: TMC over CANopen — no REMOTE path

`handleTmcAct` (DeviceRouter.cpp:808) just calls `TMCController::act(doc)`
directly — LOCAL only. Comment says "TMC config is board-specific —
always local" but that's wrong for CAN setups where the TMC2209 is on
the slave board.

### Fix
Add a REMOTE branch that writes TMC OD entries via SDO:

```cpp
cJSON* DeviceRouter::handleTmcAct(cJSON* doc) {
#ifdef TMC_CONTROLLER
    // TMC parameters are per-axis. Extract stepperid to find the route.
    cJSON* tmc = cJSON_GetObjectItem(doc, "tmc");
    if (!tmc) {
        // No tmc sub-object — delegate to local controller for
        // backward-compatible single-value commands
        int result = TMCController::act(doc);
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", result);
        return resp;
    }

    int stepperid = 0;
    cJSON* sidItem = cJSON_GetObjectItem(tmc, "stepperid");
    if (sidItem) stepperid = sidItem->valueint;

    const auto* route = UC2::RoutingTable::find(
        UC2::RouteEntry::TMC, (uint8_t)stepperid);

    if (!route || route->where == UC2::RouteEntry::OFF) {
        return TMCController::act(doc);  // fallback to local
    }

    if (route->where == UC2::RouteEntry::LOCAL) {
        int result = TMCController::act(doc);
        cJSON* resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "return", result);
        return resp;
    }

    // REMOTE — write TMC OD entries via SDO
#ifdef CAN_CONTROLLER_CANOPEN
    uint8_t nodeId = route->nodeId;
    uint8_t sub    = route->subAxis + 1;
    bool ok = true;

    cJSON* ms = cJSON_GetObjectItem(tmc, "microsteps");
    if (ms) ok = CANopenModule::writeSDO_u16(nodeId, UC2_OD::TMC_MICROSTEPS, sub, ms->valueint) && ok;

    cJSON* cur = cJSON_GetObjectItem(tmc, "current");
    if (cur) ok = CANopenModule::writeSDO_u16(nodeId, UC2_OD::TMC_RMS_CURRENT, sub, cur->valueint) && ok;

    cJSON* sg = cJSON_GetObjectItem(tmc, "stallguard");
    if (sg) ok = CANopenModule::writeSDO_u8(nodeId, UC2_OD::TMC_STALLGUARD, sub, sg->valueint) && ok;

    cJSON* resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "return", ok ? 1 : 0);
    return resp;
#else
    return nullptr;
#endif
#else
    return nullptr;
#endif
}
```

Also wire `syncRpdoToModules_slave` to detect TMC OD changes and apply
them to the local TMCController — same pattern as LED/laser/galvo.

---

## Issue 3: State (reboot) via CANopen

`/state_act` with `{"state_act":"restart"}` should work over CAN. The OD
entry exists: `0x2507 reboot_command`. But there's no route through
DeviceRouter and the slave's `syncRpdoToModules_slave` doesn't watch for
it.

### Fix
1. Add `handleStateAct` to DeviceRouter that routes to `State::act(doc)` for
   LOCAL and writes `REBOOT_COMMAND = 1` via SDO for REMOTE.
2. In `syncRpdoToModules_slave`, detect `OD_RAM.x2507_reboot_command == 1`
   and call `ESP.restart()`.

---

## Issue 4: LED code duplication — setMode/setPattern/setPixels vs execLedCommand

The user correctly identified that `setMode`, `setPattern`, and `setPixels`
in LedController overlap with `execLedCommand`. Here's the mapping:

| CANopen function | What it does | execLedCommand equivalent |
|---|---|---|
| `setMode(mode, brightness, colour)` | Switch on mode enum, calls fillAll/fillHalves/turnOff | `execLedCommand({mode, r, g, b, brightness, ...})` |
| `setPattern(patternId, speed)` | Sets pattern animation state | Not in execLedCommand — pattern is separate |
| `setPixels(data, count)` | Bulk RGB buffer → pixel array | Not in execLedCommand — bulk is separate |

**Recommendation:** `setMode` should call `execLedCommand` internally
instead of duplicating the switch. `setPattern` and `setPixels` are
genuinely different operations (animation state and bulk buffer) that
don't map to the `LedCommand` struct, so they should stay as separate
functions but be documented.

```cpp
void setMode(uint8_t mode, uint8_t brightness, uint32_t colour) {
    uint8_t r = (colour >> 16) & 0xFF;
    uint8_t g = (colour >> 8)  & 0xFF;
    uint8_t b = (colour)       & 0xFF;
    activePatternId = 0;  // disable pattern animation

    LedCommand cmd;
    cmd.r = r; cmd.g = g; cmd.b = b;
    cmd.brightness = brightness;
    // Map mode enum → LedCommand mode
    switch (mode) {
        case 0: cmd.mode = LedMode::OFF; break;
        case 1: cmd.mode = LedMode::FILL; break;
        case 2: cmd.mode = LedMode::LEFT; break;
        case 3: cmd.mode = LedMode::RIGHT; break;
        case 4: cmd.mode = LedMode::TOP; break;
        case 5: cmd.mode = LedMode::BOTTOM; break;
        default: cmd.mode = LedMode::FILL; break;
    }
    execLedCommand(cmd);
}
```

---

## Issue 5: Motor enable over CAN

`parseEnableMotor` runs locally only (sets the local motor driver
enable pin). For remote motors, we need an SDO write to
`MOTOR_ENABLE[sub]` on the slave.

### Fix
In `handleMotorAct`, after calling `parseEnableMotor(doc)` for local
motors, check if `isen` is present and any motor routes to REMOTE:

```cpp
cJSON* isenItem = cJSON_GetObjectItem(doc, "isen");
if (isenItem) {
    uint8_t isen = cJSON_IsTrue(isenItem) ? 1 : 0;
    // Apply to all remote motors
    for (int ax = 0; ax < 4; ax++) {
        const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::MOTOR, ax);
        if (route && route->where == UC2::RouteEntry::REMOTE) {
            CANopenModule::writeSDO_u8(route->nodeId, UC2_OD::MOTOR_ENABLE,
                                        route->subAxis + 1, isen);
        }
    }
}
```

---

## Issue 6: Laser/LED/Home get REMOTE — always reads local

`handleLaserGet`, `handleLedGet`, `handleHomeGet` all return the local
controller's state regardless of routing. For REMOTE, they should read
from `s_remoteSlaves` cache (motor pattern) or issue SDO reads.

For PR-7.7 scope: add a "slave not on this board" warning to the
response if the route is REMOTE and no TPDO cache exists. Full SDO-read
get is deferred.

---

## Issue 7: Delete MotorJsonParser::act()

`MotorJsonParser::act()` is no longer called on CAN builds. DeviceRouter
calls the individual `parseXxx` functions directly. The monolithic
`act()` should be marked as deprecated and its body reduced to just
calling the same parseXxx functions (for non-CAN builds that still
call it via the old serial path — though those should also go through
DeviceRouter now).

---

## Claude Code prompt

```
# PR-7.8: Fix motor arrival notification, wire remaining CAN paths, clean up LED duplication

Branch: feature/canopen-pr-7-8-remaining
Depends on: current feature/runtime-config (PR-7.7 done)

## Priority order — do these in sequence, commit after each

### 1. Fix motor arrival position notification (CRITICAL — user-visible bug)

File: main/src/canopen/CANopenModule.cpp
Function: syncRpdoToModules_master(), around line 1048

Current code detects wasRunning && !nowRunning and calls
QidRegistry::reportActionDone(md->qid). This emits {"qid":N,"state":"done"}
but NOT the position JSON that ImSwitch and PS4 controller path expect.

Add a serial JSON emission BEFORE the QidRegistry call:

    if (wasRunning && !nowRunning) {
        ESP_LOGI(TAG_CO, "Slave slot %d (motor %d) DONE pos=%ld",
                 slot, logicalAx, (long)newPos);

        // Emit the same JSON format that the local motor path sends
        // via FocusMotor::sendMotorPos. This is what ImSwitch watches for.
        cJSON* root = cJSON_CreateObject();
        cJSON* stprs = cJSON_AddArrayToObject(root, "steppers");
        cJSON* item = cJSON_CreateObject();
        cJSON_AddNumberToObject(item, "stepperid", logicalAx);
        cJSON_AddNumberToObject(item, "position", newPos);
        cJSON_AddNumberToObject(item, "isDone", 1);
        cJSON_AddItemToArray(stprs, item);
        cJSON_AddNumberToObject(root, "qid", md ? md->qid : -1);
        char* jsonStr = cJSON_PrintUnformatted(root);
        if (jsonStr) {
            SerialProcess::safeSendJsonString(jsonStr);
            free(jsonStr);
        }
        cJSON_Delete(root);

        if (md && md->qid > 0) {
            QidRegistry::reportActionDone(md->qid);
        }
    }

Verify: send /motor_act to a remote motor. When it arrives, the master
serial output should include BOTH:
  {"steppers":[{"stepperid":0,"position":3000,"isDone":1}],"qid":5}
  {"qid":5,"state":"done"}

### 2. Add motor enable over CAN

File: main/src/canopen/DeviceRouter.cpp
Function: handleMotorAct(), after the parseEnableMotor(doc) call at line 115

Add:
    // Forward motor enable to remote nodes
    cJSON* isenItem = cJSON_GetObjectItem(doc, "isen");
    if (isenItem) {
        uint8_t isen = (cJSON_IsTrue(isenItem) || (cJSON_IsNumber(isenItem) && isenItem->valueint != 0)) ? 1 : 0;
        for (int ax = 0; ax < 4; ax++) {
            const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::MOTOR, ax);
            if (route && route->where == UC2::RouteEntry::REMOTE) {
#ifdef CAN_CONTROLLER_CANOPEN
                CANopenModule::writeSDO_u8(route->nodeId, UC2_OD::MOTOR_ENABLE,
                                            route->subAxis + 1, isen);
#endif
            }
        }
    }

Also wire the slave side: in syncRpdoToModules_slave, after the motor
command word dispatch, add a check for MOTOR_ENABLE changes:

    static uint8_t lastMotorEnable[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    for (int ax = 0; ax < 4; ax++) {
        uint8_t en = OD_RAM.x2005_motor_enable[ax];
        if (en != lastMotorEnable[ax]) {
            lastMotorEnable[ax] = en;
            if (en) FocusMotor::enableMotor(ax);
            else    FocusMotor::disableMotor(ax);
        }
    }

### 3. Add TMC REMOTE path

File: main/src/canopen/DeviceRouter.cpp
Function: handleTmcAct()

Replace the current "always local" body with a routing-table-aware
version. Extract stepperid from the JSON, look up
RoutingTable::find(TMC, stepperid). If REMOTE, write TMC OD entries
(0x2020 microsteps, 0x2021 current, 0x2022 stallguard) via SDO.

Wire the slave side: in syncRpdoToModules_slave, detect changes to
TMC OD entries and call TMCController::setMicrosteps/setCurrent/etc.

### 4. Add state/reboot via DeviceRouter

File: main/src/canopen/DeviceRouter.cpp

Add to routeCommand:
    if (strcmp(task, state_act_endpoint) == 0)
        return handleStateAct(doc);
    if (strcmp(task, state_get_endpoint) == 0)
        return handleStateGet(doc);

Implement handleStateAct:
- Parse the JSON for "state_act" field
- If "restart": on LOCAL, call ESP.restart(). On REMOTE,
  write REBOOT_COMMAND = 1 via SDO.
- Other state_act sub-commands: delegate to State::act(doc) locally.

Wire the slave side: in syncRpdoToModules_slave, check
OD_RAM.x2507_reboot_command. If == 1, log a message and call
ESP.restart().

Remove the state_act/state_get branches from SerialProcess::jsonProcessor
(they currently bypass DeviceRouter).

### 5. Clean up LED code duplication

File: main/src/led/LedController.cpp

Rewrite setMode() to construct a LedCommand and call execLedCommand()
instead of duplicating the switch statement. setMode should only do:
1. Disable pattern animation (activePatternId = 0)
2. Set brightness via matrix->setBrightness
3. Map the mode uint8_t to a LedMode enum value
4. Build a LedCommand struct with the mapped mode + r/g/b from colour
5. Call execLedCommand(cmd)

Leave setPattern() and setPixels() as-is — they handle genuinely
different operations (animation state and bulk buffer) that don't
map to LedCommand.

### 6. Delete MotorJsonParser::act() body

File: main/src/motor/MotorJsonParser.cpp

Replace the body of act() with:

    int act(cJSON *doc) {
        // DEPRECATED: DeviceRouter::handleMotorAct is the single entry point.
        // This function exists only for non-CAN builds that haven't migrated.
        // It calls the same sub-parsers that DeviceRouter calls.
        int qid = cJsonTool::getJsonInt(doc, "qid");
        parseEnableMotor(doc);
        parseAutoEnableMotor(doc);
        parseSetPosition(doc);
        parseMotorPinDirection(doc);
        parseSetHardLimits(doc);
        parseSetJoystickDirection(doc);
        parseMotorDriveJson(doc);
#ifdef STAGE_SCAN
        parseStageScan(doc);
#endif
        return qid;
    }

This makes it obvious that act() is just a convenience wrapper and
DeviceRouter is the canonical path.

### 7. Add REMOTE warnings to get handlers

Files: DeviceRouter.cpp — handleLaserGet, handleLedGet, handleHomeGet

For each: check the routing table. If the device routes to REMOTE and
we don't have a cached value, return a JSON with:
    {"return": 0, "error": "device is on remote node — get not implemented over CAN yet"}

This prevents silent wrong data (returning local state for a remote device).

### 8. Verification

After all steps:

1. /motor_act to remote motor → arrival message includes position + isDone
2. /motor_act with isen:1 → remote motor driver enables
3. /tmc_act with stepperid + microsteps → remote TMC configured
4. /state_act with restart → remote node reboots
5. LED setMode via CANopen → calls execLedCommand (verify with log)
6. All 4 envs compile (UC2_2, UC2_3, master, slave)
7. Stage scan works on master with remote motors
8. PS4 controller motor feedback includes position

### 9. Do NOT

- Do NOT change the JSON wire format
- Do NOT add new OD entries (use existing 0x2000-0x2F05)
- Do NOT touch can_transport.cpp
- Do NOT refactor FocusMotor::sendMotorPos — just replicate its JSON
  format in syncRpdoToModules_master
- Do NOT move the TMC slave-side dispatch into TMCController — keep it
  in syncRpdoToModules_slave alongside motor/laser/led/galvo for consistency
```

---

## Architecture after all fixes

```
Serial / WiFi / BT
    │
    ▼
SerialProcess::jsonProcessor
    │
    ├──► DeviceRouter::routeCommand ◄── THE SINGLE DISPATCHER
    │     │
    │     ├── /motor_act → config parsers + per-stepper LOCAL/REMOTE
    │     ├── /motor_get → cache read (REMOTE) or FocusMotor (LOCAL)
    │     ├── /laser_act → LaserController (LOCAL) or SDO (REMOTE)
    │     ├── /laser_get → LaserController (LOCAL) or warn (REMOTE)
    │     ├── /ledarr_act → LedController (LOCAL) or SDO (REMOTE)
    │     ├── /home_act → HomeMotor (LOCAL) or SDO (REMOTE)
    │     ├── /galvo_act → GalvoController (LOCAL) or SDO (REMOTE)
    │     ├── /tmc_act → TMCController (LOCAL) or SDO (REMOTE) ← NEW
    │     ├── /state_act → State (LOCAL) or SDO reboot (REMOTE) ← NEW
    │     └── /state_get → local or SDO uptime read (REMOTE)
    │
    ├──► Non-routed controllers (analogout, BT, DAC, I2C, ...)           
    │
    └──► CANopen meta-commands (/can_act, /can_get)

Slave side (syncRpdoToModules_slave):
    OD_RAM changes → dispatch to local controllers
    ├── motor command word → s_axisCmds → FocusMotor
    ├── motor enable → FocusMotor::enableMotor ← NEW
    ├── laser PWM → LaserController::setLaserVal
    ├── LED mode/colour → LedController::setMode (via execLedCommand) ← FIXED
    ├── LED pattern → LedController::setPattern
    ├── galvo command → GalvoController
    ├── TMC params → TMCController ← NEW
    └── reboot command → ESP.restart() ← NEW

Master side (syncRpdoToModules_master):
    RPDO arrival → s_remoteSlaves cache
    └── running→stopped → emit position JSON + QidRegistry ← FIXED
```
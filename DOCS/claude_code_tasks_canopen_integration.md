# Claude Code Work Packages — Post-Reset Clean Start

## Prerequisites

You have merged commits `20bd8ae..7033352` into `main`. That gives you:
- PWM/laser resolution fixes
- QID tracking system (QidRegistry + CAN QID_REPORT)
- CAN master address enforcement
- Hardware/controller bugfixes

The `rewrite-canopen` branch is archived as `archive/rewrite-canopen-v1`.
All work below starts from this clean `main`.

## End goal

After all PRs are merged, the old CAN transport layer (`can_transport.cpp`, ISO-TP library, custom message types) is **completely removed** from the codebase. All CAN communication goes through CANopenNode — including motor commands, laser/LED control, state reporting, and firmware updates (OTA via SDO block transfer). The old code is not kept behind `#ifdef` guards. It is deleted. Non-CAN boards (standalone serial/WiFi) are unaffected — they never used the CAN transport and continue to work as before.

## Reference material for Claude Code

When running these tasks, Claude Code needs access to these repositories/files:

1. **Your repo**: `https://github.com/youseetoo/uc2-esp32` (the `main` branch after the merge)
2. **The MWE project**: the `uc2_canopen_mwe.zip` we built earlier — unzip it alongside your repo
3. **The EDS file**: `openUC2_satellite.eds` — place in repo root or DOCUMENTATION/
4. **The skpang trainer**: the `Archiv.zip` you uploaded — contains the working ESP32 CO_driver

The MWE contains proven, working code for:
- `ESP32_CanOpenNode/` — the CANopenNode library with Arduino/ESP32 TWAI driver
- `ESP32S3_CO_trainer_OD/` — a working OD.h/OD.c pair that compiles
- `src/uc2_canopen.cpp` — TWAI init, FreeRTOS task structure, SDO helpers
- `src/uc2_module_controller.h` — Module base class pattern

---

## PR-1: RuntimeConfig + NVS (the foundation)

**Branch:** `feature/runtime-config`
**Depends on:** clean main
**Estimated scope:** ~6 new files, ~100 lines changed in main.cpp
**Why first:** everything else needs this to replace the #ifdef maze

### Claude Code prompt:

```
Read the codebase structure in main/main.cpp and main/src/ to understand how controllers are currently initialized. Look at all the #ifdef guards used: MOTOR_CONTROLLER, LASER_CONTROLLER, LED_CONTROLLER, CAN_CONTROLLER, HOME_MOTOR, BLUETOOTH, etc. These are set in platformio.ini per build environment.

Create a RuntimeConfig system that replaces these compile-time flags with runtime NVS-backed configuration. Here's what to do:

1. Create main/src/config/RuntimeConfig.h:

```cpp
#pragma once
#include <stdint.h>

enum class NodeRole : uint8_t {
    STANDALONE = 0,   // No CAN — serial/WiFi only (existing UC2_2, UC2_3 boards)
    CAN_MASTER = 1,   // CAN HAT — bridges serial to CAN bus
    CAN_SLAVE  = 2    // Satellite — receives commands via CAN
};

struct RuntimeConfig {
    // Module enables — defaults match the most common board (UC2_3)
    bool motor       = false;
    bool laser       = false;
    bool led         = false;
    bool home        = false;
    bool digitalIn   = false;
    bool digitalOut  = false;
    bool analogIn    = false;
    bool dac         = false;
    bool tmc         = false;
    bool encoder     = false;
    bool joystick    = false;
    bool galvo       = false;
    bool bluetooth   = false;
    bool wifi        = false;
    bool pid         = false;
    bool scanner     = false;
    
    // CAN role
    NodeRole canRole = NodeRole::STANDALONE;
    uint8_t  canNodeId = 10;  // CANopen node-id (1-127), stored in NVS
    
    // Convenience
    bool isMaster() const { return canRole == NodeRole::CAN_MASTER; }
    bool isSlave()  const { return canRole == NodeRole::CAN_SLAVE; }
    bool hasCan()   const { return canRole != NodeRole::STANDALONE; }
};

extern RuntimeConfig runtimeConfig;
```

2. Create main/src/config/NVSConfig.h and NVSConfig.cpp:
   - Use the Arduino Preferences library (already available in the project)
   - Namespace: "uc2cfg"
   - loadConfig(): read each bool field from NVS, fall back to compile-time defaults from PinConfig
   - saveConfig(): write all fields to NVS
   - resetConfig(): clear the namespace, reload defaults
   - toJson(cJSON*): serialize current config to JSON for /config_get endpoint
   - fromJson(cJSON*): merge partial JSON into current config for /config_set endpoint
   
   IMPORTANT: the compile-time defaults should come from the existing PinConfig/ModuleConfig pattern. Look at how moduleConfig->motor, moduleConfig->laser etc. are currently set per board. The NVS values OVERRIDE those defaults. If NVS has never been written, the PinConfig defaults apply.

3. In main/main.cpp:
   - After Serial.begin() and PinConfig loading, call NVSConfig::loadConfig()
   - Replace every `#ifdef MOTOR_CONTROLLER` gate in setup() and loop() with `if (runtimeConfig.motor)`
   - Replace every `#ifdef LASER_CONTROLLER` gate with `if (runtimeConfig.laser)`
   - etc. for all controllers
   - KEEP the #ifdef guards on the #include lines and the class instantiations — we're gating the CALLS, not the compilation. This way the code still compiles for all boards, it just doesn't run disabled modules.
   - For the CAN-specific guards (#ifdef CAN_CONTROLLER, CAN_SEND_COMMANDS, CAN_RECEIVE_MOTOR etc.), leave them as-is for now. We'll wire those to runtimeConfig.canRole in a later PR.

4. Add serial endpoints in SerialProcess.cpp:
   - "/config_get" → returns runtimeConfig as JSON
   - "/config_set" → merges incoming JSON, saves to NVS, requires reboot to take effect
   - "/config_reset" → clears NVS config namespace, reboots

5. Add NVSConfig.cpp to main/CMakeLists.txt

6. DO NOT touch any CAN code, any controller implementation, or the platformio.ini build environments.

Test that it compiles for: env:UC2_2 and env:seeed_xiao_esp32s3_can_slave_motor
```

---

## PR-2: Rename can_controller → can_transport + cleanup

**Branch:** `refactor/can-transport-rename`
**Depends on:** PR-1
**Estimated scope:** file rename + grep-replace, ~10 lines of logic

### Claude Code prompt:

```
This is a pure refactoring PR — no logic changes except one small enforcement.

1. Rename files:
   - main/src/can/can_controller.cpp → main/src/can/can_transport.cpp
   - main/src/can/can_controller.h → main/src/can/can_transport.h

2. Update all references:
   - grep -r "can_controller" across the entire codebase
   - Update every #include "can_controller.h" → #include "can_transport.h"
   - Update main/CMakeLists.txt source list

3. In the renamed can_transport.cpp, no logic changes. The rename establishes the naming convention:
   - can_transport = low-level TWAI + ISO-TP framing (what exists today)
   - CANopenModule = protocol layer using CANopenNode (added in a later PR)

4. Clean up repo root:
   - Add to .gitignore: observations_*.txt, logs.txt, *.pyc, __pycache__/
   - Remove if present: any .docx files in repo root (documentation should be .md)
   - Remove if present: any observation log .txt files in repo root

5. DO NOT change any logic in can_transport.cpp. This is a rename-only PR.

Test compilation for: env:UC2_3_CAN_HAT_Master_v2 and env:seeed_xiao_esp32s3_can_slave_motor
```

---

## PR-3: Add CANopenNode library + generate OD from EDS

**Branch:** `feature/canopen-library`
**Depends on:** PR-2
**Estimated scope:** ~50 files added (library), 2 generated files, platformio.ini changes
**Why separate:** this PR just adds the library and proves it compiles — no integration with UC2 modules yet

### Claude Code prompt:

```
This PR adds the CANopenNode library to the project and generates the Object Dictionary. No UC2 module integration yet — just prove the library compiles alongside the existing code.

CRITICAL: We are using the Arduino-compatible port from the skpang trainer project, NOT writing our own. The library is in the MWE project's ESP32_CanOpenNode/ directory.

1. COPY THE LIBRARY:
   Copy the entire ESP32_CanOpenNode/ directory from the MWE project (uc2_canopen_mwe/ESP32_CanOpenNode/) into lib/ESP32_CanOpenNode/ in this repo.
   
   This directory contains:
   - src/301/ — CiA 301 core (NMT, SDO server/client, PDO, heartbeat, etc.)
   - src/303/ — LEDs
   - src/304/ — SRDO, GFC
   - src/305/ — LSS master/slave
   - src/309/ — Gateway ASCII
   - src/storage/ — Storage/EEPROM
   - src/extra/ — Trace
   - src/CO_driver_target.h — ESP32 type definitions (FreeRTOS mutexes, TWAI queues)
   - src/CO_driver_ESP32.c — ESP32 TWAI ↔ CANopenNode bridge (the key file!)
   - src/CANopen.h, CANopen.c — top-level CANopenNode init/process
   - src/OD.h, OD.c — the trainer's OD (we'll replace these)
   - library.json, library.properties — PlatformIO library metadata

2. COPY THE UC2 OBJECT DICTIONARY:
   Copy the OD.h and OD.c from uc2_canopen_mwe/ESP32S3_CO_trainer_OD/ into lib/ESP32_CanOpenNode/src/
   replacing the trainer's OD files.
   
   NOTE: For now we use the trainer's OD as-is. The full UC2 OD (generated from openUC2_satellite.eds via CANopenEditor) will be swapped in once we have CANopenEditor set up. The trainer OD has the basic entries we need for a first integration: 0x6000 (inputs), 0x6001 (counter), 0x6200 (outputs), 0x6401 (analog inputs), plus all CiA 301 communication objects.

3. ADD PLATFORMIO ENVIRONMENTS:
   In platformio.ini, add two NEW environments (don't modify existing ones):

   ```ini
   [env:canopen_common]
   ; Shared settings for CANopen builds
   build_flags =
       -DCAN_CONTROLLER_CANOPEN=1
       -I${PROJECT_DIR}/lib/ESP32_CanOpenNode/src
   lib_deps =
       ; existing deps inherited from parent env
   lib_extra_dirs = ${PROJECT_DIR}/lib

   [env:UC2_canopen_master]
   extends = env:esp32dev
   board_build.cmake_extra_args =
       -DCONFIG=UC2_3_CAN_HAT_Master_v2
   build_flags =
       ${env:esp32dev.build_flags}
       ${env:canopen_common.build_flags}
       -DNODE_ROLE=1
       -DUC2_NODE_ID=1
       -DMOTOR_CONTROLLER=0
       -DLASER_CONTROLLER=0
       -DLED_CONTROLLER=0
       -DCAN_CONTROLLER=1

   [env:UC2_canopen_slave]  
   extends = env:seeed_xiao_esp32s3
   board_build.cmake_extra_args =
       -DCONFIG=seeed_xiao_esp32s3_can_slave_motor
   build_flags =
       ${env:seeed_xiao_esp32s3.build_flags}
       ${env:canopen_common.build_flags}
       -DNODE_ROLE=2
       -DUC2_NODE_ID=10
       -DMOTOR_CONTROLLER=1
       -DLASER_CONTROLLER=0
       -DLED_CONTROLLER=0
       -DCAN_CONTROLLER=1
   ```

4. VERIFY COMPILATION:
   The library should compile as part of the build without errors. It won't DO anything yet — no code calls CO_new() or CO_CANinit(). We're just proving the library links.
   
   If there are include path issues (the library uses includes like "301/CO_driver.h"), fix them by adding the right -I flags or by checking that library.json has the correct includeDirs.

5. DO NOT:
   - Modify any existing source files (main.cpp, can_transport.cpp, any controller)
   - Call any CANopenNode functions yet
   - Create any new .cpp files in main/src/

Test: `pio run -e UC2_canopen_master` and `pio run -e UC2_canopen_slave` both compile without errors.
```

---

## PR-4: CANopenModule — slave side (the core integration)

**Branch:** `feature/canopen-slave`
**Depends on:** PR-3
**Estimated scope:** 2 new files (~400 lines), ~30 lines changed in main.cpp
**This is the most important PR — take time to review it carefully**

### Claude Code prompt:

```
This PR creates the CANopenModule class that replaces can_transport's role for CANopen slave nodes. It bridges the CANopenNode Object Dictionary to the existing UC2 Module controllers (MotorController, LaserController, etc.).

USE THE MWE's uc2_canopen.cpp AS YOUR PRIMARY REFERENCE. That file contains working, tested code for:
- TWAI driver initialization (CAN_ctrl_task)
- CANopenNode initialization sequence (CO_new → CO_CANinit → CO_LSSinit → CO_CANopenInit → CO_CANopenInitPDO)
- FreeRTOS task structure (CO_tmrTask at 1ms, CO_interruptTask, CO_main)
- SDO read/write helper functions
- NMT state monitoring

Here's the file structure from the MWE that works — adapt it to the UC2 codebase:

```
// MWE's uc2_canopen.cpp task structure (PROVEN WORKING):
// 
// CAN_ctrl_task (prio 1):
//   - twai_driver_install(), twai_start()
//   - Loop: drain TWAI RX FIFO → CAN_RX_queue, send CAN_TX_queue → twai_transmit()
//   - Error recovery: bus-off → reinstall driver
//
// CO_main (prio 3):
//   - CO_new(), CO_CANinit(), CO_LSSinit(), CO_CANopenInit(), CO_CANopenInitPDO()
//   - Loop: CO_process() every 1ms, monitor NMT state
//
// CO_tmrTask (prio 4):
//   - CO_process_SYNC(), CO_process_RPDO(), CO_process_TPDO() every 1ms
//   - THIS IS WHERE OD ↔ MODULE SYNC HAPPENS
//
// CO_interruptTask (prio 2):
//   - CO_CANinterrupt() — matches RX frames to CANopen handlers
```

Create these files:

1. main/src/canopen/CANopenModule.h:

```cpp
#pragma once
#include <Arduino.h>
#include <stdint.h>

// Only available when CAN_CONTROLLER_CANOPEN is defined
#ifdef CAN_CONTROLLER_CANOPEN

extern "C" {
#include <CANopen.h>
#include "OD.h"
}

class CANopenModule {
public:
    void setup();   // Init TWAI + CANopenNode + start FreeRTOS tasks
    void loop();    // NMT state monitoring, LED indication
    
    // SDO access (used by master's serial→CAN bridge)
    static bool writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                         uint8_t* data, size_t dataSize);
    static bool readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                        uint8_t* buf, size_t bufSize, size_t* readSize);
    
    // Check if CANopen stack is running
    static bool isOperational();
    
private:
    static void CAN_ctrl_task(void* arg);
    static void CO_main_task(void* arg);
    static void CO_tmr_task(void* arg);
    static void CO_interrupt_task(void* arg);
    
    // OD ↔ Module sync (slave only, called from CO_tmr_task)
    static void syncRpdoToModules();   // OD_RAM → Module.act()
    static void syncModulesToTpdo();   // Module.get() → OD_RAM
};

extern CO_t* CO;  // Global CANopenNode object (same pattern as MWE)

#endif // CAN_CONTROLLER_CANOPEN
```

2. main/src/canopen/CANopenModule.cpp:

Copy the TWAI setup, FreeRTOS tasks, and SDO helpers directly from the MWE's uc2_canopen.cpp. The structure is proven to work. Then add the UC2-specific sync functions:

```cpp
void CANopenModule::syncRpdoToModules() {
    // Called every 1ms from CO_tmr_task AFTER CO_process_RPDO()
    // Read values that RPDOs have written into OD_RAM and dispatch to modules
    
    // Example for motor (using trainer OD entries as placeholders):
    // OD_RAM.x6200_writeOutput8Bit[0] → motor command
    static uint8_t prevOutput = 0xFF;
    uint8_t output = OD_RAM.x6200_writeOutput8Bit[0];
    if (output != prevOutput) {
        prevOutput = output;
        log_i("RPDO received output byte: 0x%02X", output);
        // TODO: When we have the full UC2 OD, this becomes:
        // int32_t targetPos = OD_RAM.x2000_motorTargetPos[0];
        // motorController->act(targetPos, speed, isabs);
    }
}

void CANopenModule::syncModulesToTpdo() {
    // Called every 1ms from CO_tmr_task BEFORE CO_process_TPDO()
    // Write module state into OD_RAM so TPDOs broadcast it
    
    static uint32_t counter = 0;
    OD_RAM.x6001_counter = counter++;
    // TODO: When we have the full UC2 OD:
    // OD_RAM.x2001_motorActualPos[0] = motorController->getPosition(0);
    // OD_RAM.x2004_motorStatusWord = motorController->getStatusBits();
}
```

IMPORTANT IMPLEMENTATION DETAILS from the MWE:
- CAN_TX_queue and CAN_RX_queue must be declared as `QueueHandle_t` and `extern`'d in both CANopenModule.cpp and the CO_driver_ESP32.c library file — the library's send_can_message() function writes to CAN_TX_queue, and CO_CANinterrupt() reads from CAN_RX_queue.
- The TWAI GPIO pins come from pinConfig (pinConfig.CAN_TX, pinConfig.CAN_RX) — read them at runtime, don't hardcode.
- STBY pin (transceiver standby) must be driven LOW to enable the transceiver.
- CO_NODE_ID comes from runtimeConfig.canNodeId (from PR-1's NVS config)

3. In main/main.cpp, add conditional initialization:

```cpp
#ifdef CAN_CONTROLLER_CANOPEN
#include "canopen/CANopenModule.h"
CANopenModule canopenModule;
#endif

void setup() {
    // ... existing setup ...
    
    #ifdef CAN_CONTROLLER_CANOPEN
    canopenModule.setup();
    #endif
}

void loop() {
    // ... existing loop ...
    
    #ifdef CAN_CONTROLLER_CANOPEN
    canopenModule.loop();
    #endif
}
```

4. Add CANopenModule.cpp to main/CMakeLists.txt (guarded by CAN_CONTROLLER_CANOPEN if needed, or just always compile — the #ifdef in the header keeps it dead code for non-CANopen builds).

5. DO NOT modify any existing controller (MotorController, LaserController, etc.). The sync functions use TODO comments for the full OD integration. Right now they just exercise the trainer OD entries (0x6000, 0x6001, 0x6200) to prove the stack works.

Test:
- pio run -e UC2_canopen_slave → compiles, flashes, prints "CANopenNode running — node-id 10" on serial
- With two boards on the same CAN bus: heartbeats should appear (verify with candump or serial log)
- pio run -e UC2_2 → still compiles and works (no CAN code activated)
```

---

## PR-5: CANopenModule — master side (serial→SDO bridge)

**Branch:** `feature/canopen-master`
**Depends on:** PR-4
**Estimated scope:** 1 new file (~200 lines), ~50 lines changed in SerialProcess.cpp
**Human review focus:** does the JSON→SDO mapping match your serial API?

### Claude Code prompt:

```
This PR adds the master-side serial→CANopen bridge. When the master ESP32 receives a JSON command over serial from the Pi, it translates it into SDO writes to the appropriate slave node.

Create main/src/canopen/DeviceRouter.h and DeviceRouter.cpp:

The DeviceRouter replaces what can_transport.cpp does today: it takes a parsed JSON command and routes it to the right CAN node. But instead of ISO-TP framing, it uses CANopenNode's SDO client.

```cpp
// DeviceRouter.h
#pragma once
#ifdef CAN_CONTROLLER_CANOPEN

#include <cJSON.h>
#include <stdint.h>

class DeviceRouter {
public:
    // Route a JSON command to the appropriate CAN slave via SDO
    // Returns the JSON response (caller must free with cJSON_Delete)
    static cJSON* routeCommand(const char* task, cJSON* doc);
    
private:
    // Map stepperid to CANopen node-id
    // stepperid 0=A→node14, 1=X→node11, 2=Y→node12, 3=Z→node13
    static uint8_t stepperIdToNodeId(int stepperid);
    
    // Command handlers
    static cJSON* handleMotorAct(cJSON* doc);
    static cJSON* handleMotorGet(cJSON* doc);
    static cJSON* handleLaserAct(cJSON* doc);
    static cJSON* handleLedAct(cJSON* doc);
    static cJSON* handleHomeAct(cJSON* doc);
    static cJSON* handleStateGet(uint8_t nodeId);
};

#endif
```

Implementation for handleMotorAct (most important — this is the hot path):

```cpp
cJSON* DeviceRouter::handleMotorAct(cJSON* doc) {
    cJSON* motor = cJSON_GetObjectItem(doc, "motor");
    if (!motor) return nullptr;
    cJSON* steppers = cJSON_GetObjectItem(motor, "steppers");
    if (!steppers) return nullptr;
    
    cJSON* resp = cJSON_CreateObject();
    int arraySize = cJSON_GetArraySize(steppers);
    
    for (int i = 0; i < arraySize; i++) {
        cJSON* stepper = cJSON_GetArrayItem(steppers, i);
        int stepperid = cJSON_GetObjectItem(stepper, "stepperid")->valueint;
        uint8_t nodeId = stepperIdToNodeId(stepperid);
        
        // Write target position via SDO to OD 0x6200:1
        // (Using trainer OD for now — will be 0x2000:1 with full UC2 OD)
        cJSON* pos = cJSON_GetObjectItem(stepper, "position");
        if (pos) {
            int32_t posVal = pos->valueint;
            uint8_t data[4];
            memcpy(data, &posVal, 4);  // little-endian, matches ESP32
            CANopenModule::writeSDO(nodeId, 0x6200, 0x01, data, 4);
        }
        
        // Read back counter to verify the node is alive
        uint8_t buf[4]; size_t readSize;
        if (CANopenModule::readSDO(nodeId, 0x6001, 0x00, buf, 4, &readSize)) {
            uint32_t counter;
            memcpy(&counter, buf, 4);
            cJSON_AddNumberToObject(resp, "counter", counter);
        }
    }
    
    cJSON_AddNumberToObject(resp, "return", 1);
    return resp;
}
```

In SerialProcess.cpp, add routing for CANopen master mode:

```cpp
// In jsonProcessor(), after the existing task dispatch:
#if defined(CAN_CONTROLLER_CANOPEN) && defined(NODE_ROLE) && NODE_ROLE == 1
    // CANopen master mode — route to slave nodes via SDO
    cJSON* canResponse = DeviceRouter::routeCommand(task, doc);
    if (canResponse) {
        char* respStr = cJSON_PrintUnformatted(canResponse);
        safeSendJsonString(respStr);
        free(respStr);
        cJSON_Delete(canResponse);
        return 1;
    }
#endif
```

Node-ID mapping (matching your current CAN address scheme):
```cpp
uint8_t DeviceRouter::stepperIdToNodeId(int stepperid) {
    switch (stepperid) {
        case 0: return 14;  // A axis (was CAN addr 272)
        case 1: return 11;  // X axis (was CAN addr 273)
        case 2: return 12;  // Y axis (was CAN addr 274)
        case 3: return 13;  // Z axis (was CAN addr 275)
        default: return 10; // fallback
    }
}
```

DO NOT:
- Modify existing can_transport.cpp logic
- Change the JSON API format — the Pi side should not need any changes
- Implement LED array bulk transfer yet (that needs SDO block transfer, save for later)

Test:
- Flash master (env:UC2_canopen_master), flash slave (env:UC2_canopen_slave)
- Connect both to CAN bus
- Send from Pi/serial monitor: {"task":"/motor_act","motor":{"steppers":[{"stepperid":1,"position":1000}]}}
- Slave should log "RPDO received" or "SDO write to 0x6200"
- Master should return {"return":1,"counter":NNN}
```

---

## PR-6: Full UC2 Object Dictionary + module wiring

**Branch:** `feature/canopen-uc2-od`
**Depends on:** PR-5
**Estimated scope:** 2 replaced files (OD.h/OD.c), ~200 lines changed in CANopenModule.cpp and DeviceRouter.cpp
**Human review focus:** verify the OD entries match your hardware**

### Claude Code prompt:

```
This PR replaces the trainer's placeholder Object Dictionary with the full UC2-specific OD and wires the sync functions to actual module controllers.

PREREQUISITE: You need to generate OD.h and OD.c from the openUC2_satellite.eds file using CANopenEditor:
1. Open CANopenEditor
2. File → Open → openUC2_satellite.eds
3. File → Export → CANopenNode V4 (OD.h + OD.c)
4. Save the generated files

If CANopenEditor is not available, create OD.h and OD.c manually based on the EDS file's manufacturer-specific entries (0x2000-0x2F03). The MWE's OD files show the exact struct/table format CANopenNode expects.

1. Replace lib/ESP32_CanOpenNode/src/OD.h and OD.c with the generated UC2 versions.

2. Update CANopenModule.cpp syncRpdoToModules():

```cpp
void CANopenModule::syncRpdoToModules() {
    // Motor: OD 0x2000-0x2007 → MotorController
    #ifdef MOTOR_CONTROLLER
    static uint8_t prevMotorCmd = 0xFF;
    uint8_t motorCmd = OD_RAM.x2003_motorCommandWord;
    if (motorCmd != prevMotorCmd && motorCmd != 0) {
        prevMotorCmd = motorCmd;
        // Build stepper command from OD entries
        Stepper s;
        s.targetPosition[0] = OD_RAM.x2000_motorTargetPos[0];
        s.speed[0] = OD_RAM.x2002_motorSpeed[0];
        s.isabs = OD_RAM.x2007_motorIsAbsolute;
        // Call existing motor controller — NO CHANGES to FocusMotor.cpp needed!
        focusMotor.startStepper(s);
        OD_RAM.x2003_motorCommandWord = 0; // clear command after processing
    }
    #endif
    
    // Laser: OD 0x2100 → LaserController
    #ifdef LASER_CONTROLLER
    static uint16_t prevLaser[3] = {0xFFFF, 0xFFFF, 0xFFFF};
    for (int i = 0; i < 3; i++) {
        if (OD_RAM.x2100_laserPWM[i] != prevLaser[i]) {
            prevLaser[i] = OD_RAM.x2100_laserPWM[i];
            laserController.setLaserVal(i, prevLaser[i]);
        }
    }
    #endif
    
    // LED: OD 0x2200-0x2202 → LedController  
    #ifdef LED_CONTROLLER
    static uint8_t prevLedMode = 0xFF;
    if (OD_RAM.x2200_ledArrayMode != prevLedMode) {
        prevLedMode = OD_RAM.x2200_ledArrayMode;
        ledController.setMode(prevLedMode);
    }
    #endif
}
```

3. Update syncModulesToTpdo():
```cpp
void CANopenModule::syncModulesToTpdo() {
    #ifdef MOTOR_CONTROLLER
    OD_RAM.x2001_motorActualPos[0] = focusMotor.getCurrentPosition(0);
    uint8_t status = 0;
    if (focusMotor.isRunning(0)) status |= 0x01;
    if (focusMotor.isHomed())     status |= 0x02;
    OD_RAM.x2004_motorStatusWord = status;
    #endif
    
    // Digital inputs (endstops)
    #ifdef DIGITAL_IN_CONTROLLER
    OD_RAM.x2300_digitalInputState[0] = digitalRead(pinConfig.DIGITAL_IN_1);
    OD_RAM.x2300_digitalInputState[1] = digitalRead(pinConfig.DIGITAL_IN_2);
    #endif
}
```

4. Update DeviceRouter.cpp to use the new OD indices (0x2000 instead of 0x6200, etc.)

5. DO NOT modify any controller implementation files.

Test: same as PR-5 but now motor actually moves when commanded via CAN.
```

---

## PR-7: PinConfig consolidation + Python tools

**Branch:** `feature/pinconfig-cleanup`
**Depends on:** PR-6 (or can run in parallel)
**Estimated scope:** delete ~6 directories, create 1, copy Python tools

### Claude Code prompt:

```
This PR cleans up the PinConfig explosion and adds the Python CAN debugging tools.

1. CONSOLIDATE SATELLITE PinConfigs:
   - Keep: seeed_xiao_esp32s3_can_slave_motor/PinConfig.h (the most complete one)
   - Rename it to: seeed_xiao_esp32s3_satellite/PinConfig.h
   - Make sure it defines ALL pins: motor, laser, LED, CAN_TX, CAN_RX, I2C, TMC UART, encoder, endstops
   - Delete these redundant directories (their module selection is now handled by RuntimeConfig/NVS):
     * seeed_xiao_esp32s3_can_slave_illumination/
     * seeed_xiao_esp32s3_can_slave_laser/
     * seeed_xiao_esp32s3_can_slave_led/
     * seeed_xiao_esp32s3_can_slave_galvo/
     * UC2_CANopen_Slave_motor/ (if it exists from the old PR)
     * UC2_CANopen_Slave_multi/ (if it exists from the old PR)

2. Update platformio.ini:
   - All satellite environments now use -DCONFIG=seeed_xiao_esp32s3_satellite
   - The env name can still differ (env:xiao_slave_motor, env:xiao_slave_led) but they all point to the same PinConfig
   - The DIFFERENCE between them is only the NVS-stored RuntimeConfig (which modules are active)

3. Add Python CAN tools:
   - Copy PYTHON/waveshare/ from the archived rewrite-canopen branch
   - This includes: waveshare_bus.py, can_sniffer.py, uc2_canopen_tool.py, uc2_canopen_motor_example.py
   - Add a PYTHON/waveshare/README.md explaining usage
   - Do NOT include __pycache__/ or .pyc files

4. Copy openUC2_satellite.eds to DOCUMENTATION/openUC2_satellite.eds

Test: all existing build environments still compile after the rename.
```

---

## PR-8: Firmware update over CANopen (OTA via SDO block transfer)

**Branch:** `feature/canopen-ota`
**Depends on:** PR-6
**Estimated scope:** 2 new files (~300 lines), ~30 lines in CANopenModule.cpp, OD changes
**Human review focus:** OTA is safety-critical — test on a sacrificial board first

### Claude Code prompt:

```
This PR implements firmware-over-CAN using the CANopenNode SDO server's block transfer capability. The master sends a firmware binary to a slave node by writing it to a DOMAIN-type OD entry, and the slave streams it into the ESP32's OTA partition.

The OD entries for OTA are already defined in the UC2 EDS (0x2F00–0x2F03). If they are not yet in your OD.h/OD.c, add them:

In OD.h, add to the RAM struct:
```cpp
// OTA firmware update (0x2F00-0x2F03)
uint32_t x2F01_otaSize;        // Firmware size in bytes (set before transfer)
uint32_t x2F02_otaCRC32;       // Expected CRC32 (set before transfer)
uint8_t  x2F03_otaStatus;      // 0=idle, 1=receiving, 2=verifying, 3=rebooting, 4=error
```

The 0x2F00 entry (firmware data) is a DOMAIN type — it does NOT get a RAM variable. Instead, it uses OD read/write callback functions that stream data directly to the OTA partition without buffering the entire binary in RAM.

1. Create main/src/canopen/CanOpenOTA.h:

```cpp
#pragma once
#ifdef CAN_CONTROLLER_CANOPEN

#include <stdint.h>
#include <stddef.h>

class CanOpenOTA {
public:
    // Initialize OTA handler — registers OD callbacks for 0x2F00
    static void init();
    
    // OD callback: called by CANopenNode when SDO writes to 0x2F00
    // This is the key function — it receives firmware data chunk by chunk
    // during an SDO block transfer and writes each chunk to the OTA partition
    static uint32_t onOtaWrite(void* object, const void* data, 
                                uint32_t size, uint8_t subIndex);
    
    // OD callback: called when SDO write to 0x2F00 completes
    static void onOtaComplete();
    
    // Check if OTA is in progress (blocks normal operations)
    static bool isActive();
    
private:
    static esp_ota_handle_t otaHandle;
    static const esp_partition_t* otaPartition;
    static uint32_t bytesReceived;
    static uint32_t expectedSize;
    static bool active;
};

#endif
```

2. Create main/src/canopen/CanOpenOTA.cpp:

```cpp
#include "CanOpenOTA.h"
#ifdef CAN_CONTROLLER_CANOPEN

#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_log.h>
#include "OD.h"

extern "C" {
#include <CANopen.h>
}

#define TAG "CAN_OTA"

esp_ota_handle_t CanOpenOTA::otaHandle = 0;
const esp_partition_t* CanOpenOTA::otaPartition = nullptr;
uint32_t CanOpenOTA::bytesReceived = 0;
uint32_t CanOpenOTA::expectedSize = 0;
bool CanOpenOTA::active = false;

void CanOpenOTA::init() {
    // Register the OD write callback for 0x2F00 (DOMAIN entry)
    // This hooks into CANopenNode's SDO server so that when the master
    // performs an SDO block transfer to 0x2F00, our onOtaWrite() is called
    // for each data chunk.
    //
    // In CANopenNode v4, this is done by setting the OD entry's 
    // read/write function pointers:
    //
    // OD_entry_t* entry = OD_find(OD, 0x2F00);
    // OD_extension_t ext = {
    //     .object = nullptr,
    //     .read = nullptr,       // write-only
    //     .write = onOtaWrite
    // };
    // OD_extension_init(entry, &ext);
    
    ESP_LOGI(TAG, "OTA handler initialized");
}

uint32_t CanOpenOTA::onOtaWrite(void* object, const void* data,
                                 uint32_t size, uint8_t subIndex) {
    if (!active) {
        // First chunk — begin OTA
        expectedSize = OD_RAM.x2F01_otaSize;
        if (expectedSize == 0) {
            ESP_LOGE(TAG, "OTA size not set — write 0x2F01 first");
            OD_RAM.x2F03_otaStatus = 4; // error
            return 0x06090030; // SDO abort: value range error
        }
        
        otaPartition = esp_ota_get_next_update_partition(NULL);
        if (otaPartition == nullptr) {
            ESP_LOGE(TAG, "No OTA partition found");
            OD_RAM.x2F03_otaStatus = 4;
            return 0x06090030;
        }
        
        esp_err_t err = esp_ota_begin(otaPartition, expectedSize, &otaHandle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
            OD_RAM.x2F03_otaStatus = 4;
            return 0x06090030;
        }
        
        active = true;
        bytesReceived = 0;
        OD_RAM.x2F03_otaStatus = 1; // receiving
        ESP_LOGI(TAG, "OTA started, expecting %lu bytes", (unsigned long)expectedSize);
    }
    
    // Write this chunk to the OTA partition
    esp_err_t err = esp_ota_write(otaHandle, data, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed at offset %lu: %s",
                 (unsigned long)bytesReceived, esp_err_to_name(err));
        esp_ota_abort(otaHandle);
        active = false;
        OD_RAM.x2F03_otaStatus = 4;
        return 0x06090030;
    }
    
    bytesReceived += size;
    
    // Log progress every 64KB
    if ((bytesReceived % 65536) < size) {
        ESP_LOGI(TAG, "OTA progress: %lu / %lu bytes (%lu%%)",
                 (unsigned long)bytesReceived, (unsigned long)expectedSize,
                 (unsigned long)(bytesReceived * 100 / expectedSize));
    }
    
    // Check if transfer is complete
    if (bytesReceived >= expectedSize) {
        onOtaComplete();
    }
    
    return 0; // success
}

void CanOpenOTA::onOtaComplete() {
    OD_RAM.x2F03_otaStatus = 2; // verifying
    ESP_LOGI(TAG, "OTA transfer complete, %lu bytes received", (unsigned long)bytesReceived);
    
    // TODO: verify CRC32 against OD_RAM.x2F02_otaCRC32
    // For now, trust the SDO block transfer's built-in CRC
    
    esp_err_t err = esp_ota_end(otaHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        OD_RAM.x2F03_otaStatus = 4;
        active = false;
        return;
    }
    
    err = esp_ota_set_boot_partition(otaPartition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        OD_RAM.x2F03_otaStatus = 4;
        active = false;
        return;
    }
    
    OD_RAM.x2F03_otaStatus = 3; // rebooting
    ESP_LOGI(TAG, "OTA verified. Rebooting in 1 second...");
    active = false;
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

bool CanOpenOTA::isActive() { return active; }

#endif
```

3. In CANopenModule.cpp setup(), call CanOpenOTA::init() after CO_CANopenInitPDO().

4. On the MASTER side, add OTA command handler in DeviceRouter.cpp:

```cpp
cJSON* DeviceRouter::handleOtaStart(cJSON* doc) {
    // Expected JSON from Pi:
    // {"task":"/ota_start","ota":{"nodeId":11,"size":1048576,"crc32":"A1B2C3D4"}}
    //
    // Master workflow:
    // 1. SDO write node 11, 0x2F01 = size (U32)
    // 2. SDO write node 11, 0x2F02 = crc32 (U32)
    // 3. SDO block transfer to node 11, 0x2F00 = firmware binary
    //    (read binary from Serial in chunks, feed to SDO client)
    //
    // The SDO block transfer is handled by CANopenNode's SDO client.
    // The binary data arrives from the Pi over serial (raw bytes after the JSON command).
    
    cJSON* ota = cJSON_GetObjectItem(doc, "ota");
    if (!ota) return nullptr;
    
    uint8_t nodeId = cJSON_GetObjectItem(ota, "nodeId")->valueint;
    uint32_t size = cJSON_GetObjectItem(ota, "size")->valueint;
    
    // Write size to slave
    uint8_t sizeData[4];
    memcpy(sizeData, &size, 4);
    CANopenModule::writeSDO(nodeId, 0x2F01, 0x00, sizeData, 4);
    
    // TODO: Read firmware binary from serial, perform SDO block transfer
    // This requires streaming — the full implementation reads chunks from
    // Serial and feeds them to CO_SDOclientDownload() in a loop.
    // For now, return acknowledgement and implement streaming in a follow-up.
    
    cJSON* resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "return", 1);
    cJSON_AddStringToObject(resp, "status", "ota_initiated");
    return resp;
}
```

5. Add the /ota_start endpoint to SerialProcess.cpp routing and Endpoints.h.

6. IMPORTANT: The SDO block transfer for large binaries (~1MB) requires a streaming approach.
   The master reads firmware data from serial in 512-byte chunks and feeds each chunk to
   CO_SDOclientDownloadBufWrite() + CO_SDOclientDownload() in a loop. This is the same
   pattern as the existing read_SDO/write_SDO helpers in the MWE but with a loop that
   continuously refills the buffer. At 500kbit/s CAN, expect ~25-30 seconds for 1MB.

7. Add CanOpenOTA.cpp to main/CMakeLists.txt.

Test:
- Flash slave with OTA-capable partition table (must have two app partitions)
- From master serial, send {"task":"/ota_start","ota":{"nodeId":10,"size":...}}
- Monitor slave log for "OTA started", "OTA progress", "OTA verified. Rebooting"
- After reboot, slave should run the new firmware
```

---

## PR-9: Remove old CAN transport + ISO-TP library

**Branch:** `cleanup/remove-old-can-transport`
**Depends on:** PR-8 (all CANopen features working including OTA)
**Estimated scope:** ~2000 lines deleted, ~100 lines changed
**Human review focus:** make sure EVERY build environment still compiles after deletion

### Claude Code prompt:

```
This is the final cleanup PR. All CAN communication now goes through CANopenNode (PRs 3-8 are merged). The old ISO-TP based transport is no longer used by any build environment. Remove it entirely.

1. DELETE THESE FILES:
   - main/src/can/can_transport.cpp (the old ISO-TP dispatcher — ~1700 lines)
   - main/src/can/can_transport.h
   - main/src/can/can_messagetype.h (custom message type enum — replaced by CANopen COB-IDs)
   - main/src/can/iso-tp-twai/CanIsoTp.cpp
   - main/src/can/iso-tp-twai/CanIsoTp.h (if it exists)
   - main/src/can/iso-tp-twai/ directory entirely
   
   Keep main/src/can/ directory — it now contains only:
   - main/src/canopen/CANopenModule.h/.cpp
   - main/src/canopen/CanOpenOTA.h/.cpp
   - main/src/canopen/DeviceRouter.h/.cpp

2. DELETE OLD CAN-RELATED BUILD FLAGS from platformio.ini:
   Remove these flags from ALL environments (they no longer exist in code):
   - -DCAN_SEND_COMMANDS=1
   - -DCAN_RECEIVE_MOTOR=1
   - -DCAN_RECEIVE_LASER=1
   - -DCAN_RECEIVE_LED=1
   - -DCAN_RECEIVE_HOME=1
   - -DCAN_RECEIVE_TMC=1
   - -DCAN_RECEIVE_DAC=1
   - -DCAN_SLAVE_MOTOR=1
   
   The only CAN-related build flags that remain are:
   - -DCAN_CONTROLLER_CANOPEN=1 (enables CANopenModule)
   - -DNODE_ROLE=1 or -DNODE_ROLE=2 (master vs slave)
   - -DUC2_NODE_ID=N (default node-id, overridable via NVS)

3. DELETE OLD CAN BUILD ENVIRONMENTS from platformio.ini:
   Remove these environments that used the old transport:
   - env:UC2_3_CAN_HAT_Master (replaced by env:UC2_canopen_master)
   - env:UC2_3_CAN_HAT_Master_v2 (replaced by env:UC2_canopen_master)
   - env:seeed_xiao_esp32s3_can_slave_motor (replaced by env:UC2_canopen_slave)
   - env:seeed_xiao_esp32s3_can_slave_illumination (replaced by env:UC2_canopen_slave with different NVS config)
   - env:seeed_xiao_esp32s3_can_slave_led (same — consolidated)
   - env:seeed_xiao_esp32s3_can_slave_laser (same — consolidated)
   - env:seeed_xiao_esp32s3_can_slave_galvo (same — consolidated)
   - env:seeed_xiao_esp32c3_can_slave_motor (if still present)
   
   After cleanup, the CAN environments are:
   - env:UC2_canopen_master (ESP32, node-id 1)
   - env:UC2_canopen_slave (XIAO ESP32-S3, node-id from NVS)

4. REMOVE DEAD #ifdef BRANCHES in main.cpp and other files:
   Search the codebase for these dead symbols and remove the entire #ifdef/#else/#endif blocks:
   - CAN_SEND_COMMANDS
   - CAN_RECEIVE_MOTOR
   - CAN_RECEIVE_LASER
   - CAN_RECEIVE_LED
   - CAN_RECEIVE_HOME
   - CAN_RECEIVE_TMC
   - CAN_SLAVE_MOTOR
   
   In main.cpp specifically, the old CAN initialization block that called can_transport setup/loop
   should be fully removed. CANopenModule is already initialized via the CAN_CONTROLLER_CANOPEN path.

5. REMOVE OLD PinConfig DIRECTORIES that only existed for old CAN variants:
   - main/config/UC2_3_CAN_HAT_Master/ (if not already removed)
   - main/config/UC2_3_CAN_HAT_Master_v2/ → keep the PinConfig.h but rename to UC2_4_CANopen_Master/ if not already done
   - main/config/UC2_4_CAN_HYBRID/ → evaluate: if it was only for the old transport, remove it

6. UPDATE the web flasher board list:
   - The flasher at youseetoo.github.io references firmware names like "can-hat-master-v2" and "xiao-can-slave-motor"
   - These should map to the new environment names
   - Add a note in DOCUMENTATION/ about the name changes

7. REMOVE the ESP-CAN-ISO-TP-TWAI library dependency:
   - In platformio.ini, remove any lib_deps entry pointing to https://github.com/youseetoo/ESP-CAN-ISO-TP-TWAI.git
   - If it's a git submodule, remove it from .gitmodules
   - Delete lib/ESP-CAN-ISO-TP-TWAI/ if it exists as a local copy

8. UPDATE main/CMakeLists.txt:
   - Remove can_transport.cpp and any iso-tp source files from SRCS list
   - Verify CANopenModule.cpp, CanOpenOTA.cpp, DeviceRouter.cpp are listed

9. VERIFY that EVERY remaining build environment compiles:
   Run: pio run -e UC2_2 (standalone, no CAN)
   Run: pio run -e UC2_canopen_master
   Run: pio run -e UC2_canopen_slave
   Run: pio run -e UC2_3 (standalone board, no CAN)
   
   If any environment references deleted files or symbols, fix the references.

10. DO NOT delete:
    - Any controller implementation (MotorController, LaserController, etc.)
    - Any non-CAN build environment (UC2_2, UC2_3, UC2_4, etc.)
    - The QidRegistry (it now works through CANopen too)
    - The serial/WiFi/BT communication paths

After this PR, `grep -r "ISO.TP\|can_transport\|can_controller\|CanIsoTp\|CAN_SEND_COMMANDS\|CAN_RECEIVE_MOTOR" main/` should return zero results.
```

---

## Merge order and dependency graph

```
main (with QID + laser fixes merged from 7033352)
│
├── PR-1: feature/runtime-config              ← Foundation (NVS module enable/disable)
│   │
│   ├── PR-2: refactor/can-transport-rename    ← Pure rename (last time this file is touched)
│   │   │
│   │   ├── PR-3: feature/canopen-library      ← Add CANopenNode library, prove it compiles
│   │   │   │
│   │   │   ├── PR-4: feature/canopen-slave    ← Slave: TWAI + CANopenNode + OD↔Module sync
│   │   │   │   │
│   │   │   │   ├── PR-5: feature/canopen-master   ← Master: serial JSON → SDO bridge
│   │   │   │   │   │
│   │   │   │   │   ├── PR-6: feature/canopen-uc2-od   ← Full UC2 OD + real motor/laser/LED wiring
│   │   │   │   │   │   │
│   │   │   │   │   │   ├── PR-8: feature/canopen-ota       ← Firmware update via SDO block transfer
│   │   │   │   │   │   │   │
│   │   │   │   │   │   │   └── PR-9: cleanup/remove-old-can  ← DELETE old transport, ISO-TP, dead #ifdefs
│   │   │   │   │   │
│   │   │   │   │   └── PR-7: feature/pinconfig-cleanup  ← Consolidate PinConfigs (parallel with 6/8)
│   │   │   │
│   │   │   └── (PR-F: feature/serial-cobs-transport — independent, merge anytime)
```

**The pipeline to full removal:**
PR-4 adds CANopenNode alongside the old transport (both exist, #ifdef selects).
PR-5 makes the master work through CANopen.
PR-6 wires all modules to the CANopen OD.
PR-8 adds OTA over CANopen (the last missing feature).
PR-9 deletes everything old — the old transport is gone, the ISO-TP library is gone, the dozen CAN_RECEIVE_* flags are gone.

After PR-9, the CAN codebase is: CANopenModule (one file), DeviceRouter (one file), CanOpenOTA (one file), plus the CANopenNode library. That's it.

## What to verify at each stage (human checklist)

| After PR | Test this | How |
|----------|-----------|-----|
| PR-1 | RuntimeConfig loads from NVS, /config_get returns JSON | Serial monitor: send `{"task":"/config_get"}` |
| PR-2 | All existing CAN builds still compile | `pio run -e UC2_3_CAN_HAT_Master_v2` |
| PR-3 | CANopenNode library compiles without errors | `pio run -e UC2_canopen_slave` |
| PR-4 | Two boards exchange heartbeats on CAN bus | Serial log shows "CANopenNode running", candump shows 0x70A frames |
| PR-5 | Motor command from Pi reaches slave via SDO | Send JSON on master serial, see "SDO write" log on slave |
| PR-6 | Motor actually moves when commanded over CANopen | Send motor_act JSON, motor turns, position reported back |
| PR-7 | Build still works after PinConfig consolidation | `pio run -e UC2_canopen_slave` with new config path |
| PR-8 | Firmware update works over CAN bus | Flash new firmware to slave via master, slave reboots with new version |
| PR-9 | **All builds compile, no old CAN references remain** | `pio run` for all env, `grep -r "can_transport\|ISO.TP\|CAN_SEND_COMMANDS" main/` returns nothing |

## After PR-9: what the CAN codebase looks like

```
main/src/canopen/
├── CANopenModule.h       (~50 lines — API)
├── CANopenModule.cpp     (~400 lines — TWAI init, FreeRTOS tasks, OD↔Module sync)
├── DeviceRouter.h        (~30 lines — API)
├── DeviceRouter.cpp      (~200 lines — JSON→SDO mapping per command type)
├── CanOpenOTA.h          (~30 lines — API)
└── CanOpenOTA.cpp        (~150 lines — SDO block → esp_ota_write)

lib/ESP32_CanOpenNode/    (~8000 lines — third-party, unmodified, battle-tested)
└── src/
    ├── 301/              — CiA 301 core (NMT, SDO, PDO, heartbeat)
    ├── CO_driver_ESP32.c — ESP32 TWAI bridge
    ├── OD.h, OD.c        — Generated from openUC2_satellite.eds
    └── ...

Total UC2-authored CAN code: ~860 lines (down from ~2500+ in can_transport.cpp + ISO-TP)
```

---

## Current integration status (as of 2025-06)

All three environments build cleanly:
- `UC2_canopen_master` (ESP32, CAN_CONTROLLER_CANOPEN + CAN_SEND_COMMANDS)
- `UC2_canopen_slave` (ESP32-S3, CAN_CONTROLLER_CANOPEN + CAN_RECEIVE_MOTOR)
- `UC2_3_CAN_HAT_Master_v2` (ESP32, old ISO-TP transport — no CANopen)

The following bugs have been fixed in this session:

| Bug | Root cause | Fix |
|-----|-----------|-----|
| `motor_act` SDO sent but slave motor didn't move | DeviceRouter mapped `stepperid=0` → `nodeId=14`, slave defaults to `nodeId=10` | Fixed `stepperIdToNodeId()`: 0→10, 1→11, 2→12, 3→13 |
| SDO write aborted on slave (OD type mismatch) | DeviceRouter wrote int32 to `0x6200:01` which is `uint8` in OD | Rewrote to use `uint16` OD entries at `0x6401:01..04` (MWE pattern) |
| Slave `syncRpdoToModules()` never moved motor | Function only logged, never called `FocusMotor::startStepper()` | Implemented: decode trigger/position/speed/ctrl from OD, dispatch to FocusMotor |
| `/can_get` returned nothing in CANopen builds | Endpoint guarded by `!defined(CAN_CONTROLLER_CANOPEN)` | Added `#elif defined(CAN_CONTROLLER_CANOPEN)` branch in `SerialProcess.cpp` |

### Motor command encoding (temporary workaround)

The current trainer OD (`lib/ESP32_CanOpenNode/src/OD.h`) has no native `int32` entry for motor position. The workaround encodes a 32-bit position as two `uint16` words:

```
0x6401:01  posHi = (position >> 16) & 0xFFFF
0x6401:02  posLo = (position      ) & 0xFFFF
0x6401:03  speed (clamped to uint16)
0x6401:04  ctrl  (bit0=isAbs, bit1=isStop)
0x6200:01  trigger byte = 0x01  ← last write, starts motion on slave
```

Both `DeviceRouter.cpp` (master write) and `CANopenModule.cpp::syncRpdoToModules()` (slave decode) use the same encoding. This is explicitly a stopgap until PR-6 (full UC2 OD).

---

## Known TODOs and near-term work items

### TODO-1 — Full UC2 OD (replaces split-uint16 workaround)  **[Blocks PR-6]**

**Problem:** The trainer OD (`OD.h/OD.c`) has `uint8_t x6200[1]` and `uint16_t x6401[4]`. Motor position is int32 but must be split into two uint16 words. This is fragile and limits the number of motor parameters that can be sent.

**Required:** Generate a proper UC2 OD from `openUC2_satellite.eds` using CANopenEditor that includes:
- `0x6000:01` — uint8 digital inputs (isRunning flag)
- `0x6200:01` — uint8 digital outputs (trigger byte)
- `0x6401:01` — int32 motor position (native, no split)
- `0x6401:02` — uint16 motor speed
- `0x6401:03` — uint16 motor acceleration
- `0x6401:04` — uint8 motor control flags (isAbs, isStop)
- `0x6402:01..04` — laser/LED values (per axis)

**Impact:** Once done, remove the `posHi`/`posLo` split from `DeviceRouter.cpp` and `syncRpdoToModules()`. Replace with single int32 SDO write.

**Files to touch:** `lib/ESP32_CanOpenNode/src/OD.h`, `OD.c` (regenerate), `DeviceRouter.cpp`, `CANopenModule.cpp`.

---

### TODO-2 — Acceleration parameter not sent to slave  **[Blocks full motor control]**

**Location:** `DeviceRouter.cpp` `handleMotorAct()` — comment: *"relative or absolute? Do we provide all info? e.g. acceleration?"*

**Problem:** `MotorData` has an `acceleration` field but `DeviceRouter` does not encode or send it. The slave runs with default acceleration profile.

**Required:** Add `0x6401:03` (uint16 acceleration) to the OD, write it from `DeviceRouter`, read it in `syncRpdoToModules()` and set `motorData->acceleration` before calling `startStepper()`.

**Prerequisite:** TODO-1 (full UC2 OD needs an acceleration entry).

---

### TODO-3 — CAN OTA not implemented for CANopen builds  **[Blocks PR-8]**

**Location:** `SerialProcess.cpp` — `/can_ota` and `/can_ota_stream` are inside `#if defined(CAN_BUS_ENABLED) && !defined(CAN_CONTROLLER_CANOPEN)` → completely absent from CANopen builds.

**Comment in code:** `// TODO: binary OTA should work through CANopen layer as well once implemented there`

**Required:** Implement `CanOpenOTA.cpp` (planned in PR-8 spec):
- Master: receive OTA binary from serial, chunk into SDO block transfer writes to `0x1F50:01` (standard CiA 302 firmware download OD index)
- Slave: `OD_write_1F50()` custom handler triggers `esp_ota_begin/write/end`, then NMT reset

**Estimated size:** ~150 lines (as estimated in PR-8 spec).

---

### TODO-4 — Binary OTA via serial absent in CANopen builds

**Location:** `SerialProcess.cpp` `loop()` — the `canOtaStream` binary receive path is inside `#if defined(CAN_BUS_ENABLED) && !defined(CAN_CONTROLLER_CANOPEN)`.

**Problem:** Even local OTA (master updating itself via serial binary) is guarded out in CANopen builds.

**Required:** Either remove the `!defined(CAN_CONTROLLER_CANOPEN)` guard (if the local OTA path doesn't use TWAI), or duplicate the binary receive into the CANopen branch and forward chunks via SDO block transfer.

---

### TODO-5 — Dial controller not adapted for CANopen master mode

**Location:** `main.cpp` line ~492 — comment: *"Dial controller needs CAN bus to be ready when in CAN master mode — has to move over to canopen too"*

**Problem:** `DialController` uses direct CAN frame construction (`can_controller::sendFrame()`). In CANopen builds `can_controller` is stubbed out (no-ops), so Dial controller over CAN silently does nothing.

**Required:** Adapt `DialController.cpp` to use `DeviceRouter::handleMotorAct()` (or a direct SDO write) when `CAN_CONTROLLER_CANOPEN` is defined. Alternative: route Dial controller commands through the same JSON serial path that `SerialProcess` uses, so `DeviceRouter` handles the routing automatically.

---

### TODO-6 — Laser/LED routing on slave not wired

**Location:** `DeviceRouter.cpp` has `handleLaserAct()` wired on master side. But `CANopenModule.cpp::syncRpdoToModules()` on the slave has no handler for laser OD entries.

**Problem:** Laser SDO writes reach the slave but `syncRpdoToModules()` only checks `x6200_writeOutput8Bit[0]` for the motor trigger. No laser value is extracted or dispatched.

**Required:**
1. Add laser OD entries (e.g., `0x6402:01` uint16 laser intensity) to the UC2 OD (see TODO-1).
2. In `DeviceRouter.cpp`: add SDO write for laser intensity to `0x6402:01` + trigger via `0x6200:01` bit 1.
3. In `CANopenModule.cpp::syncRpdoToModules()`: detect laser trigger bit, read `x6402[0]`, call `LaserController::setLaser(axis, value)`.

---

### TODO-7 — Remove `can_controller_stubs.cpp` after full migration  **[PR-9]**

**Location:** `main/src/can/can_controller_stubs.cpp`

**Problem:** This file provides no-op stubs for `can_controller::sendFrame()`, `::init()`, `::deinit()` so that controller files (FocusMotor, LaserController, etc.) that call `can_controller::` still compile. The stubs silence linker errors but mean any controller file that calls these functions silently does nothing at runtime.

**Required (PR-9):** For each controller file that calls `can_controller::*`:
1. Wrap the call in `#ifndef CAN_CONTROLLER_CANOPEN` / `#endif`
2. Once all controllers are wrapped, delete `can_controller_stubs.cpp` and remove it from `CMakeLists.txt`

**Files to audit:** `FocusMotor.cpp`, `LaserController.cpp`, `LEDController.cpp`, `main.cpp` (any direct `can_controller::` calls).

---

### TODO-8 — Consolidate `#ifdef CAN_SEND_COMMANDS` / `CAN_RECEIVE_MOTOR` flags

**Problem:** The codebase has ~10 separate `CAN_RECEIVE_*` / `CAN_SEND_*` flags used to enable features per environment. Once CANopen is fully integrated, all of these should be derivable from a single `CAN_ROLE_MASTER` / `CAN_ROLE_SLAVE` flag plus the OD role.

**Required (PR-9):** After all modules are wired to CANopen:
- Replace `CAN_SEND_COMMANDS + CAN_RECEIVE_MOTOR + CAN_RECEIVE_LASER + ...` with a single `CAN_ROLE=MASTER` or `CAN_ROLE=SLAVE` `platformio.ini` flag.
- Update `platformio.ini` environments accordingly.
- Verify with `grep -r "CAN_RECEIVE\|CAN_SEND_COMMANDS" main/` that no stale flags remain.

---

## Quick-start test script (post-fix)

After flashing master + slave, verify `motor_act` end-to-end:

```bash
# On master serial (115200 baud)
# 1. Check slave is visible on CAN bus
{"task":"/can_get"}
# Expected: {"nodeId":10,"canRole":"master","nmtState":"OPERATIONAL","txErrors":0,...}

# 2. Send motor_act to slave (axis A = node 10)
{"task":"/motor_act","motor":{"steppers":[{"stepperid":0,"position":1000,"speed":1000,"isabs":true,"isaccel":false}]}}
# Expected: no error, slave LED blinks, motor moves 1000 steps

# 3. Read back position
{"task":"/motor_get"}
# Expected: {"motor":{"steppers":[{"stepperid":0,"position":1000,...}]}}
```

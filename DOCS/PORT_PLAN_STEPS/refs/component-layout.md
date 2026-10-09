# Ref: Component layout, app skeleton, source gating

Index: `../PORT_PLAN_STEPS.md`. Target layout with exact source moves.

## 1. Target layout

```
uc2-esp32/
├─ CMakeLists.txt              # top-level: project(uc2) + components/ wiring (exists, 3 lines)
├─ sdkconfig.defaults          # distilled (T1.1)
├─ sdkconfig.defaults.esp32    # target overlay
├─ sdkconfig.defaults.esp32s3  # target overlay
├─ components/
│  ├─ uc2_hal/                 # NEW (T1.3): Arduino-compat shim, see hal-shim.md
│  ├─ uc2_app/                 # NEW (T1.4): main/src glue
│  │    └─ from: main/main.cpp, main/JsonKeys.h, main/cJsonTool.h,
│  │             main/PinConfigDefault.h  (C++ until T4.6, then C)
│  ├─ uc2_core/                # NEW
│  │    └─ from: main/src/state, main/src/config, main/src/qid,
│  │             main/src/message, main/src/buzzer   (+ Kconfig: env-app-mapping.md §3)
│  ├─ uc2_serial/              # NEW
│  │    └─ from: main/src/serial (SerialProcess, SerialProtocol, Endpoints)
│  ├─ uc2_io/                  # NEW
│  │    └─ from: main/src/digitalin, main/src/digitalout, main/src/analogin,
│  │             main/src/analogout, main/src/encoder (InterruptController)
│  ├─ uc2_led/                 # NEW
│  │    └─ from: main/src/led (LedController, **Ws2812Rmt — first-party, keep bare-metal
│  │             RMT + chip-specific structs**, channel-allocation comments),
│  │             main/src/signal (SignalController)
│  ├─ uc2_motor/               # NEW
│  │    └─ from: main/src/motor (FocusMotor, MotorJsonParser, MotorGamePad, FAccelStep,
│  │             AccelStep, StageScan, FocusScan, MotorTypes), main/src/home (HomeMotor),
│  │             main/src/pid, main/src/axis (clean-slate closed-loop module)
│  ├─ uc2_scanner/             # NEW
│  │    └─ from: main/src/scanner (GalvoController, HighSpeedScannerCore, DAC_MCP4822,
│  │             ScannerController)
│  ├─ uc2_periph/              # NEW
│  │    └─ from: main/src/i2c (i2c_master, i2c_slave_dial, I2cBridge, tca_controller),
│  │             main/src/tmc, main/src/dac, main/src/fan, main/src/tmp102,
│  │             main/src/thermal, main/src/heat (DS18b20)
│  ├─ uc2_canopen/             # NEW
│  │    └─ from: main/src/canopen (CANopenModule, CanOpenOTA, CanOpenOTAStreaming,
│  │             OtaBinaryReceive, DeviceRouter, RoutingTable, SdoEmit.h,
│  │             UC2_OD_Indices.h), main/src/gpio_can, main/src/ptz
│  ├─ uc2_joystick/            # NEW
│  │    └─ from: main/src/joystick (JoystickUsbHost, JoystickRouter)
│  ├─ uc2_bt/                  # NEW (T3.3)
│  │    └─ from: main/src/bt (BtController, HidController, HidGamePad.h,
│  │             PS4TrackpadParser.h, esp_hid_gap.c) + lib/PSController (C++ leaf until T4.5)
│  ├─ uc2_dial/                # NEW (D4)
│  │    └─ from: main/src/dial (DialController), main/src/image (ImageController)
│  ├─ canopen_node/            # MOVE as-is (C): lib/ESP32_CanOpenNode/src (+ CANopen.h)
│  │                           #   — already uses IDF TWAI driver + TWAI ISR directly
│  ├─ uc2_od/                  # MOVE as-is (C, generated): lib/uc2_od
│  │                           #   — codegen (tools/canopen/regenerate_all.py) untouched
│  ├─ cjson/                   # MOVE as-is (C): main/cJSON (git submodule → repoint .gitmodules)
│  ├─ esp_usb_host_hid/        # MOVE as-is (C, IDF usb_host): lib/esp_usb_host_hid
│  ├─ dac_module/              # MOVE as-is: lib/DAC_Module (C-ify in T5.1 if C++)
│  ├─ fastaccelstepper/        # VENDOR (C++ leaf, D2): gin66/FastAccelStepper ^0.31.5 →
│  │                           #   vendor source + fas_backend.h (C) in T5.2
│  ├─ tmc_stepper/             # VENDOR (C++ leaf, D2): teemuatlut/TMCStepper ^0.7.3
│  ├─ accelstepper/            # VENDOR→C (T5.1): youseetoo/AccelStepper URL fork
│  ├─ encoder/                 # VENDOR→C (T5.1): madhephaestus/ESP32Encoder 0.12.0 (PCNT;
│  │                           #   pin 0.12.0 — master moved to IDF-5 PCNT, don't upgrade)
│  ├─ tca95xx/                 # VENDOR→C (T5.1): RobTillaart TCA9555 ^0.4.4 + first-party
│  │                           #   wrappers from main/src/i2c (tca_controller)
│  ├─ onewire_ds18b20/         # VENDOR→C (T5.1): paulstoffregen/OneWire ^2.3.7 +
│  │                           #   milesburton/DallasTemperature ^3.11.0
│  ├─ uc2_pixel_buf/           # NEW (T5.3): plain struct pixel buffer replacing Adafruit
│  │                           #   NeoPixel (buffer-only usage today)
│  └─ m5gfx/                   # VENDOR (D4): M5Unified + M5GFX + M5Dial (C++ leaf)
├─ boards/                     # PinConfig per board (replaces main/config/* + copy_PinConfig.py)
│  ├─ uc2_2/PinConfig.h        # ← main/config/UC2_2/
│  ├─ uc2_3/PinConfig.h        # ← main/config/UC2_3/
│  ├─ uc2_4/PinConfig.h        # ← main/config/UC2_4/
│  ├─ uc2_wemos/PinConfig.h    # ← main/config/UC2_WEMOS/
│  ├─ canopen_master/PinConfig.h      # ← main/config/UC2_canopen_master/
│  ├─ canopen_standalone_v4/PinConfig.h   # ← main/config/UC2_canopen_standalone_v4/
│  ├─ canopen_slave_{laser,led,gpio,galvo,motor,accelmotor}/PinConfig.h
│  │    # ← main/config/UC2_canopen_slave_*/  (PIO env name == config dir name)
│  ├─ canopen_bridge_ptz/PinConfig.h    # ← main/config/UC2_canopen_bridge_ptz/
│  ├─ canopen_bridge_ps4_usbhost/PinConfig.h  # ← main/config/UC2_canopen_bridge_ps4_usbhost/
│  ├─ xiao_odmr/PinConfig.h    # ← main/config/seeed_xiao_esp32s3/
│  ├─ xiao_ledservo/PinConfig.h   # ← main/config/seeed_xiao_esp32s3_ledservo/
│  ├─ xiao_ledring/PinConfig.h    # ← main/config/seeed_xiao_esp32s3_ledring/
│  ├─ waveshare_ledarray/PinConfig.h   # ← main/config/waveshare_esp32s3_ledarray/
│  ├─ m5stack_dial/PinConfig.h  # ← main/config/m5stack_dial/  (D4)
│  ├─ waveshare_7inch/PinConfig.h      # ← main/config/waveshare_esp32s3_7inchScreen/ (D4)
│  └─ (stale dirs — UC2_1, HoLiSheet, XYZ_*, X_MOTOR_64LED_PIN, UC2_CassetteRecorder/
│     Insert/OMNISCOPE, UC2e, *_can_slave_*, waveshare_hub75_*, UC2_3_CAN_HAT*, UC2_4_CAN*,
│     UC2_XYZRotator, seeed_xiao_esp32c3_* — deleted in T2.11, D3-confirmed)
├─ apps/                       # one IDF app per env (env-app-mapping.md); each:
│  │                           # CMakeLists.txt + main.c + sdkconfig.defaults; PinConfig
│  │                           # via include dir boards/<app>
│  └─ (17 CI + canopen_bridge_ptz + optional m5stack_dial/waveshare_7inch/uc2_dev)
├─ partitions/                 # custom_partition*.csv moved from root (T2.11)
├─ test/                       # host tests: native/ (existing), unity/ (T0.5, 168-suite)
├─ tools/                      # canopen codegen unchanged
├─ legacy/                     # platformio.ini, copy_PinConfig.py, unused libs (T2.11/T6.6)
└─ DOCS/baselines.md           # T0.3
```

`main/testFeedbackStepper/` (standalone mini-PIO rig) left untouched (convert or keep on
PIO — noted in T6.4). `can_sniffer/`, `fas_standalone_test/` likewise.

## 2. App skeleton (per app)

```cmake
# apps/<app>/CMakeLists.txt
idf_component_register(SRCS "main.c" INCLUDE_DIRS "."
                       REQUIRES uc2_app uc2_core uc2_serial uc2_io uc2_led uc2_motor
                                uc2_scanner uc2_periph uc2_canopen uc2_joystick uc2_bt
                                uc2_dial canopen_node uc2_od cjson esp_usb_host_hid
                                dac_module fastaccelstepper tmc_stepper accelstepper
                                encoder tca95xx onewire_ds18b20 uc2_pixel_buf)
# PinConfig.h for this board:
target_include_directories(<app> PRIVATE ${CMAKE_SOURCE_DIR}/boards/<app>)
```

```c
// apps/<app>/main.c
#include "uc2_app.h"
void app_main(void) { uc2_app_start(); }
```

`uc2_app_start()` wraps today's `setup()`/`loop()` semantics from `main.cpp` (loop →
periodic FreeRTOS task, as `main.cpp` already does its tasking). Components not needed by
an app are **compiled empty** via the source-gating pattern below, so one shared REQUIRES
list is safe and app CMakeLists stay trivial.

## 3. Per-component source gating (replaces `main/CMakeLists.txt` feature logic)

Each `uc2_*/CMakeLists.txt` gates its sources on the Kconfig symbols, mirroring
`main/CMakeLists.txt` exactly, e.g.:

```cmake
# components/uc2_motor/CMakeLists.txt (sketch)
if(CONFIG_UC2_MOTOR_CONTROLLER)
  set(srcs FocusMotor.cpp MotorJsonParser.cpp MotorGamePad.cpp)
  if(CONFIG_UC2_USE_FASTACCEL)  list(APPEND srcs FAccelStep.cpp)   endif()
  if(CONFIG_UC2_USE_ACCELSTEP)  list(APPEND srcs AccelStep.cpp)    endif()
  if(CONFIG_UC2_STAGE_SCAN)     list(APPEND srcs StageScan.cpp FocusScan.cpp) endif()
  if(CONFIG_UC2_HOME_MOTOR)     list(APPEND srcs ${CMAKE_SOURCE_DIR}/../main/src/home/HomeMotor.cpp) endif()
  idf_component_register(SRCS ${srcs} INCLUDE_DIRS "."
                         REQUIRES uc2_hal driver cjson fastaccelstepper accelstepper tmc_stepper encoder)
else()
  idf_component_register(SRCS "" INCLUDE_DIRS ".")
endif()
```

Rules: `uc2_canopen`/`uc2_app` are **always** registered (DeviceRouter/RoutingTable and the
core chain compile for every app, same as today); `canopen_node`, `uc2_od`, `cjson`,
`esp_usb_host_hid`, `dac_module`, vendor components register unconditionally (small /
already C).

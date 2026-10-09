# Ref: Env → IDF-app mapping (exact)

Index: `../PORT_PLAN_STEPS.md`. 17 CI envs (build-and-release.yaml matrix) +
`UC2_canopen_bridge_ptz` (not in that CI, but in build-frame) + `esp32dev` (dev-only) + the
two M5GFX envs (D4). Feature sets transcribed 1:1 from `platformio.ini` `build_flags`
(spot-checked 2026-07-24) and become **Kconfig symbols** (§3 here) set in each app's
`apps/<app>/sdkconfig.defaults`. `NODE_ROLE` 1 = master/originator, 2 = slave.
"Libs" = vendor deps that app needs (→ component REQUIRES).

## 1. ESP32 (classic) apps

| App | PIO env (source) | Partitions | Feature Kconfig (all `=y` unless noted) | Libs/components | Notes |
|-----|------------------|-----------|------------------------------------------|-----------------|-------|
| `apps/uc2_2` | `UC2_2` | `custom_partition.csv` | UC2_BLUETOOTH, UC2_BTHID, UC2_MOTOR_CONTROLLER, UC2_LASER_CONTROLLER, UC2_LED_CONTROLLER, UC2_DIGITAL_IN_CONTROLLER, UC2_HOME_MOTOR, UC2_MESSAGE_CONTROLLER, UC2_DAC_CONTROLLER, UC2_HEAT_CONTROLLER, UC2_DS18B20_CONTROLLER, UC2_USE_FASTACCEL, UC2_STAGE_SCAN; UC2_CORE_DEBUG_LEVEL=0 (release) | uc2_bt, uc2_motor, uc2_led, uc2_periph, fastaccelstepper, tmc_stepper, tca95xx, onewire_ds18b20, uc2_pixel_buf, encoder | BT board → Phase 3 |
| `apps/uc2_3` | `UC2_3` | `custom_partition.csv` | UC2_2 set **+ UC2_USE_TCA9535** | same + tca95xx active | BT board → Phase 3 |
| `apps/uc2_4` | `UC2_4` | `custom_partition.csv` | UC2_3 set **minus UC2_DAC_CONTROLLER** | same, no dac_module | **FAS owns RMT 0..3; WS2812 on ch 4** — preserve allocation (comment in `boards/uc2_4/PinConfig.h` + `uc2_led` init). BT board → Phase 3 |
| `apps/uc2_wemos` | `UC2_WEMOS` | `custom_partition.csv` | = UC2_3 feature set (env extends `UC2_3_default`) | same as uc2_3 | PinConfig from `main/config/UC2_WEMOS/` (its own dir, not UC2_3's — the old `-DCONFIG=UC2_3` cmake arg was only used by the retired copy script). BT board → Phase 3 |
| `apps/canopen_master` | `UC2_canopen_master_release` | `custom_partition.csv` | UC2_NODE_ROLE=1, UC2_LASER_CONTROLLER, UC2_DIGITAL_IN_CONTROLLER, UC2_MESSAGE_CONTROLLER, UC2_CAN_BUS_ENABLED, UC2_CAN_CONTROLLER_CANOPEN, UC2_DIAL_CONTROLLER, UC2_MOTOR_CONTROLLER, UC2_HOME_MOTOR, UC2_BTHID, UC2_TMC_CONTROLLER, UC2_LED_CONTROLLER, UC2_BLUETOOTH, UC2_STAGE_SCAN, UC2_GALVO_CONTROLLER, UC2_DAC_CONTROLLER, UC2_TMP102_CONTROLLER, UC2_FAN_CONTROLLER, UC2_CANOPEN_OTA_NO_RECEIVER, UC2_FORCE_BT_CLASSIC_ONLY | all uc2_*, tmc_stepper, encoder, onewire, uc2_dial | **RAM-tight**; Phase 3. BT classic-only. Stepping backend check in T3.5 (no FAS/ACCELSTEP flag — see note below). |
| `apps/canopen_standalone_v4` | `UC2_canopen_standalone_v4_release` | `custom_partition.csv` | UC2_NODE_ROLE=1, UC2_USE_TCA9535, UC2_LASER_CONTROLLER, UC2_DIGITAL_IN_CONTROLLER, UC2_MESSAGE_CONTROLLER, UC2_CAN_BUS_ENABLED, UC2_CAN_CONTROLLER_CANOPEN, UC2_MOTOR_CONTROLLER, UC2_HOME_MOTOR, UC2_USE_FASTACCEL, UC2_LED_CONTROLLER, UC2_BTHID, UC2_BLUETOOTH, UC2_STAGE_SCAN, UC2_GALVO_CONTROLLER, UC2_DAC_CONTROLLER, UC2_CANOPEN_OTA_NO_RECEIVER, UC2_CANOPEN_OTA_NO_SENDER, UC2_FORCE_BT_CLASSIC_ONLY | all uc2_*, fastaccelstepper, tca95xx, onewire | BT board → Phase 3 |
| `apps/uc2_dev` *(optional, D3)* | `esp32dev` | `custom_partition.csv` | none (bare) | uc2_app only | Dev scratch; trivial app or drop |

## 2. ESP32-S3 apps

All S3 apps share: `ESP32S3_MODEL_XIAO=y` (kept for parity — gates pin/board behavior in
code), `UC2_CORE_DEBUG_LEVEL` per env (several S3 envs ship at 5 for parity; **do not
silently lower**). S3 USB apps additionally need USB-Serial-JTAG console (equivalent of
`ARDUINO_USB_CDC_ON_BOOT=1`): `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` in
`apps/<app>/sdkconfig.defaults`.

| App | PIO env (source) | Partitions | Feature Kconfig (added on top of shared) | Libs/components | Notes |
|-----|------------------|-----------|------------------------------------------|-----------------|-------|
| `apps/canopen_slave_laser` | `UC2_canopen_slave_laser_release` | `custom_partition_esp32s3.csv` | UC2_NODE_ROLE=2, UC2_MESSAGE_CONTROLLER, UC2_DIGITAL_IN_CONTROLLER, UC2_CAN_BUS_ENABLED, UC2_CAN_CONTROLLER_CANOPEN, UC2_LASER_CONTROLLER; UC2_CORE_DEBUG_LEVEL=0 | uc2_canopen, uc2_core, uc2_serial, canopen_node, uc2_od, cjson | **App #1 (Phase 1)** |
| `apps/canopen_slave_led` | `UC2_canopen_slave_led_release` | same | base + UC2_LED_CONTROLLER, UC2_LASER_CONTROLLER, UC2_THERMAL_CONTROLLER; DEBUG=0 | + uc2_led (Ws2812Rmt S3 path), thermal | Phase 2 (T2.4) |
| `apps/canopen_slave_gpio` | `UC2_canopen_slave_gpio_release` | same | base + UC2_DIGITAL_OUT_CONTROLLER, UC2_GPIO_CAN_SLAVE_CONTROLLER; DEBUG=0 | + gpio_can, I2cBridge (I2C_BRIDGE_CONTROLLER is PinConfig-gated) | Phase 2 (T2.3) |
| `apps/canopen_slave_galvo` | `UC2_canopen_slave_galvo_release` | same | base + UC2_GALVO_CONTROLLER; DEBUG=0 | + uc2_scanner (MCP4822 SPI), SPI driver | Phase 2 (T2.5) |
| `apps/canopen_slave_motor` | `UC2_canopen_slave_motor_release` | same | base + UC2_MOTOR_CONTROLLER, UC2_HOME_MOTOR, UC2_USE_FASTACCEL, UC2_TMC_CONTROLLER, UC2_ENCODER_CONTROLLER, UC2_LINEAR_ENCODER_CONTROLLER, UC2_NODE_ROLE=2; DEBUG=5 (parity) | + uc2_motor, fastaccelstepper (leaf, D2), tmc_stepper, encoder | Phase 2 (T2.7). **build-frame also builds 4 axis variants** via `-DCAN_AXIS_ID=CAN_ID_MOT_{X,Y,Z,A}` → Kconfig `UC2_CAN_AXIS_ID` string, 5 builds ("" X Y Z A). FAS RMT-ownership check mandatory. |
| `apps/canopen_slave_accelmotor` | `UC2_canopen_slave_accelmotor_release` | same | base + UC2_MOTOR_CONTROLLER, UC2_HOME_MOTOR, UC2_USE_ACCELSTEP, UC2_TMC_CONTROLLER, UC2_ENCODER_CONTROLLER, UC2_LINEAR_ENCODER_CONTROLLER | + uc2_motor, accelstepper, tmc_stepper, encoder | Phase 2 (T2.6) |
| `apps/canopen_bridge_ptz` | `UC2_canopen_bridge_ptz_release` | same | base + UC2_PTZ_KEYBOARD_CONTROLLER | + ptz (UART1 RS-485: RX GPIO44, TX GPIO43) | In build-frame, not in build-and-release. Phase 2 (T2.8) |
| `apps/canopen_bridge_ps4_usbhost` | `UC2_canopen_bridge_ps4_usbhost_release` | same | base + UC2_JOYSTICK_USBHOST_PROVIDER | + esp_usb_host_hid, joystick; USB OTG GPIO19/20 fixed | Phase 2 (T2.10), **no BT** |
| `apps/xiao_odmr` | `seeed_xiao_esp32s3` | `custom_partition_esp32s3.csv` | UC2_MOTOR_CONTROLLER, UC2_LASER_CONTROLLER, UC2_LED_CONTROLLER, UC2_DIGITAL_IN_CONTROLLER, UC2_I2C_SLAVE_MOTOR, UC2_MESSAGE_CONTROLLER, UC2_USE_ACCELSTEP, UC2_LINEAR_ENCODER_CONTROLLER, UC2_HEAT_CONTROLLER, UC2_HOME_MOTOR, UC2_STAGE_SCAN; DEBUG=5 (parity); USB console | + accelstepper, encoder, onewire, uc2_pixel_buf, **IS31FL3741** (note below) | Phase 2 (T2.9) |
| `apps/xiao_ledservo` | `seeed_xiao_esp32s3_ledservo` | `custom_partition_esp32s3.csv` | UC2_LASER_CONTROLLER, UC2_LED_CONTROLLER, UC2_DIGITAL_IN_CONTROLLER, UC2_MESSAGE_CONTROLLER; DEBUG=5; USB console | + uc2_pixel_buf, tmc_stepper, uc2_led | Phase 2 (T2.1) |
| `apps/xiao_ledring` | `seeed_xiao_esp32s3_ledring` | `custom_partition_esp32s3.csv` | UC2_LASER_CONTROLLER, UC2_DIGITAL_IN_CONTROLLER, UC2_I2C_SLAVE_LASER, UC2_MESSAGE_CONTROLLER; DEBUG=5; USB console | + tmc_stepper | Phase 2 (T2.1) |
| `apps/waveshare_ledarray` | `waveshare_esp32s3_ledarray` | `custom_partition_esp32s3_waveshare.csv` | UC2_LED_CONTROLLER, UC2_MESSAGE_CONTROLLER; USB console | + uc2_led (WS2812 **RMT ch 0**, no motors → no FAS contention), uc2_pixel_buf | Phase 2 (T2.2) |
| `apps/m5stack_dial` *(D4)* | `m5stack_dial` | (esp32s3 default) | UC2_M5DIAL, UC2_DIAL_CONTROLLER, UC2_NODE_ROLE=2, UC2_CAN_BUS_ENABLED, UC2_CAN_CONTROLLER_CANOPEN, UC2_MESSAGE_CONTROLLER | + m5gfx leaf (M5Unified/M5GFX/M5Dial), encoder, uc2_canopen, canopen_node, uc2_od, cjson | Gated by D4; board is `m5stack-stamps3` |
| `apps/waveshare_7inch` *(D4)* | `waveshare_esp32s3_7inchScreen` | `custom_partitions_8MB_tinyuf2.csv` (8 MB flash) | UC2_MESSAGE_CONTROLLER, UC2_NODE_ROLE=2, UC2_MOTOR_CONTROLLER, UC2_USE_FASTACCEL, UC2_CAN_BUS_ENABLED, UC2_CAN_CONTROLLER_CANOPEN; USB console; `LV_CONF_INCLUDE_SIMPLE` if M5GFX kept; **qio_opi PSRAM + 80 MHz flash** in sdkconfig | + m5gfx leaf, fastaccelstepper, encoder, onewire | Gated by D4; board is `esp32s3box` |

> **Note (IS31FL3741):** `seeed_xiao_esp32s3` lists `adafruit/Adafruit IS31FL3741 Library`
> in `lib_deps` but the high-level plan does not flag it as used in `main/`. **Action in
> T2.9 step 1:** grep `IS31FL3741` in `main/`; if 0 references, drop the dep; if
> referenced, add a tiny C leaf component. Do not assume.

> **Note (UC2_canopen_master and FastAccelStepper):** the master env does **not** set
> `UC2_USE_FASTACCEL`/`UC2_USE_ACCELSTEP`; confirm which stepping backend `FocusMotor`
> compiles under (check `main/CMakeLists.txt` `MOTOR_CONTROLLER` block + `FAccelStep`/
> `AccelStep` gates) in **T3.5 step 1** and mirror the exact source set.

## 3. Per-app sdkconfig/Kconfig feature set — mechanics

- **Kconfig symbols** (`components/uc2_core/Kconfig`, all `bool` unless noted):
  `UC2_NODE_ROLE` (int, default 1), `UC2_CORE_DEBUG_LEVEL` (int 0..5, default 0),
  `UC2_CAN_AXIS_ID` (string, default `""`), and the controller flags exactly as named in
  §1–2 (`UC2_BLUETOOTH`, `UC2_BTHID`, `UC2_MOTOR_CONTROLLER`, `UC2_LASER_CONTROLLER`,
  `UC2_LED_CONTROLLER`, `UC2_DIGITAL_IN_CONTROLLER`, `UC2_DIGITAL_OUT_CONTROLLER`,
  `UC2_DAC_CONTROLLER`, `UC2_HEAT_CONTROLLER`, `UC2_DS18B20_CONTROLLER`,
  `UC2_USE_TCA9535`, `UC2_HOME_MOTOR`, `UC2_MESSAGE_CONTROLLER`, `UC2_USE_FASTACCEL`,
  `UC2_USE_ACCELSTEP`, `UC2_STAGE_SCAN`, `UC2_CAN_BUS_ENABLED`, `UC2_CAN_CONTROLLER_CANOPEN`,
  `UC2_GPIO_CAN_SLAVE_CONTROLLER`, `UC2_PTZ_KEYBOARD_CONTROLLER`, `UC2_GALVO_CONTROLLER`,
  `UC2_DIAL_CONTROLLER`, `UC2_M5DIAL`, `UC2_TMC_CONTROLLER`, `UC2_ENCODER_CONTROLLER`,
  `UC2_LINEAR_ENCODER_CONTROLLER`, `UC2_I2C_SLAVE_MOTOR`, `UC2_I2C_SLAVE_LASER`,
  `UC2_I2C_MASTER`, `UC2_TMP102_CONTROLLER`, `UC2_FAN_CONTROLLER`, `UC2_THERMAL_CONTROLLER`,
  `UC2_JOYSTICK_USBHOST_PROVIDER`, `UC2_CANOPEN_OTA_NO_RECEIVER`,
  `UC2_CANOPEN_OTA_NO_SENDER`, `UC2_FORCE_BT_CLASSIC_ONLY`).
  Source files gate on these **exactly where `main/CMakeLists.txt` gates them today**
  (the 207-line file is the authoritative source→feature map; see
  `component-layout.md` §3).
- **`apps/<app>/sdkconfig.defaults`** sets the feature symbols from the §1–2 tables +
  board specifics (partitions table filename, USB console, PSRAM, BT stack).
- **Root `sdkconfig.defaults`** (T1.1, distilled, ≤ ~250 lines): log levels default, main
  task stack (match PIO's app-main stack), FreeRTOS tick, app version string from git tag,
  ccache off, `CONFIG_COMPILER_OPTIMIZATION_DEFAULT`, per-target: TWAI (keep always on —
  tiny; canopen_node needs it), SPI/I2C/LEDC/PCNT/ADC always on, BT symbols **only** in
  BT apps' defaults.
- **`sdkconfig.defaults.esp32` / `sdkconfig.defaults.esp32s3`**: target overlays IDF picks
  automatically (flash 40 MHz default, partition table name per target, USB console on for
  S3 only if the app also sets it).

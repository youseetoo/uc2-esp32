# UC2 CANopen Parameter Reference

**AUTO-GENERATED** from `uc2_canopen_registry.yaml`. Do not edit manually.

Regenerate with `python tools/regenerate_all.py`.

## Overview

- Vendor: openUC2 GmbH
- Product: UC2 ESP32-S3 Satellite Node
- Vendor ID: `0x000055C2`
- Default baud rate: 500 kbit/s

## Node-ID assignments

| Role | Node-ID |
|------|---------|
| master | 1 |
| motor_a | 14 |
| motor_x | 11 |
| motor_y | 12 |
| motor_z | 13 |
| led | 20 |
| laser | 21 |
| joystick | 22 |
| galvo | 30 |
| galvo_2 | 31 |
| encoder | 40 |
| pid | 50 |

## OD index map

### Motor — base `0x2000`

Stepper motor control with TMC2209 driver, up to 4 axes per node

*Maps to JSON endpoint(s):* `/motor_act, /motor_get, /motor_set`

*C++ class:* `FocusMotor`

| Index | Sub | Name | Type | Access | PDO | Description |
|-------|-----|------|------|--------|-----|-------------|
| `0x2000` | 1..4 | `motor_target_position` | I32 | rw | rpdo1 | Target position in steps. Triggers a move when command word is set. |
| `0x2001` | 1..4 | `motor_actual_position` | I32 | ro | tpdo1 | Current encoder/step counter position. Updated by syncModulesToTpdo every cycle. |
| `0x2002` | 1..4 | `motor_speed` | U32 | rw | rpdo1 | Maximum speed for the next move. Persists across moves. |
| `0x2003` | 0 | `motor_command_word` | U8 | rw | rpdo1 | Per-axis trigger bitmask. Slave reads, executes, clears. |
| `0x2004` | 1..4 | `motor_status_word` | U8 | ro | tpdo1 | Per-axis status. TPDO-broadcast on change. |
| `0x2005` | 1..4 | `motor_enable` | U8 | rw | rpdo2 | 1=driver enabled, 0=disabled (released). Maps to /motor_set isen. |
| `0x2006` | 1..4 | `motor_acceleration` | U32 | rw | SDO only | Acceleration profile. SDO-only — set once, used for all moves. |
| `0x2007` | 1..4 | `motor_is_absolute` | U8 | rw | rpdo2 | 0=relative move, 1=absolute move. Per-axis. |
| `0x2008` | 1..4 | `motor_min_position` | I32 | rw | SDO only | Soft limit minimum. Slave clips target to this range. |
| `0x2009` | 1..4 | `motor_max_position` | I32 | rw | SDO only | Soft limit maximum. |
| `0x200A` | 1..4 | `motor_jerk` | U32 | rw | SDO only | S-curve jerk (0=trapezoidal). Optional; 0 disables S-curve. |

### Homing — base `0x2010`

Sensorless or endstop-based homing

*Maps to JSON endpoint(s):* `/home_act`

*C++ class:* `HomeMotor`

| Index | Sub | Name | Type | Access | PDO | Description |
|-------|-----|------|------|--------|-----|-------------|
| `0x2010` | 1..4 | `homing_command` | U8 | rw | rpdo2 | 0=idle, 1=start homing on this axis |
| `0x2011` | 1..4 | `homing_speed` | U32 | rw | SDO only |  |
| `0x2012` | 1..4 | `homing_direction` | I8 | rw | SDO only | -1 or +1 |
| `0x2013` | 1..4 | `homing_timeout` | U32 | rw | SDO only |  |
| `0x2014` | 1..4 | `homing_endstop_release` | I32 | rw | SDO only | Distance to back off after touching endstop |
| `0x2015` | 1..4 | `homing_endstop_polarity` | U8 | rw | SDO only | 0=normally low, 1=normally high |

### Tmc — base `0x2020`

TMC2209 silent stepper driver configuration

*Maps to JSON endpoint(s):* `/tmc_act`

*C++ class:* `TMCController`

| Index | Sub | Name | Type | Access | PDO | Description |
|-------|-----|------|------|--------|-----|-------------|
| `0x2020` | 1..4 | `tmc_microsteps` | U16 | rw | SDO only | 1, 2, 4, 8, 16, 32, 64, 128, 256 |
| `0x2021` | 1..4 | `tmc_rms_current` | U16 | rw | SDO only |  |
| `0x2022` | 1..4 | `tmc_stallguard_threshold` | U8 | rw | SDO only | StallGuard4 sensitivity (0-255) |
| `0x2023` | 1..4 | `tmc_coolstep_semin` | U8 | rw | SDO only |  |
| `0x2024` | 1..4 | `tmc_coolstep_semax` | U8 | rw | SDO only |  |
| `0x2025` | 1..4 | `tmc_blank_time` | U8 | rw | SDO only |  |
| `0x2026` | 1..4 | `tmc_toff` | U8 | rw | SDO only |  |
| `0x2027` | 1..4 | `tmc_stall_count` | U32 | ro | SDO only | Cumulative stall events since boot |

### Laser — base `0x2100`

Laser intensity control via PWM, up to 4 channels per node

*Maps to JSON endpoint(s):* `/laser_act, /laser_set`

*C++ class:* `LaserController`

| Index | Sub | Name | Type | Access | PDO | Description |
|-------|-----|------|------|--------|-----|-------------|
| `0x2100` | 1..4 | `laser_pwm_value` | U16 | rw | rpdo3 | PWM duty 0..maxValue (where maxValue = 2^resolution - 1) |
| `0x2101` | 1..4 | `laser_max_value` | U16 | ro | SDO only | Read-only — full scale value (depends on PWM resolution) |
| `0x2102` | 1..4 | `laser_pwm_frequency` | U32 | rw | SDO only |  |
| `0x2103` | 1..4 | `laser_pwm_resolution` | U8 | rw | SDO only | Bits of PWM resolution. Frequency * 2^resolution must be <= APB clock. |
| `0x2104` | 1..4 | `laser_despeckle_period` | U32 | rw | SDO only | Despeckle modulation period; 0 = off |
| `0x2105` | 1..4 | `laser_despeckle_amplitude` | U16 | rw | SDO only |  |
| `0x2106` | 0 | `laser_safety_state` | U8 | ro | tpdo3 | Safety status — TPDO broadcast on change |

### Led — base `0x2200`

Addressable LED array (NeoPixel) with pattern support

*Maps to JSON endpoint(s):* `/led_act`

*C++ class:* `LedController`

| Index | Sub | Name | Type | Access | PDO | Description |
|-------|-----|------|------|--------|-----|-------------|
| `0x2200` | 0 | `led_array_mode` | U8 | rw | rpdo3 | 0 = off 1 = on (uniform colour from 0x2202) 2 = left half 3 = right half 4 = ... |
| `0x2201` | 0 | `led_brightness` | U8 | rw | rpdo3 | Global brightness 0..255 |
| `0x2202` | 0 | `led_uniform_colour` | U32 | rw | rpdo3 | 0x00RRGGBB packed colour for uniform fill modes |
| `0x2203` | 0 | `led_pixel_count` | U16 | ro | SDO only | Number of physical LEDs on this node |
| `0x2204` | 0 | `led_layout_width` | U8 | ro | SDO only |  |
| `0x2205` | 0 | `led_layout_height` | U8 | ro | SDO only |  |
| `0x2210` | 0 | `led_pixel_data` | DOMAIN | rw | SDO only | Bulk pixel data via SDO segmented/block transfer. Format: tightly-packed N x ... |
| `0x2220` | 0 | `led_pattern_id` | U8 | rw | rpdo3 | Built-in pattern: 0=none, 1=rainbow, 2=breathe, 3=chase, 4=fire, 5=sparkle, 6... |
| `0x2221` | 0 | `led_pattern_speed` | U16 | rw | SDO only | Animation speed (frames per second) |

### Digital_Io — base `0x2300`

Endstops, triggers, generic GPIOs

*Maps to JSON endpoint(s):* `/digitalin_get, /digitalout_act`

*C++ class:* `DigitalInController, DigitalOutController`

| Index | Sub | Name | Type | Access | PDO | Description |
|-------|-----|------|------|--------|-----|-------------|
| `0x2300` | 1..8 | `digital_input_state` | U8 | ro | tpdo2 | Per-channel digital input state (0/1) |
| `0x2301` | 1..8 | `digital_output_command` | U8 | rw | rpdo4 |  |
| `0x2302` | 0 | `digital_input_change_mask` | U8 | ro | tpdo2 | Bit set if corresponding input changed since last TPDO. Slave clears after TP... |

### Analog_Io — base `0x2310`

ADC inputs, DAC outputs

*Maps to JSON endpoint(s):* `/analogin_get, /dac_act`

| Index | Sub | Name | Type | Access | PDO | Description |
|-------|-----|------|------|--------|-----|-------------|
| `0x2310` | 1..8 | `analog_input_value` | U16 | ro | tpdo2 |  |
| `0x2311` | 1..8 | `analog_input_filtered` | U16 | ro | SDO only | EWMA-filtered ADC reading |
| `0x2320` | 1..4 | `dac_output_value` | U16 | rw | SDO only |  |

### Encoder — base `0x2340`

Quadrature or magnetic (AS5600) encoder feedback

*C++ class:* `EncoderController`

| Index | Sub | Name | Type | Access | PDO | Description |
|-------|-----|------|------|--------|-----|-------------|
| `0x2340` | 1..4 | `encoder_position` | I32 | ro | tpdo3 |  |
| `0x2341` | 1..4 | `encoder_velocity` | I32 | ro | tpdo3 |  |
| `0x2342` | 1..4 | `encoder_zero_offset` | I32 | rw | SDO only |  |

### Joystick — base `0x2400`

Analog joystick or PSX gamepad bridge

*C++ class:* `JoystickController`

| Index | Sub | Name | Type | Access | PDO | Description |
|-------|-----|------|------|--------|-----|-------------|
| `0x2400` | 1..4 | `joystick_axis` | I16 | ro | tpdo4 | X, Y, Z, R axes (-32768..32767) |
| `0x2401` | 0 | `joystick_buttons` | U16 | ro | tpdo4 | Button bitmask |
| `0x2402` | 0 | `joystick_speed_multiplier` | U8 | rw | SDO only |  |
| `0x2403` | 0 | `joystick_deadzone` | U16 | rw | SDO only |  |

### System — base `0x2500`

Firmware version, board info, runtime stats

*Maps to JSON endpoint(s):* `/state_get, /modules_get`

| Index | Sub | Name | Type | Access | PDO | Description |
|-------|-----|------|------|--------|-----|-------------|
| `0x2500` | 0 | `firmware_version_string` | STRING | ro | SDO only |  |
| `0x2501` | 0 | `board_name` | STRING | ro | SDO only |  |
| `0x2502` | 0 | `enabled_modules_bitmask` | U32 | rw | SDO only | Bit 0  = motor Bit 1  = laser Bit 2  = led Bit 3  = digitalIn Bit 4  = digita... |
| `0x2503` | 0 | `uptime_seconds` | U32 | ro | SDO only |  |
| `0x2504` | 0 | `free_heap_bytes` | U32 | ro | SDO only |  |
| `0x2505` | 0 | `can_error_counter` | U32 | ro | SDO only |  |
| `0x2506` | 0 | `cpu_temperature` | I16 | ro | SDO only |  |
| `0x2507` | 0 | `reboot_command` | U8 | wo | SDO only | Write 1 to soft-reboot the node |

### Galvo — base `0x2600`

Galvo mirrors with raster/line scanning

*Maps to JSON endpoint(s):* `/galvo_act, stage_scan`

*C++ class:* `GalvoController`

| Index | Sub | Name | Type | Access | PDO | Description |
|-------|-----|------|------|--------|-----|-------------|
| `0x2600` | 1..2 | `galvo_target_position` | I32 | rw | rpdo4 | Target X (sub1) and Y (sub2) galvo position in DAC counts |
| `0x2601` | 1..2 | `galvo_actual_position` | I32 | ro | tpdo4 |  |
| `0x2602` | 0 | `galvo_command_word` | U8 | rw | rpdo4 | 0 = idle 1 = goto (single point) 2 = line scan (X axis sweep) 3 = raster scan... |
| `0x2603` | 0 | `galvo_status_word` | U8 | ro | tpdo4 |  |
| `0x2604` | 0 | `galvo_scan_speed` | U32 | rw | SDO only |  |
| `0x2605` | 0 | `galvo_n_steps_line` | U16 | rw | SDO only |  |
| `0x2606` | 0 | `galvo_n_steps_pixel` | U16 | rw | SDO only |  |
| `0x2607` | 0 | `galvo_d_steps_line` | U16 | rw | SDO only |  |
| `0x2608` | 0 | `galvo_d_steps_pixel` | U16 | rw | SDO only |  |
| `0x2609` | 0 | `galvo_t_pre_us` | U32 | rw | SDO only | Pre-trigger settle time |
| `0x260A` | 0 | `galvo_t_post_us` | U32 | rw | SDO only | Post-trigger hold time |
| `0x260B` | 0 | `galvo_x_start` | I32 | rw | SDO only |  |
| `0x260C` | 0 | `galvo_y_start` | I32 | rw | SDO only |  |
| `0x260D` | 0 | `galvo_x_step` | I32 | rw | SDO only |  |
| `0x260E` | 0 | `galvo_y_step` | I32 | rw | SDO only |  |
| `0x260F` | 0 | `galvo_camera_trigger_mode` | U8 | rw | SDO only | 0=off, 1=per-pixel, 2=per-line, 3=per-frame |

### Pid — base `0x2700`

Generic PID controller (e.g. for focus stabilization)

*C++ class:* `PIDController`

| Index | Sub | Name | Type | Access | PDO | Description |
|-------|-----|------|------|--------|-----|-------------|
| `0x2700` | 0 | `pid_setpoint` | I32 | rw | rpdo4 |  |
| `0x2701` | 0 | `pid_actual_value` | I32 | ro | tpdo4 |  |
| `0x2702` | 0 | `pid_kp` | U32 | rw | SDO only | Proportional gain (x1000 fixed point) |
| `0x2703` | 0 | `pid_ki` | U32 | rw | SDO only |  |
| `0x2704` | 0 | `pid_kd` | U32 | rw | SDO only |  |
| `0x2705` | 0 | `pid_enable` | U8 | rw | SDO only |  |

### Ota — base `0x2F00`

Firmware update via SDO block transfer

*C++ class:* `CanOpenOTA`

| Index | Sub | Name | Type | Access | PDO | Description |
|-------|-----|------|------|--------|-----|-------------|
| `0x2F00` | 0 | `ota_firmware_data` | DOMAIN | wo | SDO only | Write firmware binary via SDO block transfer. Each chunk streams to esp_ota_w... |
| `0x2F01` | 0 | `ota_firmware_size` | U32 | rw | SDO only | Total firmware size in bytes. Set BEFORE starting transfer. |
| `0x2F02` | 0 | `ota_firmware_crc32` | U32 | rw | SDO only | Expected CRC32. Verified after transfer completes. |
| `0x2F03` | 0 | `ota_status` | U8 | ro | tpdo3 | 0 = idle 1 = receiving 2 = verifying 3 = success (rebooting in 1s) 4 = error |
| `0x2F04` | 0 | `ota_bytes_received` | U32 | ro | SDO only | Progress indicator — bytes written to flash so far |
| `0x2F05` | 0 | `ota_error_code` | U8 | ro | SDO only | 0 = no error 1 = no OTA partition 2 = esp_ota_begin failed 3 = esp_ota_write ... |

## PDO mapping summary

### RPDO1

- COB-ID: `0x200 + node_id`
- Direction: m2s
- Description: Hot-path motor command — single frame triggers a move

| # | Index:Sub | Bits | Maps to |
|---|-----------|------|---------|
| 1 | `0x2000:01` | 32 | `motor_target_position` |
| 2 | `0x2002:01` | 32 | `motor_speed` |

### RPDO2

- COB-ID: `0x300 + node_id`
- Direction: m2s
- Description: Motor control flags + enable

| # | Index:Sub | Bits | Maps to |
|---|-----------|------|---------|
| 1 | `0x2003:00` | 8 | `motor_command_word` |
| 2 | `0x2007:01` | 8 | `motor_is_absolute` |
| 3 | `0x2005:01` | 8 | `motor_enable` |
| 4 | `0x2010:01` | 8 | `homing_command` |

### RPDO3

- COB-ID: `0x400 + node_id`
- Direction: m2s
- Description: Laser + LED control (illumination node)

| # | Index:Sub | Bits | Maps to |
|---|-----------|------|---------|
| 1 | `0x2100:01` | 16 | `laser_pwm_value` |
| 2 | `0x2200:00` | 8 | `led_array_mode` |
| 3 | `0x2201:00` | 8 | `led_brightness` |
| 4 | `0x2202:00` | 32 | `led_uniform_colour` |

### RPDO4

- COB-ID: `0x500 + node_id`
- Direction: m2s
- Description: Galvo target position (XY in one frame)

| # | Index:Sub | Bits | Maps to |
|---|-----------|------|---------|
| 1 | `0x2600:01` | 32 | `galvo_target_position` |
| 2 | `0x2600:02` | 32 | `galvo_target_position` |

### TPDO1

- COB-ID: `0x180 + node_id`
- Direction: s2m
- Description: Motor actual position + status — slave PUSHES on change
- Event timer: 100 ms
- Inhibit time: 5 ms

| # | Index:Sub | Bits | Maps to |
|---|-----------|------|---------|
| 1 | `0x2001:01` | 32 | `motor_actual_position` |
| 2 | `0x2004:01` | 8 | `motor_status_word` |

### TPDO2

- COB-ID: `0x280 + node_id`
- Direction: s2m
- Description: Digital + analog input state
- Event timer: 200 ms
- Inhibit time: 10 ms

| # | Index:Sub | Bits | Maps to |
|---|-----------|------|---------|
| 1 | `0x2300:01` | 8 | `digital_input_state` |
| 2 | `0x2300:02` | 8 | `digital_input_state` |
| 3 | `0x2310:01` | 16 | `analog_input_value` |
| 4 | `0x2310:02` | 16 | `analog_input_value` |

### TPDO3

- COB-ID: `0x380 + node_id`
- Direction: s2m
- Description: Encoder feedback + safety status
- Event timer: 50 ms
- Inhibit time: 5 ms

| # | Index:Sub | Bits | Maps to |
|---|-----------|------|---------|
| 1 | `0x2340:01` | 32 | `encoder_position` |
| 2 | `0x2106:00` | 8 | `laser_safety_state` |
| 3 | `0x2F03:00` | 8 | `ota_status` |

### TPDO4

- COB-ID: `0x480 + node_id`
- Direction: s2m
- Description: Joystick / galvo status / PID feedback
- Event timer: 50 ms

| # | Index:Sub | Bits | Maps to |
|---|-----------|------|---------|
| 1 | `0x2400:01` | 16 | `joystick_axis` |
| 2 | `0x2400:02` | 16 | `joystick_axis` |
| 3 | `0x2601:01` | 32 | `galvo_actual_position` |

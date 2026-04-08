"""
AUTO-GENERATED from uc2_canopen_registry.yaml — DO NOT EDIT
Regenerate with: python tools/canopen/regenerate_all.py

Use these constants when writing CANopen scripts via python-canopen
or the Waveshare USB-CAN adapter.

Example:
    import canopen
    from uc2_indices import OD, NODE
    network = canopen.Network()
    network.connect(channel='can0', bustype='socketcan')
    node = network.add_node(NODE.MOTOR_X, 'openUC2_satellite.eds')
    node.sdo[OD.MOTOR_TARGET_POSITION][1].raw = 1000
"""

class OD:
    """CANopen Object Dictionary indices for openUC2 satellite nodes."""
    # MOTOR — Stepper motor control with TMC2209 driver, up to 4 axes per node
    MOTOR_TARGET_POSITION = 0x2000
    MOTOR_ACTUAL_POSITION = 0x2001
    MOTOR_SPEED = 0x2002
    MOTOR_COMMAND_WORD = 0x2003
    MOTOR_STATUS_WORD = 0x2004
    MOTOR_ENABLE = 0x2005
    MOTOR_ACCELERATION = 0x2006
    MOTOR_IS_ABSOLUTE = 0x2007
    MOTOR_MIN_POSITION = 0x2008
    MOTOR_MAX_POSITION = 0x2009
    MOTOR_JERK = 0x200A

    # HOMING — Sensorless or endstop-based homing
    HOMING_COMMAND = 0x2010
    HOMING_SPEED = 0x2011
    HOMING_DIRECTION = 0x2012
    HOMING_TIMEOUT = 0x2013
    HOMING_ENDSTOP_RELEASE = 0x2014
    HOMING_ENDSTOP_POLARITY = 0x2015

    # TMC — TMC2209 silent stepper driver configuration
    TMC_MICROSTEPS = 0x2020
    TMC_RMS_CURRENT = 0x2021
    TMC_STALLGUARD_THRESHOLD = 0x2022
    TMC_COOLSTEP_SEMIN = 0x2023
    TMC_COOLSTEP_SEMAX = 0x2024
    TMC_BLANK_TIME = 0x2025
    TMC_TOFF = 0x2026
    TMC_STALL_COUNT = 0x2027

    # LASER — Laser intensity control via PWM, up to 4 channels per node
    LASER_PWM_VALUE = 0x2100
    LASER_MAX_VALUE = 0x2101
    LASER_PWM_FREQUENCY = 0x2102
    LASER_PWM_RESOLUTION = 0x2103
    LASER_DESPECKLE_PERIOD = 0x2104
    LASER_DESPECKLE_AMPLITUDE = 0x2105
    LASER_SAFETY_STATE = 0x2106

    # LED — Addressable LED array (NeoPixel) with pattern support
    LED_ARRAY_MODE = 0x2200
    LED_BRIGHTNESS = 0x2201
    LED_UNIFORM_COLOUR = 0x2202
    LED_PIXEL_COUNT = 0x2203
    LED_LAYOUT_WIDTH = 0x2204
    LED_LAYOUT_HEIGHT = 0x2205
    LED_PIXEL_DATA = 0x2210
    LED_PATTERN_ID = 0x2220
    LED_PATTERN_SPEED = 0x2221

    # DIGITAL_IO — Endstops, triggers, generic GPIOs
    DIGITAL_INPUT_STATE = 0x2300
    DIGITAL_OUTPUT_COMMAND = 0x2301
    DIGITAL_INPUT_CHANGE_MASK = 0x2302

    # ANALOG_IO — ADC inputs, DAC outputs
    ANALOG_INPUT_VALUE = 0x2310
    ANALOG_INPUT_FILTERED = 0x2311
    DAC_OUTPUT_VALUE = 0x2320

    # ENCODER — Quadrature or magnetic (AS5600) encoder feedback
    ENCODER_POSITION = 0x2340
    ENCODER_VELOCITY = 0x2341
    ENCODER_ZERO_OFFSET = 0x2342

    # JOYSTICK — Analog joystick or PSX gamepad bridge
    JOYSTICK_AXIS = 0x2400
    JOYSTICK_BUTTONS = 0x2401
    JOYSTICK_SPEED_MULTIPLIER = 0x2402
    JOYSTICK_DEADZONE = 0x2403

    # SYSTEM — Firmware version, board info, runtime stats
    FIRMWARE_VERSION_STRING = 0x2500
    BOARD_NAME = 0x2501
    ENABLED_MODULES_BITMASK = 0x2502
    UPTIME_SECONDS = 0x2503
    FREE_HEAP_BYTES = 0x2504
    CAN_ERROR_COUNTER = 0x2505
    CPU_TEMPERATURE = 0x2506
    REBOOT_COMMAND = 0x2507

    # GALVO — Galvo mirrors with raster/line scanning
    GALVO_TARGET_POSITION = 0x2600
    GALVO_ACTUAL_POSITION = 0x2601
    GALVO_COMMAND_WORD = 0x2602
    GALVO_STATUS_WORD = 0x2603
    GALVO_SCAN_SPEED = 0x2604
    GALVO_N_STEPS_LINE = 0x2605
    GALVO_N_STEPS_PIXEL = 0x2606
    GALVO_D_STEPS_LINE = 0x2607
    GALVO_D_STEPS_PIXEL = 0x2608
    GALVO_T_PRE_US = 0x2609
    GALVO_T_POST_US = 0x260A
    GALVO_X_START = 0x260B
    GALVO_Y_START = 0x260C
    GALVO_X_STEP = 0x260D
    GALVO_Y_STEP = 0x260E
    GALVO_CAMERA_TRIGGER_MODE = 0x260F

    # PID — Generic PID controller (e.g. for focus stabilization)
    PID_SETPOINT = 0x2700
    PID_ACTUAL_VALUE = 0x2701
    PID_KP = 0x2702
    PID_KI = 0x2703
    PID_KD = 0x2704
    PID_ENABLE = 0x2705

    # OTA — Firmware update via SDO block transfer
    OTA_FIRMWARE_DATA = 0x2F00
    OTA_FIRMWARE_SIZE = 0x2F01
    OTA_FIRMWARE_CRC32 = 0x2F02
    OTA_STATUS = 0x2F03
    OTA_BYTES_RECEIVED = 0x2F04
    OTA_ERROR_CODE = 0x2F05

class NODE:
    """Node-ID assignments for the openUC2 CAN bus."""
    MASTER = 1
    MOTOR_A = 14
    MOTOR_X = 11
    MOTOR_Y = 12
    MOTOR_Z = 13
    LED = 20
    LASER = 21
    JOYSTICK = 22
    GALVO = 30
    GALVO_2 = 31
    ENCODER = 40
    PID = 50

# Reverse lookup: index → human name
OD_NAMES = {
    0x2000: 'motor_target_position',
    0x2001: 'motor_actual_position',
    0x2002: 'motor_speed',
    0x2003: 'motor_command_word',
    0x2004: 'motor_status_word',
    0x2005: 'motor_enable',
    0x2006: 'motor_acceleration',
    0x2007: 'motor_is_absolute',
    0x2008: 'motor_min_position',
    0x2009: 'motor_max_position',
    0x200A: 'motor_jerk',
    0x2010: 'homing_command',
    0x2011: 'homing_speed',
    0x2012: 'homing_direction',
    0x2013: 'homing_timeout',
    0x2014: 'homing_endstop_release',
    0x2015: 'homing_endstop_polarity',
    0x2020: 'tmc_microsteps',
    0x2021: 'tmc_rms_current',
    0x2022: 'tmc_stallguard_threshold',
    0x2023: 'tmc_coolstep_semin',
    0x2024: 'tmc_coolstep_semax',
    0x2025: 'tmc_blank_time',
    0x2026: 'tmc_toff',
    0x2027: 'tmc_stall_count',
    0x2100: 'laser_pwm_value',
    0x2101: 'laser_max_value',
    0x2102: 'laser_pwm_frequency',
    0x2103: 'laser_pwm_resolution',
    0x2104: 'laser_despeckle_period',
    0x2105: 'laser_despeckle_amplitude',
    0x2106: 'laser_safety_state',
    0x2200: 'led_array_mode',
    0x2201: 'led_brightness',
    0x2202: 'led_uniform_colour',
    0x2203: 'led_pixel_count',
    0x2204: 'led_layout_width',
    0x2205: 'led_layout_height',
    0x2210: 'led_pixel_data',
    0x2220: 'led_pattern_id',
    0x2221: 'led_pattern_speed',
    0x2300: 'digital_input_state',
    0x2301: 'digital_output_command',
    0x2302: 'digital_input_change_mask',
    0x2310: 'analog_input_value',
    0x2311: 'analog_input_filtered',
    0x2320: 'dac_output_value',
    0x2340: 'encoder_position',
    0x2341: 'encoder_velocity',
    0x2342: 'encoder_zero_offset',
    0x2400: 'joystick_axis',
    0x2401: 'joystick_buttons',
    0x2402: 'joystick_speed_multiplier',
    0x2403: 'joystick_deadzone',
    0x2500: 'firmware_version_string',
    0x2501: 'board_name',
    0x2502: 'enabled_modules_bitmask',
    0x2503: 'uptime_seconds',
    0x2504: 'free_heap_bytes',
    0x2505: 'can_error_counter',
    0x2506: 'cpu_temperature',
    0x2507: 'reboot_command',
    0x2600: 'galvo_target_position',
    0x2601: 'galvo_actual_position',
    0x2602: 'galvo_command_word',
    0x2603: 'galvo_status_word',
    0x2604: 'galvo_scan_speed',
    0x2605: 'galvo_n_steps_line',
    0x2606: 'galvo_n_steps_pixel',
    0x2607: 'galvo_d_steps_line',
    0x2608: 'galvo_d_steps_pixel',
    0x2609: 'galvo_t_pre_us',
    0x260A: 'galvo_t_post_us',
    0x260B: 'galvo_x_start',
    0x260C: 'galvo_y_start',
    0x260D: 'galvo_x_step',
    0x260E: 'galvo_y_step',
    0x260F: 'galvo_camera_trigger_mode',
    0x2700: 'pid_setpoint',
    0x2701: 'pid_actual_value',
    0x2702: 'pid_kp',
    0x2703: 'pid_ki',
    0x2704: 'pid_kd',
    0x2705: 'pid_enable',
    0x2F00: 'ota_firmware_data',
    0x2F01: 'ota_firmware_size',
    0x2F02: 'ota_firmware_crc32',
    0x2F03: 'ota_status',
    0x2F04: 'ota_bytes_received',
    0x2F05: 'ota_error_code',
}

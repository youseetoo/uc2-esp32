#pragma once
// T4.1: PinConfig — shared safe-default initializer (Phase 4 C conversion).
//
// PINCONFIG_DEFAULTS_FIELDS expands to the designated-initializer field list
// of the safe all-disabled default table (single source of truth, formerly
// the C++ base-class defaults in PinConfigDefault.h). Usage:
//
//   const struct PinConfig pinConfig = {
//       PINCONFIG_DEFAULTS_FIELDS
//       /* board overrides — ONLY fields that differ from the defaults */
//       .pindefName = "MyBoard",
//       .LASER_0 = GPIO_NUM_2,
//   };
//
// The macro ends with a trailing comma (valid before the board overrides or
// before the closing brace), so the board's overrides follow directly.
// Re-initializing a field the macro already set is INTENTIONAL (C23: later
// designator wins) — but GCC 15 (gnu23 + -Wextra + -Werror) promotes
// -Woverride-init to an error, so each app adds -Wno-override-init to the
// main target (see apps/<app>/CMakeLists.txt).
// pinconfig_defaults() in
// pinconfig_defaults.c is the same table as a function for C callers that
// want to copy/patch it.
#include "PinConfigDefault.h"

#define PINCONFIG_DEFAULTS_FIELDS \
    /* Task / system */ \
    .dumpHeap = false, \
    .DEFAULT_TASK_PRIORITY = 0, \
    .MAIN_TASK_STACKSIZE = 8128, \
    .ANALOGJOYSTICK_TASK_STACKSIZE = 1024, \
    .STAGESCAN_TASK_STACKSIZE = 4 * 2048, \
    .HIDCONTROLLER_EVENT_STACK_SIZE = 2 * 2048, /* Don't go below 2048 */ \
    .HTTP_MAX_URI_HANDLERS = 35, \
    .BT_CONTROLLER_TASK_STACKSIZE = 4 * 2048, \
    .MOTOR_TASK_STACKSIZE = 4 * 1024, \
    .MOTOR_TASK_UPDATEWEBSOCKET_STACKSIZE = 6 * 1024, \
    .INTERRUPT_CONTROLLER_TASK_STACKSIZE = 6 * 1024, \
    .TCA_TASK_STACKSIZE = 2048, \
    .SCANNER_TASK_STACKSIZE = 10000, \
    .TEMPERATURE_TASK_STACKSIZE = 1024, \
    .BAUDRATE = 115200, \
    .serialTimeout = 50, \
    .pindefName = "pindef", \
    /* CAN / debug */ \
    .DEBUG_CAN_ISO_TP = false, \
    .CAN_ID_CURRENT = (uint32_t)disabled, \
    .AccelStepperMotorType = 8, \
    /* Motor step/dir pins (all disabled) */ \
    .MOTOR_X_DIR = disabled, .MOTOR_X_STEP = disabled, \
    .MOTOR_Y_DIR = disabled, .MOTOR_Y_STEP = disabled, \
    .MOTOR_Z_DIR = disabled, .MOTOR_Z_STEP = disabled, \
    .MOTOR_A_DIR = disabled, .MOTOR_A_STEP = disabled, \
    .MOTOR_B_DIR = disabled, .MOTOR_B_STEP = disabled, \
    .MOTOR_C_DIR = disabled, .MOTOR_C_STEP = disabled, \
    .MOTOR_D_DIR = disabled, .MOTOR_D_STEP = disabled, \
    .MOTOR_E_DIR = disabled, .MOTOR_E_STEP = disabled, \
    .MOTOR_F_DIR = disabled, .MOTOR_F_STEP = disabled, \
    .MOTOR_G_DIR = disabled, .MOTOR_G_STEP = disabled, \
    /* Hard limits */ \
    .hardLimitEnabledA = false, .hardLimitPolarityA = false, \
    .hardLimitEnabledX = false, .hardLimitPolarityX = false, \
    .hardLimitEnabledY = false, .hardLimitPolarityY = false, \
    .hardLimitEnabledZ = false, .hardLimitPolarityZ = false, \
    .homeEndstopReleaseSteps = 50, \
    /* Motor enable / homing */ \
    .MOTOR_ENABLE = disabled, \
    .MOTOR_ENABLE_INVERTED = false, \
    .MOTOR_AUTOENABLE = false, \
    .HOME_X_DIR = -1, .HOME_Y_DIR = -1, .HOME_Z_DIR = -1, \
    .HOME_X_SPEED = 3000, .HOME_Y_SPEED = 3000, .HOME_Z_SPEED = 3000, \
    .HOME_X_SIG_INVERTED = false, \
    .HOME_Y_SIG_INVERTED = false, \
    .HOME_Z_SIG_INVERTED = false, \
    /* I2C / motor encoder pins */ \
    .I2C_INT = GPIO_NUM_NC, \
    .MOTOR_X_0 = disabled, .MOTOR_X_1 = disabled, \
    .MOTOR_Y_0 = disabled, .MOTOR_Y_1 = disabled, \
    .MOTOR_Z_0 = disabled, .MOTOR_Z_1 = disabled, \
    .MOTOR_A_0 = disabled, .MOTOR_A_1 = disabled, \
    .objectiveMotorAxis = (uint8_t)disabled, \
    .focusMotorAxis = (uint8_t)disabled, \
    /* LED */ \
    .LED_PIN = disabled, \
    .LED_COUNT = disabled, \
    .IS_STATUS_LED = false, \
    .RING_INNER_START = 0, .RING_INNER_COUNT = 20, \
    .RING_MIDDLE_START = 20, .RING_MIDDLE_COUNT = 28, \
    .RING_BIGGEST_START = 48, .RING_BIGGEST_COUNT = 40, \
    .RING_OUTEST_START = 88, .RING_OUTEST_COUNT = 48, \
    .MATRIX_W = 8, \
    .MATRIX_H = 8, \
    .SEGMENT_INNER_RIGHT_START = 0, .SEGMENT_INNER_BOTTOM_START = 0, \
    .SEGMENT_INNER_LEFT_START = 0, .SEGMENT_INNER_TOP_START = 0, \
    .SEGMENT_INNER_COUNT = 0, \
    .SEGMENT_MIDDLE_RIGHT_START = 0, .SEGMENT_MIDDLE_BOTTOM_START = 0, \
    .SEGMENT_MIDDLE_LEFT_START = 0, .SEGMENT_MIDDLE_TOP_START = 0, \
    .SEGMENT_MIDDLE_COUNT = 0, \
    .SEGMENT_BIGGEST_RIGHT_START = 0, .SEGMENT_BIGGEST_BOTTOM_START = 0, \
    .SEGMENT_BIGGEST_LEFT_START = 0, .SEGMENT_BIGGEST_TOP_START = 0, \
    .SEGMENT_BIGGEST_COUNT = 0, \
    .SEGMENT_OUTEST_RIGHT_START = 0, .SEGMENT_OUTEST_BOTTOM_START = 0, \
    .SEGMENT_OUTEST_LEFT_START = 0, .SEGMENT_OUTEST_TOP_START = 0, \
    .SEGMENT_OUTEST_COUNT = 0, \
    /* Joystick / laser / digital out */ \
    .ANLOG_JOYSTICK_X = disabled, .ANLOG_JOYSTICK_Y = disabled, \
    .LASER_1 = disabled, .LASER_2 = disabled, \
    .LASER_3 = disabled, .LASER_4 = disabled, .LASER_0 = disabled, \
    .DIGITAL_OUT_1 = disabled, .DIGITAL_OUT_2 = disabled, \
    .DIGITAL_OUT_3 = disabled, \
    .testLaserPinOnBoot = false, \
    .LASER_PWM_FREQUENCY = 5000, /* Hz, default for laser diodes */ \
    .LASER_PWM_RESOLUTION = 10,  /* bits (1024 steps) */ \
    /* Camera / digital in / DAC / analog */ \
    .CAMERA_TRIGGER_PIN = disabled, \
    .CAMERA_TRIGGER_INVERTED = false, \
    .DIGITAL_IN_1 = disabled, .DIGITAL_IN_2 = disabled, .DIGITAL_IN_3 = disabled, \
    .dac_fake_1 = disabled, .dac_fake_2 = disabled, \
    .analogout_PIN_1 = disabled, .analogout_PIN_2 = disabled, \
    .analogout_PIN_3 = disabled, \
    .analogin_PIN_0 = disabled, .analogin_PIN_1 = disabled, \
    .analogin_PIN_2 = disabled, .analogin_PIN_3 = disabled, \
    .pid1 = disabled, .pid2 = disabled, .pid3 = disabled, \
    /* PSX / joystick tuning */ \
    .PSX_MAC = "", \
    .PSX_CONTROLLER_TYPE = 0, /* 1 = ps3, 2 = ps4 */ \
    .JOYSTICK_SPEED_MULTIPLIER = 50, \
    .JOYSTICK_MAX_ILLU = 255, \
    .JOYSTICK_SPEED_MULTIPLIER_Z = 10, \
    /* WiFi / mDNS */ \
    .mSSID = "Uc2", \
    .mPWD = "12345678", \
    .mSSIDAP = "UC2", \
    .hostname = "youseetoo", \
    .mAP = true, \
    /* Caliper / encoder */ \
    .X_CAL_DATA = disabled, .Y_CAL_DATA = disabled, .Z_CAL_DATA = disabled, \
    .X_CAL_CLK = disabled, .Y_CAL_CLK = disabled, .Z_CAL_CLK = disabled, \
    .ENC_X_A = disabled, .ENC_Y_A = disabled, .ENC_Z_A = disabled, \
    .ENC_X_B = disabled, .ENC_Y_B = disabled, .ENC_Z_B = disabled, \
    .ENC_X_encoderDirection = true, \
    .ENC_Y_encoderDirection = true, \
    .ENC_Z_encoderDirection = true, \
    /* TMC2209 */ \
    .tmc_SW_RX = disabled, \
    .tmc_SW_TX = disabled, \
    .tmc_pin_diag = disabled, \
    .TMC_DEBUG = false, \
    .tmc_microsteps = 16, \
    .tmc_rms_current = 500, \
    .tmc_stall_value = 100, \
    .tmc_sgthrs = 100, \
    .tmc_semin = 0, \
    .tmc_semax = 0, \
    .tmc_sedn = 0b01, \
    .tmc_tcoolthrs = 0xFFFFF, \
    .tmc_blank_time = 24, \
    .tmc_toff = 4, \
    .tmc_en_spreadcycle = 1, \
    .tmc_tpwmthrs_sps = 15000, \
    .tmc_hstrt = 5, \
    .tmc_hend = 0, \
    .tmc_internal_vref = true, \
    .tmc_hold_multiplier_pct = 50, \
    /* Thermal (NTC) */ \
    .thermal_PIN_0 = disabled, .thermal_PIN_1 = disabled, \
    .thermal_PIN_2 = disabled, .thermal_PIN_3 = disabled, \
    .thermal_series_r_ohm = 1000, /* lower leg of the divider */ \
    .thermal_supply_mv = 3300,    /* rail feeding the NTC */ \
    .thermal_warn_c = 58.0f, \
    .thermal_critical_c = 70.0f, \
    .thermal_recover_c = 50.0f, \
    .thermal_enabled_default = false, \
    /* I2C / aux devices */ \
    .isI2Cinitiated = false, \
    .I2C_SCL = disabled, \
    .I2C_SDA = disabled, \
    .I2C_ADD_TCA = disabled, \
    .I2C_CONTROLLER_TYPE = I2C_CONTROLLER_DISABLED, \
    .I2C_ADD_SLAVE = (uint8_t)disabled, \
    .REMOTE_MOTOR_AXIS_ID = 1, \
    .REMOTE_LASER_ID = 0, \
    .I2C_ADD_MOT_X = 0x40, .I2C_ADD_MOT_Y = 0x41, \
    .I2C_ADD_MOT_Z = 0x42, .I2C_ADD_MOT_A = 0x43, \
    .I2C_ADD_LEX_MAT = 0x60, \
    .I2C_ADD_LEX_PWM0 = 0x50, .I2C_ADD_LEX_PWM1 = 0x51, \
    .I2C_ADD_LEX_PWM2 = 0x52, .I2C_ADD_LEX_PWM3 = 0x53, \
    .I2C_ADD_M5_DIAL = 0x60, \
    /* SPI / galvo */ \
    .SPI_MOSI = disabled, .SPI_MISO = disabled, \
    .SPI_SCK = disabled, .SPI_CS = disabled, \
    .galvo_miso = (uint8_t)disabled, .galvo_sck = (uint8_t)disabled, \
    .galvo_sdi = (uint8_t)disabled, .galvo_cs = (uint8_t)disabled, \
    .galvo_ldac = (uint8_t)disabled, .galvo_laser = (uint8_t)disabled, \
    .galvo_trig_pixel = (uint8_t)disabled, \
    .galvo_trig_line = (uint8_t)disabled, \
    .galvo_trig_frame = (uint8_t)disabled, \
    /* Buzzer / temp / fan */ \
    .BUZZER_PIN = disabled, \
    .DS28b20_PIN = disabled, \
    .FAN_TACHO_PIN = disabled, \
    /* CAN */ \
    .CAN_TX = 5, \
    .CAN_RX = 44, \
    .CAN_ID_CENTRAL_NODE = 1, \
    .CAN_ID_MOT_A = 10, .CAN_ID_MOT_X = 11, \
    .CAN_ID_MOT_Y = 12, .CAN_ID_MOT_Z = 13, \
    .CAN_ID_MOT_B = 14, .CAN_ID_MOT_C = 15, .CAN_ID_MOT_D = 16, \
    .CAN_ID_MOT_E = 17, .CAN_ID_MOT_F = 18, .CAN_ID_MOT_G = 19, \
    .CAN_ID_LASER_0 = 20, .CAN_ID_LASER_1 = 21, \
    .CAN_ID_LASER_2 = 22, .CAN_ID_LASER_3 = 23, .CAN_ID_LASER_4 = 24, \
    .CAN_NODE_LASER = {20, 21, 22, 23}, \
    .CAN_SUBAXIS_LASER = {0, 1, 2, 3}, \
    .CAN_ID_LED_0 = 30, \
    .CAN_ID_GALVO_0 = 40, \
    .CAN_ID_GPIO_0 = 60, \
    .ROUTE_MOTOR = {disabled, disabled, disabled, disabled}, \
    .ROUTE_HOME = {disabled, disabled, disabled, disabled}, \
    .ROUTE_TMC = {disabled, disabled, disabled, disabled}, \
    .ROUTE_LASER = {disabled, disabled, disabled, disabled}, \
    .ROUTE_LASER_4 = disabled, \
    .CAN_NODE_LASER_4 = 30, \
    .CAN_SUBAXIS_LASER_4 = 0, \
    .ROUTE_LED = disabled, \
    .ROUTE_GALVO = disabled, \
    .CAN_ID_SECONDARY = 0, \
    /* Hybrid mode */ \
    .HYBRID_MOTOR_CAN_THRESHOLD = 4, \
    .HYBRID_LASER_CAN_THRESHOLD = 4, \
    .HYBRID_LED_DUAL_OUTPUT = false, \
    .LED_AUTO_OFF_TIME_MS = 60000, \
    .LED_AUTO_OFF_INTENSITY_THRESHOLD = 150, \
    /* E-stop / bus power */ \
    .pinEmergencyExit = disabled, \
    .pinEmergencyExitPolarity = 1, \
    .BUSPOWER_OFF_PIN = disabled, \
    /* GPIO-CAN slave */ \
    .GPIO_ESTOP_PIN = disabled, \
    .GPIO_COLLISION_ADC = disabled, \
    .GPIO_COLLISION_THRESHOLD_DEFAULT = 150, \
    .GPIO_COLLISION_SENSITIVITY_DEFAULT = 4, \
    .GPIO_ADC_FILTER_ALPHA_X1024 = 128, \
    .MASTER_GPIO_SLAVE_NODE_ID = disabled, \
    .MASTER_PTZ_NODE_ID = disabled,

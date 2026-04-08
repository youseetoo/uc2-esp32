// AUTO-GENERATED from uc2_canopen_registry.yaml
// Regenerate with: python tools/regenerate_all.py
//
// This file is the CANopenNode v4 Object Dictionary header.
// It declares OD_RAM (the runtime data) and OD (the descriptor table).

#ifndef OD_H
#define OD_H

#include "301/CO_ODinterface.h"

#define OD_CNT_NMT                 1
#define OD_CNT_EM                  1
#define OD_CNT_SYNC                1
#define OD_CNT_HB_PROD             1
#define OD_CNT_HB_CONS             1
#define OD_CNT_RPDO                4
#define OD_CNT_TPDO                4

// Runtime variables for OD entries
typedef struct {
    // --- CiA 301 communication area ---
    uint8_t  x1001_errorRegister;
    uint16_t x1017_producerHeartbeatTime;

    // --- Manufacturer-specific area ---
    // motor
    int32_t x2000_motor_target_position[4];
    int32_t x2001_motor_actual_position[4];
    uint32_t x2002_motor_speed[4];
    uint8_t x2003_motor_command_word;
    uint8_t x2004_motor_status_word[4];
    uint8_t x2005_motor_enable[4];
    uint32_t x2006_motor_acceleration[4];
    uint8_t x2007_motor_is_absolute[4];
    int32_t x2008_motor_min_position[4];
    int32_t x2009_motor_max_position[4];
    uint32_t x200A_motor_jerk[4];

    // homing
    uint8_t x2010_homing_command[4];
    uint32_t x2011_homing_speed[4];
    int8_t x2012_homing_direction[4];
    uint32_t x2013_homing_timeout[4];
    int32_t x2014_homing_endstop_release[4];
    uint8_t x2015_homing_endstop_polarity[4];

    // tmc
    uint16_t x2020_tmc_microsteps[4];
    uint16_t x2021_tmc_rms_current[4];
    uint8_t x2022_tmc_stallguard_threshold[4];
    uint8_t x2023_tmc_coolstep_semin[4];
    uint8_t x2024_tmc_coolstep_semax[4];
    uint8_t x2025_tmc_blank_time[4];
    uint8_t x2026_tmc_toff[4];
    uint32_t x2027_tmc_stall_count[4];

    // laser
    uint16_t x2100_laser_pwm_value[4];
    uint16_t x2101_laser_max_value[4];
    uint32_t x2102_laser_pwm_frequency[4];
    uint8_t x2103_laser_pwm_resolution[4];
    uint32_t x2104_laser_despeckle_period[4];
    uint16_t x2105_laser_despeckle_amplitude[4];
    uint8_t x2106_laser_safety_state;

    // led
    uint8_t x2200_led_array_mode;
    uint8_t x2201_led_brightness;
    uint32_t x2202_led_uniform_colour;
    uint16_t x2203_led_pixel_count;
    uint8_t x2204_led_layout_width;
    uint8_t x2205_led_layout_height;
    uint8_t* x2210_led_pixel_data;  // DOMAIN — pointer set at runtime
    uint8_t x2220_led_pattern_id;
    uint16_t x2221_led_pattern_speed;

    // digital_io
    uint8_t x2300_digital_input_state[8];
    uint8_t x2301_digital_output_command[8];
    uint8_t x2302_digital_input_change_mask;

    // analog_io
    uint16_t x2310_analog_input_value[8];
    uint16_t x2311_analog_input_filtered[8];
    uint16_t x2320_dac_output_value[4];

    // encoder
    int32_t x2340_encoder_position[4];
    int32_t x2341_encoder_velocity[4];
    int32_t x2342_encoder_zero_offset[4];

    // joystick
    int16_t x2400_joystick_axis[4];
    uint16_t x2401_joystick_buttons;
    uint8_t x2402_joystick_speed_multiplier;
    uint16_t x2403_joystick_deadzone;

    // system
    char x2500_firmware_version_string[32];
    char x2501_board_name[32];
    uint32_t x2502_enabled_modules_bitmask;
    uint32_t x2503_uptime_seconds;
    uint32_t x2504_free_heap_bytes;
    uint32_t x2505_can_error_counter;
    int16_t x2506_cpu_temperature;
    uint8_t x2507_reboot_command;

    // galvo
    int32_t x2600_galvo_target_position[2];
    int32_t x2601_galvo_actual_position[2];
    uint8_t x2602_galvo_command_word;
    uint8_t x2603_galvo_status_word;
    uint32_t x2604_galvo_scan_speed;
    uint16_t x2605_galvo_n_steps_line;
    uint16_t x2606_galvo_n_steps_pixel;
    uint16_t x2607_galvo_d_steps_line;
    uint16_t x2608_galvo_d_steps_pixel;
    uint32_t x2609_galvo_t_pre_us;
    uint32_t x260A_galvo_t_post_us;
    int32_t x260B_galvo_x_start;
    int32_t x260C_galvo_y_start;
    int32_t x260D_galvo_x_step;
    int32_t x260E_galvo_y_step;
    uint8_t x260F_galvo_camera_trigger_mode;

    // pid
    int32_t x2700_pid_setpoint;
    int32_t x2701_pid_actual_value;
    uint32_t x2702_pid_kp;
    uint32_t x2703_pid_ki;
    uint32_t x2704_pid_kd;
    uint8_t x2705_pid_enable;

    // ota
    uint8_t* x2F00_ota_firmware_data;  // DOMAIN — pointer set at runtime
    uint32_t x2F01_ota_firmware_size;
    uint32_t x2F02_ota_firmware_crc32;
    uint8_t x2F03_ota_status;
    uint32_t x2F04_ota_bytes_received;
    uint8_t x2F05_ota_error_code;

} OD_RAM_t;

extern OD_RAM_t OD_RAM;
extern OD_t *OD;

// PDO mapping helper macros (for syncRpdoToModules / syncModulesToTpdo)
#define OD_MOTOR_TARGET_POSITION                         OD_RAM.x2000_motor_target_position
#define OD_MOTOR_ACTUAL_POSITION                         OD_RAM.x2001_motor_actual_position
#define OD_MOTOR_SPEED                                   OD_RAM.x2002_motor_speed
#define OD_MOTOR_COMMAND_WORD                            OD_RAM.x2003_motor_command_word
#define OD_MOTOR_STATUS_WORD                             OD_RAM.x2004_motor_status_word
#define OD_MOTOR_ENABLE                                  OD_RAM.x2005_motor_enable
#define OD_MOTOR_ACCELERATION                            OD_RAM.x2006_motor_acceleration
#define OD_MOTOR_IS_ABSOLUTE                             OD_RAM.x2007_motor_is_absolute
#define OD_MOTOR_MIN_POSITION                            OD_RAM.x2008_motor_min_position
#define OD_MOTOR_MAX_POSITION                            OD_RAM.x2009_motor_max_position
#define OD_MOTOR_JERK                                    OD_RAM.x200A_motor_jerk
#define OD_HOMING_COMMAND                                OD_RAM.x2010_homing_command
#define OD_HOMING_SPEED                                  OD_RAM.x2011_homing_speed
#define OD_HOMING_DIRECTION                              OD_RAM.x2012_homing_direction
#define OD_HOMING_TIMEOUT                                OD_RAM.x2013_homing_timeout
#define OD_HOMING_ENDSTOP_RELEASE                        OD_RAM.x2014_homing_endstop_release
#define OD_HOMING_ENDSTOP_POLARITY                       OD_RAM.x2015_homing_endstop_polarity
#define OD_TMC_MICROSTEPS                                OD_RAM.x2020_tmc_microsteps
#define OD_TMC_RMS_CURRENT                               OD_RAM.x2021_tmc_rms_current
#define OD_TMC_STALLGUARD_THRESHOLD                      OD_RAM.x2022_tmc_stallguard_threshold
#define OD_TMC_COOLSTEP_SEMIN                            OD_RAM.x2023_tmc_coolstep_semin
#define OD_TMC_COOLSTEP_SEMAX                            OD_RAM.x2024_tmc_coolstep_semax
#define OD_TMC_BLANK_TIME                                OD_RAM.x2025_tmc_blank_time
#define OD_TMC_TOFF                                      OD_RAM.x2026_tmc_toff
#define OD_TMC_STALL_COUNT                               OD_RAM.x2027_tmc_stall_count
#define OD_LASER_PWM_VALUE                               OD_RAM.x2100_laser_pwm_value
#define OD_LASER_MAX_VALUE                               OD_RAM.x2101_laser_max_value
#define OD_LASER_PWM_FREQUENCY                           OD_RAM.x2102_laser_pwm_frequency
#define OD_LASER_PWM_RESOLUTION                          OD_RAM.x2103_laser_pwm_resolution
#define OD_LASER_DESPECKLE_PERIOD                        OD_RAM.x2104_laser_despeckle_period
#define OD_LASER_DESPECKLE_AMPLITUDE                     OD_RAM.x2105_laser_despeckle_amplitude
#define OD_LASER_SAFETY_STATE                            OD_RAM.x2106_laser_safety_state
#define OD_LED_ARRAY_MODE                                OD_RAM.x2200_led_array_mode
#define OD_LED_BRIGHTNESS                                OD_RAM.x2201_led_brightness
#define OD_LED_UNIFORM_COLOUR                            OD_RAM.x2202_led_uniform_colour
#define OD_LED_PIXEL_COUNT                               OD_RAM.x2203_led_pixel_count
#define OD_LED_LAYOUT_WIDTH                              OD_RAM.x2204_led_layout_width
#define OD_LED_LAYOUT_HEIGHT                             OD_RAM.x2205_led_layout_height
#define OD_LED_PIXEL_DATA                                OD_RAM.x2210_led_pixel_data
#define OD_LED_PATTERN_ID                                OD_RAM.x2220_led_pattern_id
#define OD_LED_PATTERN_SPEED                             OD_RAM.x2221_led_pattern_speed
#define OD_DIGITAL_INPUT_STATE                           OD_RAM.x2300_digital_input_state
#define OD_DIGITAL_OUTPUT_COMMAND                        OD_RAM.x2301_digital_output_command
#define OD_DIGITAL_INPUT_CHANGE_MASK                     OD_RAM.x2302_digital_input_change_mask
#define OD_ANALOG_INPUT_VALUE                            OD_RAM.x2310_analog_input_value
#define OD_ANALOG_INPUT_FILTERED                         OD_RAM.x2311_analog_input_filtered
#define OD_DAC_OUTPUT_VALUE                              OD_RAM.x2320_dac_output_value
#define OD_ENCODER_POSITION                              OD_RAM.x2340_encoder_position
#define OD_ENCODER_VELOCITY                              OD_RAM.x2341_encoder_velocity
#define OD_ENCODER_ZERO_OFFSET                           OD_RAM.x2342_encoder_zero_offset
#define OD_JOYSTICK_AXIS                                 OD_RAM.x2400_joystick_axis
#define OD_JOYSTICK_BUTTONS                              OD_RAM.x2401_joystick_buttons
#define OD_JOYSTICK_SPEED_MULTIPLIER                     OD_RAM.x2402_joystick_speed_multiplier
#define OD_JOYSTICK_DEADZONE                             OD_RAM.x2403_joystick_deadzone
#define OD_FIRMWARE_VERSION_STRING                       OD_RAM.x2500_firmware_version_string
#define OD_BOARD_NAME                                    OD_RAM.x2501_board_name
#define OD_ENABLED_MODULES_BITMASK                       OD_RAM.x2502_enabled_modules_bitmask
#define OD_UPTIME_SECONDS                                OD_RAM.x2503_uptime_seconds
#define OD_FREE_HEAP_BYTES                               OD_RAM.x2504_free_heap_bytes
#define OD_CAN_ERROR_COUNTER                             OD_RAM.x2505_can_error_counter
#define OD_CPU_TEMPERATURE                               OD_RAM.x2506_cpu_temperature
#define OD_REBOOT_COMMAND                                OD_RAM.x2507_reboot_command
#define OD_GALVO_TARGET_POSITION                         OD_RAM.x2600_galvo_target_position
#define OD_GALVO_ACTUAL_POSITION                         OD_RAM.x2601_galvo_actual_position
#define OD_GALVO_COMMAND_WORD                            OD_RAM.x2602_galvo_command_word
#define OD_GALVO_STATUS_WORD                             OD_RAM.x2603_galvo_status_word
#define OD_GALVO_SCAN_SPEED                              OD_RAM.x2604_galvo_scan_speed
#define OD_GALVO_N_STEPS_LINE                            OD_RAM.x2605_galvo_n_steps_line
#define OD_GALVO_N_STEPS_PIXEL                           OD_RAM.x2606_galvo_n_steps_pixel
#define OD_GALVO_D_STEPS_LINE                            OD_RAM.x2607_galvo_d_steps_line
#define OD_GALVO_D_STEPS_PIXEL                           OD_RAM.x2608_galvo_d_steps_pixel
#define OD_GALVO_T_PRE_US                                OD_RAM.x2609_galvo_t_pre_us
#define OD_GALVO_T_POST_US                               OD_RAM.x260A_galvo_t_post_us
#define OD_GALVO_X_START                                 OD_RAM.x260B_galvo_x_start
#define OD_GALVO_Y_START                                 OD_RAM.x260C_galvo_y_start
#define OD_GALVO_X_STEP                                  OD_RAM.x260D_galvo_x_step
#define OD_GALVO_Y_STEP                                  OD_RAM.x260E_galvo_y_step
#define OD_GALVO_CAMERA_TRIGGER_MODE                     OD_RAM.x260F_galvo_camera_trigger_mode
#define OD_PID_SETPOINT                                  OD_RAM.x2700_pid_setpoint
#define OD_PID_ACTUAL_VALUE                              OD_RAM.x2701_pid_actual_value
#define OD_PID_KP                                        OD_RAM.x2702_pid_kp
#define OD_PID_KI                                        OD_RAM.x2703_pid_ki
#define OD_PID_KD                                        OD_RAM.x2704_pid_kd
#define OD_PID_ENABLE                                    OD_RAM.x2705_pid_enable
#define OD_OTA_FIRMWARE_DATA                             OD_RAM.x2F00_ota_firmware_data
#define OD_OTA_FIRMWARE_SIZE                             OD_RAM.x2F01_ota_firmware_size
#define OD_OTA_FIRMWARE_CRC32                            OD_RAM.x2F02_ota_firmware_crc32
#define OD_OTA_STATUS                                    OD_RAM.x2F03_ota_status
#define OD_OTA_BYTES_RECEIVED                            OD_RAM.x2F04_ota_bytes_received
#define OD_OTA_ERROR_CODE                                OD_RAM.x2F05_ota_error_code

#endif // OD_H
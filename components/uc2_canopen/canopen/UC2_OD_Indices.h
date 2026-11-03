// AUTO-GENERATED from uc2_canopen_registry.yaml — DO NOT EDIT
// Regenerate with: python tools/regenerate_all.py
//
// This file provides named constants for all UC2 CANopen OD entries.
// Use these instead of magic numbers like 0x6401:01.
//
// Example:
//   CANopenModule::writeSDO(nodeId,
//       UC2_OD::MOTOR_TARGET_POSITION, 1,  // index, sub
//       (uint8_t*)&pos, sizeof(int32_t));
//
// T4.4 4g: C compatibility — C callers use the UC2_OD_ prefix macros;
// C++ callers keep using the UC2_OD:: namespace shim.

#pragma once
#include <stdint.h>

// ── C API (macro constants, available to both C and C++) ───────────────────
#define UC2_OD_MOTOR_TARGET_POSITION                    0x2000
#define UC2_OD_MOTOR_ACTUAL_POSITION                    0x2001
#define UC2_OD_MOTOR_SPEED                              0x2002
#define UC2_OD_MOTOR_COMMAND_WORD                       0x2003
#define UC2_OD_MOTOR_STATUS_WORD                        0x2004
#define UC2_OD_MOTOR_ENABLE                             0x2005
#define UC2_OD_MOTOR_ACCELERATION                       0x2006
#define UC2_OD_MOTOR_IS_ABSOLUTE                        0x2007
#define UC2_OD_MOTOR_MIN_POSITION                       0x2008
#define UC2_OD_MOTOR_MAX_POSITION                       0x2009
#define UC2_OD_MOTOR_JERK                               0x200A
#define UC2_OD_MOTOR_IS_FOREVER                         0x200B

#define UC2_OD_HOMING_COMMAND                           0x2010
#define UC2_OD_HOMING_SPEED                             0x2011
#define UC2_OD_HOMING_DIRECTION                         0x2012
#define UC2_OD_HOMING_TIMEOUT                           0x2013
#define UC2_OD_HOMING_ENDSTOP_RELEASE                   0x2014
#define UC2_OD_HOMING_ENDSTOP_POLARITY                  0x2015
#define UC2_OD_HOMING_STATUS                            0x2016

#define UC2_OD_TMC_MICROSTEPS                           0x2020
#define UC2_OD_TMC_RMS_CURRENT                          0x2021
#define UC2_OD_TMC_STALLGUARD_THRESHOLD                 0x2022
#define UC2_OD_TMC_COOLSTEP_SEMIN                       0x2023
#define UC2_OD_TMC_COOLSTEP_SEMAX                       0x2024
#define UC2_OD_TMC_BLANK_TIME                           0x2025
#define UC2_OD_TMC_TOFF                                 0x2026
#define UC2_OD_TMC_STALL_COUNT                          0x2027

#define UC2_OD_HARDLIMIT_COMMAND                        0x2030
#define UC2_OD_HARDLIMIT_ENABLED                        0x2031
#define UC2_OD_HARDLIMIT_POLARITY                       0x2032

#define UC2_OD_AXIS_MEASURED_STEPS                      0x2040
#define UC2_OD_AXIS_POSITION_ERROR_STEPS                0x2041
#define UC2_OD_AXIS_MODE                                0x2042
#define UC2_OD_AXIS_HEALTH                              0x2043
#define UC2_OD_AXIS_FAULT                               0x2044
#define UC2_OD_AXIS_RESET                               0x2045
#define UC2_OD_AXIS_CALIBRATED                          0x2046
#define UC2_OD_AXIS_REFERENCED                          0x2047
#define UC2_OD_AXIS_CALIBRATE                           0x2048
#define UC2_OD_AXIS_COUNTS_PER_STEP_Q16                 0x2049
#define UC2_OD_AXIS_BACKLASH_STEPS                      0x204A
#define UC2_OD_AXIS_RAW_COUNTS                          0x204B

#define UC2_OD_LASER_PWM_VALUE                          0x2100
#define UC2_OD_LASER_MAX_VALUE                          0x2101
#define UC2_OD_LASER_PWM_FREQUENCY                      0x2102
#define UC2_OD_LASER_PWM_RESOLUTION                     0x2103
#define UC2_OD_LASER_DESPECKLE_PERIOD                   0x2104
#define UC2_OD_LASER_DESPECKLE_AMPLITUDE                0x2105
#define UC2_OD_LASER_SAFETY_STATE                       0x2106

#define UC2_OD_LED_ARRAY_MODE                           0x2200
#define UC2_OD_LED_BRIGHTNESS                           0x2201
#define UC2_OD_LED_UNIFORM_COLOUR                       0x2202
#define UC2_OD_LED_PIXEL_COUNT                          0x2203
#define UC2_OD_LED_LAYOUT_WIDTH                         0x2204
#define UC2_OD_LED_LAYOUT_HEIGHT                        0x2205
#define UC2_OD_LED_PIXEL_DATA                           0x2210
#define UC2_OD_LED_SINGLE_PIXEL                         0x2211
#define UC2_OD_LED_SHAPE                                0x2212
#define UC2_OD_LED_PATTERN_ID                           0x2220
#define UC2_OD_LED_PATTERN_SPEED                        0x2221

#define UC2_OD_DIGITAL_INPUT_STATE                      0x2300
#define UC2_OD_DIGITAL_OUTPUT_COMMAND                   0x2301
#define UC2_OD_DIGITAL_INPUT_CHANGE_MASK                0x2302

#define UC2_OD_ANALOG_INPUT_VALUE                       0x2310
#define UC2_OD_ANALOG_INPUT_FILTERED                    0x2311
#define UC2_OD_DAC_OUTPUT_VALUE                         0x2320

#define UC2_OD_COLLISION_REFERENCE                      0x2330
#define UC2_OD_COLLISION_THRESHOLD                      0x2331
#define UC2_OD_COLLISION_SENSITIVITY                    0x2332
#define UC2_OD_COLLISION_COMMAND                        0x2333
#define UC2_OD_COLLISION_MEAN                           0x2334
#define UC2_OD_COLLISION_MODE                           0x2335
#define UC2_OD_COLLISION_SIGMA                          0x2336

#define UC2_OD_ENCODER_POSITION                         0x2340
#define UC2_OD_ENCODER_VELOCITY                         0x2341
#define UC2_OD_ENCODER_ZERO_OFFSET                      0x2342

#define UC2_OD_I2C_COMMAND                              0x2350
#define UC2_OD_I2C_TRIGGER                              0x2351
#define UC2_OD_I2C_STATUS                               0x2352
#define UC2_OD_I2C_RESPONSE                             0x2353
#define UC2_OD_I2C_RESP_LEN                             0x2354

#define UC2_OD_JOYSTICK_AXIS                            0x2400
#define UC2_OD_JOYSTICK_BUTTONS                         0x2401
#define UC2_OD_JOYSTICK_SPEED_MULTIPLIER                0x2402
#define UC2_OD_JOYSTICK_DEADZONE                        0x2403

#define UC2_OD_FIRMWARE_VERSION_STRING                  0x2500
#define UC2_OD_BOARD_NAME                               0x2501
#define UC2_OD_ENABLED_MODULES_BITMASK                  0x2502
#define UC2_OD_UPTIME_SECONDS                           0x2503
#define UC2_OD_FREE_HEAP_BYTES                          0x2504
#define UC2_OD_CAN_ERROR_COUNTER                        0x2505
#define UC2_OD_CPU_TEMPERATURE                          0x2506
#define UC2_OD_REBOOT_COMMAND                           0x2507
#define UC2_OD_BUILD_TIMESTAMP                          0x2508
#define UC2_OD_MAC_ADDRESS                              0x2509
#define UC2_OD_COMMANDED_NODE_ID                        0x250A

#define UC2_OD_GALVO_TARGET_POSITION                    0x2600
#define UC2_OD_GALVO_ACTUAL_POSITION                    0x2601
#define UC2_OD_GALVO_COMMAND_WORD                       0x2602
#define UC2_OD_GALVO_STATUS_WORD                        0x2603
#define UC2_OD_GALVO_SCAN_SPEED                         0x2604
#define UC2_OD_GALVO_N_STEPS_LINE                       0x2605
#define UC2_OD_GALVO_N_STEPS_PIXEL                      0x2606
#define UC2_OD_GALVO_D_STEPS_LINE                       0x2607
#define UC2_OD_GALVO_D_STEPS_PIXEL                      0x2608
#define UC2_OD_GALVO_T_PRE_US                           0x2609
#define UC2_OD_GALVO_T_POST_US                          0x260A
#define UC2_OD_GALVO_X_START                            0x260B
#define UC2_OD_GALVO_Y_START                            0x260C
#define UC2_OD_GALVO_X_STEP                             0x260D
#define UC2_OD_GALVO_Y_STEP                             0x260E
#define UC2_OD_GALVO_CAMERA_TRIGGER_MODE                0x260F
#define UC2_OD_GALVO_POINTS_DATA                        0x2610

#define UC2_OD_PID_SETPOINT                             0x2700
#define UC2_OD_PID_ACTUAL_VALUE                         0x2701
#define UC2_OD_PID_KP                                   0x2702
#define UC2_OD_PID_KI                                   0x2703
#define UC2_OD_PID_KD                                   0x2704
#define UC2_OD_PID_ENABLE                               0x2705

#define UC2_OD_OTA_FIRMWARE_DATA                        0x2F00
#define UC2_OD_OTA_FIRMWARE_SIZE                        0x2F01
#define UC2_OD_OTA_FIRMWARE_CRC32                       0x2F02
#define UC2_OD_OTA_STATUS                               0x2F03
#define UC2_OD_OTA_BYTES_RECEIVED                       0x2F04
#define UC2_OD_OTA_ERROR_CODE                           0x2F05

// Node-ID assignments (C macros)
#define UC2_NODE_MASTER               1
#define UC2_NODE_MOTOR_A              14
#define UC2_NODE_MOTOR_X              11
#define UC2_NODE_MOTOR_Y              12
#define UC2_NODE_MOTOR_Z              13
#define UC2_NODE_LED                  20
#define UC2_NODE_LASER                21
#define UC2_NODE_JOYSTICK             22
#define UC2_NODE_GALVO                30
#define UC2_NODE_GALVO_2              31
#define UC2_NODE_ENCODER              40
#define UC2_NODE_PID                  50

#ifdef __cplusplus
// ── C++ shim (preserves UC2_OD:: / UC2_NODE:: namespace API) ──────────────
namespace UC2_OD {
#define OD_CONST(name) constexpr uint16_t name = UC2_OD_##name;
OD_CONST(MOTOR_TARGET_POSITION)
OD_CONST(MOTOR_ACTUAL_POSITION)
OD_CONST(MOTOR_SPEED)
OD_CONST(MOTOR_COMMAND_WORD)
OD_CONST(MOTOR_STATUS_WORD)
OD_CONST(MOTOR_ENABLE)
OD_CONST(MOTOR_ACCELERATION)
OD_CONST(MOTOR_IS_ABSOLUTE)
OD_CONST(MOTOR_MIN_POSITION)
OD_CONST(MOTOR_MAX_POSITION)
OD_CONST(MOTOR_JERK)
OD_CONST(MOTOR_IS_FOREVER)

OD_CONST(HOMING_COMMAND)
OD_CONST(HOMING_SPEED)
OD_CONST(HOMING_DIRECTION)
OD_CONST(HOMING_TIMEOUT)
OD_CONST(HOMING_ENDSTOP_RELEASE)
OD_CONST(HOMING_ENDSTOP_POLARITY)
OD_CONST(HOMING_STATUS)

OD_CONST(TMC_MICROSTEPS)
OD_CONST(TMC_RMS_CURRENT)
OD_CONST(TMC_STALLGUARD_THRESHOLD)
OD_CONST(TMC_COOLSTEP_SEMIN)
OD_CONST(TMC_COOLSTEP_SEMAX)
OD_CONST(TMC_BLANK_TIME)
OD_CONST(TMC_TOFF)
OD_CONST(TMC_STALL_COUNT)

OD_CONST(HARDLIMIT_COMMAND)
OD_CONST(HARDLIMIT_ENABLED)
OD_CONST(HARDLIMIT_POLARITY)

OD_CONST(AXIS_MEASURED_STEPS)
OD_CONST(AXIS_POSITION_ERROR_STEPS)
OD_CONST(AXIS_MODE)
OD_CONST(AXIS_HEALTH)
OD_CONST(AXIS_FAULT)
OD_CONST(AXIS_RESET)
OD_CONST(AXIS_CALIBRATED)
OD_CONST(AXIS_REFERENCED)
OD_CONST(AXIS_CALIBRATE)
OD_CONST(AXIS_COUNTS_PER_STEP_Q16)
OD_CONST(AXIS_BACKLASH_STEPS)
OD_CONST(AXIS_RAW_COUNTS)

OD_CONST(LASER_PWM_VALUE)
OD_CONST(LASER_MAX_VALUE)
OD_CONST(LASER_PWM_FREQUENCY)
OD_CONST(LASER_PWM_RESOLUTION)
OD_CONST(LASER_DESPECKLE_PERIOD)
OD_CONST(LASER_DESPECKLE_AMPLITUDE)
OD_CONST(LASER_SAFETY_STATE)

OD_CONST(LED_ARRAY_MODE)
OD_CONST(LED_BRIGHTNESS)
OD_CONST(LED_UNIFORM_COLOUR)
OD_CONST(LED_PIXEL_COUNT)
OD_CONST(LED_LAYOUT_WIDTH)
OD_CONST(LED_LAYOUT_HEIGHT)
OD_CONST(LED_PIXEL_DATA)
OD_CONST(LED_SINGLE_PIXEL)
OD_CONST(LED_SHAPE)
OD_CONST(LED_PATTERN_ID)
OD_CONST(LED_PATTERN_SPEED)

OD_CONST(DIGITAL_INPUT_STATE)
OD_CONST(DIGITAL_OUTPUT_COMMAND)
OD_CONST(DIGITAL_INPUT_CHANGE_MASK)

OD_CONST(ANALOG_INPUT_VALUE)
OD_CONST(ANALOG_INPUT_FILTERED)
OD_CONST(DAC_OUTPUT_VALUE)

OD_CONST(COLLISION_REFERENCE)
OD_CONST(COLLISION_THRESHOLD)
OD_CONST(COLLISION_SENSITIVITY)
OD_CONST(COLLISION_COMMAND)
OD_CONST(COLLISION_MEAN)
OD_CONST(COLLISION_MODE)
OD_CONST(COLLISION_SIGMA)

OD_CONST(ENCODER_POSITION)
OD_CONST(ENCODER_VELOCITY)
OD_CONST(ENCODER_ZERO_OFFSET)

OD_CONST(I2C_COMMAND)
OD_CONST(I2C_TRIGGER)
OD_CONST(I2C_STATUS)
OD_CONST(I2C_RESPONSE)
OD_CONST(I2C_RESP_LEN)

OD_CONST(JOYSTICK_AXIS)
OD_CONST(JOYSTICK_BUTTONS)
OD_CONST(JOYSTICK_SPEED_MULTIPLIER)
OD_CONST(JOYSTICK_DEADZONE)

OD_CONST(FIRMWARE_VERSION_STRING)
OD_CONST(BOARD_NAME)
OD_CONST(ENABLED_MODULES_BITMASK)
OD_CONST(UPTIME_SECONDS)
OD_CONST(FREE_HEAP_BYTES)
OD_CONST(CAN_ERROR_COUNTER)
OD_CONST(CPU_TEMPERATURE)
OD_CONST(REBOOT_COMMAND)
OD_CONST(BUILD_TIMESTAMP)
OD_CONST(MAC_ADDRESS)
OD_CONST(COMMANDED_NODE_ID)

OD_CONST(GALVO_TARGET_POSITION)
OD_CONST(GALVO_ACTUAL_POSITION)
OD_CONST(GALVO_COMMAND_WORD)
OD_CONST(GALVO_STATUS_WORD)
OD_CONST(GALVO_SCAN_SPEED)
OD_CONST(GALVO_N_STEPS_LINE)
OD_CONST(GALVO_N_STEPS_PIXEL)
OD_CONST(GALVO_D_STEPS_LINE)
OD_CONST(GALVO_D_STEPS_PIXEL)
OD_CONST(GALVO_T_PRE_US)
OD_CONST(GALVO_T_POST_US)
OD_CONST(GALVO_X_START)
OD_CONST(GALVO_Y_START)
OD_CONST(GALVO_X_STEP)
OD_CONST(GALVO_Y_STEP)
OD_CONST(GALVO_CAMERA_TRIGGER_MODE)
OD_CONST(GALVO_POINTS_DATA)

OD_CONST(PID_SETPOINT)
OD_CONST(PID_ACTUAL_VALUE)
OD_CONST(PID_KP)
OD_CONST(PID_KI)
OD_CONST(PID_KD)
OD_CONST(PID_ENABLE)

OD_CONST(OTA_FIRMWARE_DATA)
OD_CONST(OTA_FIRMWARE_SIZE)
OD_CONST(OTA_FIRMWARE_CRC32)
OD_CONST(OTA_STATUS)
OD_CONST(OTA_BYTES_RECEIVED)
OD_CONST(OTA_ERROR_CODE)
}  // namespace UC2_OD

namespace UC2_NODE {
constexpr uint8_t MASTER   = UC2_NODE_MASTER;
constexpr uint8_t MOTOR_A  = UC2_NODE_MOTOR_A;
constexpr uint8_t MOTOR_X  = UC2_NODE_MOTOR_X;
constexpr uint8_t MOTOR_Y  = UC2_NODE_MOTOR_Y;
constexpr uint8_t MOTOR_Z  = UC2_NODE_MOTOR_Z;
constexpr uint8_t LED      = UC2_NODE_LED;
constexpr uint8_t LASER    = UC2_NODE_LASER;
constexpr uint8_t JOYSTICK = UC2_NODE_JOYSTICK;
constexpr uint8_t GALVO    = UC2_NODE_GALVO;
constexpr uint8_t GALVO_2  = UC2_NODE_GALVO_2;
constexpr uint8_t ENCODER  = UC2_NODE_ENCODER;
constexpr uint8_t PID      = UC2_NODE_PID;
}  // namespace UC2_NODE
#endif

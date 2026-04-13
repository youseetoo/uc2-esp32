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

#pragma once
#include <stdint.h>

namespace UC2_OD {

// ─── MOTOR (base 0x2000) ───
// Stepper motor control with TMC2209 driver, up to 4 axes per node

constexpr uint16_t MOTOR_TARGET_POSITION                    = 0x2000;
constexpr uint16_t MOTOR_ACTUAL_POSITION                    = 0x2001;
constexpr uint16_t MOTOR_SPEED                              = 0x2002;
constexpr uint16_t MOTOR_COMMAND_WORD                       = 0x2003;
constexpr uint16_t MOTOR_STATUS_WORD                        = 0x2004;
constexpr uint16_t MOTOR_ENABLE                             = 0x2005;
constexpr uint16_t MOTOR_ACCELERATION                       = 0x2006;
constexpr uint16_t MOTOR_IS_ABSOLUTE                        = 0x2007;
constexpr uint16_t MOTOR_MIN_POSITION                       = 0x2008;
constexpr uint16_t MOTOR_MAX_POSITION                       = 0x2009;
constexpr uint16_t MOTOR_JERK                               = 0x200A;
constexpr uint16_t MOTOR_IS_FOREVER                         = 0x200B;

// ─── HOMING (base 0x2010) ───
// Sensorless or endstop-based homing

constexpr uint16_t HOMING_COMMAND                           = 0x2010;
constexpr uint16_t HOMING_SPEED                             = 0x2011;
constexpr uint16_t HOMING_DIRECTION                         = 0x2012;
constexpr uint16_t HOMING_TIMEOUT                           = 0x2013;
constexpr uint16_t HOMING_ENDSTOP_RELEASE                   = 0x2014;
constexpr uint16_t HOMING_ENDSTOP_POLARITY                  = 0x2015;

// ─── TMC (base 0x2020) ───
// TMC2209 silent stepper driver configuration

constexpr uint16_t TMC_MICROSTEPS                           = 0x2020;
constexpr uint16_t TMC_RMS_CURRENT                          = 0x2021;
constexpr uint16_t TMC_STALLGUARD_THRESHOLD                 = 0x2022;
constexpr uint16_t TMC_COOLSTEP_SEMIN                       = 0x2023;
constexpr uint16_t TMC_COOLSTEP_SEMAX                       = 0x2024;
constexpr uint16_t TMC_BLANK_TIME                           = 0x2025;
constexpr uint16_t TMC_TOFF                                 = 0x2026;
constexpr uint16_t TMC_STALL_COUNT                          = 0x2027;

// ─── LASER (base 0x2100) ───
// Laser intensity control via PWM, up to 4 channels per node

constexpr uint16_t LASER_PWM_VALUE                          = 0x2100;
constexpr uint16_t LASER_MAX_VALUE                          = 0x2101;
constexpr uint16_t LASER_PWM_FREQUENCY                      = 0x2102;
constexpr uint16_t LASER_PWM_RESOLUTION                     = 0x2103;
constexpr uint16_t LASER_DESPECKLE_PERIOD                   = 0x2104;
constexpr uint16_t LASER_DESPECKLE_AMPLITUDE                = 0x2105;
constexpr uint16_t LASER_SAFETY_STATE                       = 0x2106;

// ─── LED (base 0x2200) ───
// Addressable LED array (NeoPixel) with pattern support

constexpr uint16_t LED_ARRAY_MODE                           = 0x2200;
constexpr uint16_t LED_BRIGHTNESS                           = 0x2201;
constexpr uint16_t LED_UNIFORM_COLOUR                       = 0x2202;
constexpr uint16_t LED_PIXEL_COUNT                          = 0x2203;
constexpr uint16_t LED_LAYOUT_WIDTH                         = 0x2204;
constexpr uint16_t LED_LAYOUT_HEIGHT                        = 0x2205;
constexpr uint16_t LED_PIXEL_DATA                           = 0x2210;
constexpr uint16_t LED_PATTERN_ID                           = 0x2220;
constexpr uint16_t LED_PATTERN_SPEED                        = 0x2221;

// ─── DIGITAL_IO (base 0x2300) ───
// Endstops, triggers, generic GPIOs

constexpr uint16_t DIGITAL_INPUT_STATE                      = 0x2300;
constexpr uint16_t DIGITAL_OUTPUT_COMMAND                   = 0x2301;
constexpr uint16_t DIGITAL_INPUT_CHANGE_MASK                = 0x2302;

// ─── ANALOG_IO (base 0x2310) ───
// ADC inputs, DAC outputs

constexpr uint16_t ANALOG_INPUT_VALUE                       = 0x2310;
constexpr uint16_t ANALOG_INPUT_FILTERED                    = 0x2311;
constexpr uint16_t DAC_OUTPUT_VALUE                         = 0x2320;

// ─── ENCODER (base 0x2340) ───
// Quadrature or magnetic (AS5600) encoder feedback

constexpr uint16_t ENCODER_POSITION                         = 0x2340;
constexpr uint16_t ENCODER_VELOCITY                         = 0x2341;
constexpr uint16_t ENCODER_ZERO_OFFSET                      = 0x2342;

// ─── JOYSTICK (base 0x2400) ───
// Analog joystick or PSX gamepad bridge

constexpr uint16_t JOYSTICK_AXIS                            = 0x2400;
constexpr uint16_t JOYSTICK_BUTTONS                         = 0x2401;
constexpr uint16_t JOYSTICK_SPEED_MULTIPLIER                = 0x2402;
constexpr uint16_t JOYSTICK_DEADZONE                        = 0x2403;

// ─── SYSTEM (base 0x2500) ───
// Firmware version, board info, runtime stats

constexpr uint16_t FIRMWARE_VERSION_STRING                  = 0x2500;
constexpr uint16_t BOARD_NAME                               = 0x2501;
constexpr uint16_t ENABLED_MODULES_BITMASK                  = 0x2502;
constexpr uint16_t UPTIME_SECONDS                           = 0x2503;
constexpr uint16_t FREE_HEAP_BYTES                          = 0x2504;
constexpr uint16_t CAN_ERROR_COUNTER                        = 0x2505;
constexpr uint16_t CPU_TEMPERATURE                          = 0x2506;
constexpr uint16_t REBOOT_COMMAND                           = 0x2507;

// ─── GALVO (base 0x2600) ───
// Galvo mirrors with raster/line scanning

constexpr uint16_t GALVO_TARGET_POSITION                    = 0x2600;
constexpr uint16_t GALVO_ACTUAL_POSITION                    = 0x2601;
constexpr uint16_t GALVO_COMMAND_WORD                       = 0x2602;
constexpr uint16_t GALVO_STATUS_WORD                        = 0x2603;
constexpr uint16_t GALVO_SCAN_SPEED                         = 0x2604;
constexpr uint16_t GALVO_N_STEPS_LINE                       = 0x2605;
constexpr uint16_t GALVO_N_STEPS_PIXEL                      = 0x2606;
constexpr uint16_t GALVO_D_STEPS_LINE                       = 0x2607;
constexpr uint16_t GALVO_D_STEPS_PIXEL                      = 0x2608;
constexpr uint16_t GALVO_T_PRE_US                           = 0x2609;
constexpr uint16_t GALVO_T_POST_US                          = 0x260A;
constexpr uint16_t GALVO_X_START                            = 0x260B;
constexpr uint16_t GALVO_Y_START                            = 0x260C;
constexpr uint16_t GALVO_X_STEP                             = 0x260D;
constexpr uint16_t GALVO_Y_STEP                             = 0x260E;
constexpr uint16_t GALVO_CAMERA_TRIGGER_MODE                = 0x260F;

// ─── PID (base 0x2700) ───
// Generic PID controller (e.g. for focus stabilization)

constexpr uint16_t PID_SETPOINT                             = 0x2700;
constexpr uint16_t PID_ACTUAL_VALUE                         = 0x2701;
constexpr uint16_t PID_KP                                   = 0x2702;
constexpr uint16_t PID_KI                                   = 0x2703;
constexpr uint16_t PID_KD                                   = 0x2704;
constexpr uint16_t PID_ENABLE                               = 0x2705;

// ─── OTA (base 0x2F00) ───
// Firmware update via SDO block transfer

constexpr uint16_t OTA_FIRMWARE_DATA                        = 0x2F00;
constexpr uint16_t OTA_FIRMWARE_SIZE                        = 0x2F01;
constexpr uint16_t OTA_FIRMWARE_CRC32                       = 0x2F02;
constexpr uint16_t OTA_STATUS                               = 0x2F03;
constexpr uint16_t OTA_BYTES_RECEIVED                       = 0x2F04;
constexpr uint16_t OTA_ERROR_CODE                           = 0x2F05;

}  // namespace UC2_OD

// Node-ID assignments
namespace UC2_NODE {
constexpr uint8_t MASTER               = 1;
constexpr uint8_t MOTOR_A              = 14;
constexpr uint8_t MOTOR_X              = 11;
constexpr uint8_t MOTOR_Y              = 12;
constexpr uint8_t MOTOR_Z              = 13;
constexpr uint8_t LED                  = 20;
constexpr uint8_t LASER                = 21;
constexpr uint8_t JOYSTICK             = 22;
constexpr uint8_t GALVO                = 30;
constexpr uint8_t GALVO_2              = 31;
constexpr uint8_t ENCODER              = 40;
constexpr uint8_t PID                  = 50;
}  // namespace UC2_NODE

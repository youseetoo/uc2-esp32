// AUTO-GENERATED/MAINTAINED for UC2 CANopen native OD (0x2000+ entries).
// Regenerate with: python tools/regenerate_all.py
// OR manually from DOCUMENTATION/openUC2_satellite.eds via CANopenEditor.
//
// Replaces the MWE trainer OD (0x6000/0x6200/0x6401).
// This header is consumed by both the application (CANopenModule.cpp)
// and the library (CANopen.c) for OD_CNT_* feature selection.

#ifndef OD_H
#define OD_H

#include "301/CO_ODinterface.h"
#include <stdint.h>

// ---------------------------------------------------------------------------
// Feature counts — consumed by CANopen.c to size internal objects
// ---------------------------------------------------------------------------
#define OD_CNT_NMT                 1
#define OD_CNT_EM                  1
#define OD_CNT_SYNC                1
#define OD_CNT_HB_PROD             1
#define OD_CNT_HB_CONS             1
#define OD_CNT_SDO_SRV             1  // 1 SDO server (always-on, slave)
#define OD_CNT_SDO_CLI             1  // 1 SDO client (master role)
#define OD_CNT_RPDO                4
#define OD_CNT_TPDO                4

// Array sizes
#define OD_CNT_ARR_1003           16  // pre-defined error field
#define OD_CNT_ARR_1010            4  // store parameters
#define OD_CNT_ARR_1011            4  // restore default parameters
#define OD_CNT_ARR_1016            8  // consumer heartbeat time

// ---------------------------------------------------------------------------
// Non-volatile communication area (CiA 301 standard + PDO configuration)
// ---------------------------------------------------------------------------
typedef struct {
    uint32_t x1000_deviceType;
    uint32_t x1005_COB_ID_SYNCMessage;
    uint32_t x1006_communicationCyclePeriod;
    uint32_t x1007_synchronousWindowLength;
    uint32_t x1012_COB_IDTimeStampObject;
    uint32_t x1014_COB_ID_EMCY;
    uint16_t x1015_inhibitTimeEMCY;
    uint8_t  x1016_consumerHeartbeatTime_sub0;
    uint32_t x1016_consumerHeartbeatTime[OD_CNT_ARR_1016];
    uint16_t x1017_producerHeartbeatTime;
    struct {
        uint8_t  highestSub_indexSupported;
        uint32_t vendor_ID;
        uint32_t productCode;
        uint32_t revisionNumber;
        uint32_t serialNumber;
    } x1018_identity;
    uint8_t  x1019_synchronousCounterOverflowValue;
    struct {
        uint8_t  highestSub_indexSupported;
        uint32_t COB_IDClientToServerTx;
        uint32_t COB_IDServerToClientRx;
        uint8_t  node_IDOfTheSDOServer;
    } x1280_SDOClientParameter;
    // RPDO communication parameters (0x1400-0x1403)
    struct { uint8_t highestSub_indexSupported; uint32_t COB_IDUsedByRPDO; uint8_t transmissionType; uint16_t eventTimer; } x1400_RPDOCommunicationParameter;
    struct { uint8_t highestSub_indexSupported; uint32_t COB_IDUsedByRPDO; uint8_t transmissionType; uint16_t eventTimer; } x1401_RPDOCommunicationParameter;
    struct { uint8_t highestSub_indexSupported; uint32_t COB_IDUsedByRPDO; uint8_t transmissionType; uint16_t eventTimer; } x1402_RPDOCommunicationParameter;
    struct { uint8_t highestSub_indexSupported; uint32_t COB_IDUsedByRPDO; uint8_t transmissionType; uint16_t eventTimer; } x1403_RPDOCommunicationParameter;
    // RPDO mapping parameters (0x1600-0x1603)
    struct { uint8_t n; uint32_t o1,o2,o3,o4,o5,o6,o7,o8; } x1600_RPDOMappingParameter;
    struct { uint8_t n; uint32_t o1,o2,o3,o4,o5,o6,o7,o8; } x1601_RPDOMappingParameter;
    struct { uint8_t n; uint32_t o1,o2,o3,o4,o5,o6,o7,o8; } x1602_RPDOMappingParameter;
    struct { uint8_t n; uint32_t o1,o2,o3,o4,o5,o6,o7,o8; } x1603_RPDOMappingParameter;
    // TPDO communication parameters (0x1800-0x1803)
    struct { uint8_t highestSub_indexSupported; uint32_t COB_IDUsedByTPDO; uint8_t transmissionType; uint16_t inhibitTime; uint16_t eventTimer; uint8_t SYNCStartValue; } x1800_TPDOCommunicationParameter;
    struct { uint8_t highestSub_indexSupported; uint32_t COB_IDUsedByTPDO; uint8_t transmissionType; uint16_t inhibitTime; uint16_t eventTimer; uint8_t SYNCStartValue; } x1801_TPDOCommunicationParameter;
    struct { uint8_t highestSub_indexSupported; uint32_t COB_IDUsedByTPDO; uint8_t transmissionType; uint16_t inhibitTime; uint16_t eventTimer; uint8_t SYNCStartValue; } x1802_TPDOCommunicationParameter;
    struct { uint8_t highestSub_indexSupported; uint32_t COB_IDUsedByTPDO; uint8_t transmissionType; uint16_t inhibitTime; uint16_t eventTimer; uint8_t SYNCStartValue; } x1803_TPDOCommunicationParameter;
    // TPDO mapping parameters (0x1A00-0x1A03)
    struct { uint8_t n; uint32_t o1,o2,o3,o4,o5,o6,o7,o8; } x1A00_TPDOMappingParameter;
    struct { uint8_t n; uint32_t o1,o2,o3,o4,o5,o6,o7,o8; } x1A01_TPDOMappingParameter;
    struct { uint8_t n; uint32_t o1,o2,o3,o4,o5,o6,o7,o8; } x1A02_TPDOMappingParameter;
    struct { uint8_t n; uint32_t o1,o2,o3,o4,o5,o6,o7,o8; } x1A03_TPDOMappingParameter;
} OD_PERSIST_COMM_t;

// ---------------------------------------------------------------------------
// Volatile runtime area — written by module code and the CANopenNode stack
// ---------------------------------------------------------------------------
typedef struct {
    // CiA 301 volatile entries
    uint8_t  x1001_errorRegister;
    uint8_t  x1003_pre_definedErrorField_sub0;
    uint32_t x1003_pre_definedErrorField[OD_CNT_ARR_1003];
    uint8_t  x1010_storeParameters_sub0;
    uint32_t x1010_storeParameters[OD_CNT_ARR_1010];
    uint8_t  x1011_restoreDefaultParameters_sub0;
    uint32_t x1011_restoreDefaultParameters[OD_CNT_ARR_1011];
    struct {
        uint8_t  highestSub_indexSupported;
        uint32_t COB_IDClientToServerRx;
        uint32_t COB_IDServerToClientTx;
    } x1200_SDOServerParameter;

    // --- UC2 Manufacturer area (0x2000+) ---
    // motor
    int32_t  x2000_motor_target_position[4];
    int32_t  x2001_motor_actual_position[4];
    uint32_t x2002_motor_speed[4];
    uint8_t  x2003_motor_command_word;
    uint8_t  x2004_motor_status_word[4];
    uint8_t  x2005_motor_enable[4];
    uint32_t x2006_motor_acceleration[4];
    uint8_t  x2007_motor_is_absolute[4];
    int32_t  x2008_motor_min_position[4];
    int32_t  x2009_motor_max_position[4];
    uint32_t x200A_motor_jerk[4];
    uint8_t  x200B_motor_is_forever[4];
    // homing
    uint8_t  x2010_homing_command[4];
    uint32_t x2011_homing_speed[4];
    int8_t   x2012_homing_direction[4];
    uint32_t x2013_homing_timeout[4];
    int32_t  x2014_homing_endstop_release[4];
    uint8_t  x2015_homing_endstop_polarity[4];
    // tmc
    uint16_t x2020_tmc_microsteps[4];
    uint16_t x2021_tmc_rms_current[4];
    uint8_t  x2022_tmc_stallguard_threshold[4];
    uint8_t  x2023_tmc_coolstep_semin[4];
    uint8_t  x2024_tmc_coolstep_semax[4];
    uint8_t  x2025_tmc_blank_time[4];
    uint8_t  x2026_tmc_toff[4];
    uint32_t x2027_tmc_stall_count[4];
    // laser
    uint16_t x2100_laser_pwm_value[4];
    uint16_t x2101_laser_max_value[4];
    uint32_t x2102_laser_pwm_frequency[4];
    uint8_t  x2103_laser_pwm_resolution[4];
    uint8_t  x2106_laser_safety_state;
    // led
    uint8_t  x2200_led_array_mode;
    uint8_t  x2201_led_brightness;
    uint32_t x2202_led_uniform_colour;
    uint16_t x2203_led_pixel_count;
    uint8_t  x2220_led_pattern_id;
    uint16_t x2221_led_pattern_speed;
    // digital_io
    uint8_t  x2300_digital_input_state[8];
    uint8_t  x2301_digital_output_command[8];
    // analog_io
    uint16_t x2310_analog_input_value[8];
    // encoder
    int32_t  x2340_encoder_position[4];
    // system
    char     x2500_firmware_version_string[32];
    char     x2501_board_name[32];
    uint32_t x2502_enabled_modules_bitmask;
    uint32_t x2503_uptime_seconds;
    uint32_t x2504_free_heap_bytes;
    uint32_t x2505_can_error_counter;
    uint8_t  x2507_reboot_command;
} OD_RAM_t;

// ---------------------------------------------------------------------------
// Attribute macros (default to empty — override for memory placement)
// ---------------------------------------------------------------------------
#ifndef OD_ATTR_PERSIST_COMM
#define OD_ATTR_PERSIST_COMM
#endif
#ifndef OD_ATTR_RAM
#define OD_ATTR_RAM
#endif
#ifndef OD_ATTR_OD
#define OD_ATTR_OD
#endif

extern OD_ATTR_PERSIST_COMM OD_PERSIST_COMM_t OD_PERSIST_COMM;
extern OD_ATTR_RAM OD_RAM_t OD_RAM;
extern OD_ATTR_OD OD_t *OD;

// ---------------------------------------------------------------------------
// Helper macros for direct OD_RAM access (used by sync functions)
// ---------------------------------------------------------------------------
#define OD_MOTOR_TARGET_POSITION    OD_RAM.x2000_motor_target_position
#define OD_MOTOR_ACTUAL_POSITION    OD_RAM.x2001_motor_actual_position
#define OD_MOTOR_SPEED              OD_RAM.x2002_motor_speed
#define OD_MOTOR_COMMAND_WORD       OD_RAM.x2003_motor_command_word
#define OD_MOTOR_STATUS_WORD        OD_RAM.x2004_motor_status_word
#define OD_MOTOR_ENABLE             OD_RAM.x2005_motor_enable
#define OD_MOTOR_ACCELERATION       OD_RAM.x2006_motor_acceleration
#define OD_MOTOR_IS_ABSOLUTE        OD_RAM.x2007_motor_is_absolute
#define OD_MOTOR_IS_FOREVER         OD_RAM.x200B_motor_is_forever
#define OD_HOMING_COMMAND           OD_RAM.x2010_homing_command
#define OD_HOMING_SPEED             OD_RAM.x2011_homing_speed
#define OD_HOMING_DIRECTION         OD_RAM.x2012_homing_direction
#define OD_HOMING_TIMEOUT           OD_RAM.x2013_homing_timeout
#define OD_HOMING_ENDSTOP_RELEASE   OD_RAM.x2014_homing_endstop_release
#define OD_HOMING_ENDSTOP_POLARITY  OD_RAM.x2015_homing_endstop_polarity
#define OD_LASER_PWM_VALUE          OD_RAM.x2100_laser_pwm_value
#define OD_LED_ARRAY_MODE           OD_RAM.x2200_led_array_mode
#define OD_LED_BRIGHTNESS           OD_RAM.x2201_led_brightness
#define OD_LED_UNIFORM_COLOUR       OD_RAM.x2202_led_uniform_colour
#define OD_LED_PATTERN_ID           OD_RAM.x2220_led_pattern_id
#define OD_LED_PATTERN_SPEED        OD_RAM.x2221_led_pattern_speed
#define OD_DIGITAL_INPUT_STATE      OD_RAM.x2300_digital_input_state
#define OD_DIGITAL_OUTPUT_COMMAND   OD_RAM.x2301_digital_output_command
#define OD_UPTIME_SECONDS           OD_RAM.x2503_uptime_seconds
#define OD_FREE_HEAP_BYTES          OD_RAM.x2504_free_heap_bytes

// ---------------------------------------------------------------------------
// OD_CNT_EM_PROD — required by CANopen.c EM_PRODUCER block
// ---------------------------------------------------------------------------
#define OD_CNT_EM_PROD  1

// ---------------------------------------------------------------------------
// OD_ENTRY_Hxxxx — pointers into ODList[], indexed by slot position.
// Indices must match the sorted ODList[] declaration in OD.c exactly.
// [0]=0x1000 [1]=0x1001 [2]=0x1003 [3]=0x1005 [4]=0x1006 [5]=0x1007
// [6]=0x1010 [7]=0x1011 [8]=0x1012 [9]=0x1014 [10]=0x1015 [11]=0x1016
// [12]=0x1017 [13]=0x1018 [14]=0x1019 [15]=0x1200 [16]=0x1280
// [17]=0x1400 ... [20]=0x1403 [21]=0x1600 ... [24]=0x1603
// [25]=0x1800 ... [28]=0x1803 [29]=0x1A00 ... [32]=0x1A03
// ---------------------------------------------------------------------------
#define OD_ENTRY_H1000  (&OD->list[0])
#define OD_ENTRY_H1001  (&OD->list[1])
#define OD_ENTRY_H1003  (&OD->list[2])
#define OD_ENTRY_H1005  (&OD->list[3])
#define OD_ENTRY_H1006  (&OD->list[4])
#define OD_ENTRY_H1007  (&OD->list[5])
#define OD_ENTRY_H1010  (&OD->list[6])
#define OD_ENTRY_H1011  (&OD->list[7])
#define OD_ENTRY_H1012  (&OD->list[8])
#define OD_ENTRY_H1014  (&OD->list[9])
#define OD_ENTRY_H1015  (&OD->list[10])
#define OD_ENTRY_H1016  (&OD->list[11])
#define OD_ENTRY_H1017  (&OD->list[12])
#define OD_ENTRY_H1018  (&OD->list[13])
#define OD_ENTRY_H1019  (&OD->list[14])
#define OD_ENTRY_H1200  (&OD->list[15])
#define OD_ENTRY_H1280  (&OD->list[16])
#define OD_ENTRY_H1400  (&OD->list[17])
#define OD_ENTRY_H1401  (&OD->list[18])
#define OD_ENTRY_H1402  (&OD->list[19])
#define OD_ENTRY_H1403  (&OD->list[20])
#define OD_ENTRY_H1600  (&OD->list[21])
#define OD_ENTRY_H1601  (&OD->list[22])
#define OD_ENTRY_H1602  (&OD->list[23])
#define OD_ENTRY_H1603  (&OD->list[24])
#define OD_ENTRY_H1800  (&OD->list[25])
#define OD_ENTRY_H1801  (&OD->list[26])
#define OD_ENTRY_H1802  (&OD->list[27])
#define OD_ENTRY_H1803  (&OD->list[28])
#define OD_ENTRY_H1A00  (&OD->list[29])
#define OD_ENTRY_H1A01  (&OD->list[30])
#define OD_ENTRY_H1A02  (&OD->list[31])
#define OD_ENTRY_H1A03  (&OD->list[32])

#endif // OD_H
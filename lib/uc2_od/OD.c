/*******************************************************************************
 * UC2 CANopen Object Dictionary — CANopenNode v4
 *
 * Manually maintained from uc2_canopen_registry.yaml.
 * Long-term: generate with tools/regenerate_all.py or export from
 * DOCUMENTATION/openUC2_satellite.eds via CANopenEditor.
 *
 * Replaces the trainer OD (lib/ESP32_CanOpenNode/src/OD.c) which used
 * 0x6000/0x6200/0x6401.  Motor commands now use UC2 entries at 0x2000+.
 ******************************************************************************/

#define OD_DEFINITION
#include "301/CO_ODinterface.h"
#include "OD.h"

#if CO_VERSION_MAJOR < 4
#error This Object dictionary requires CANopenNode V4.0 or above!
#endif

/* Sub-index 0 (array length) constants for UC2 arrays without _sub0 fields */
static uint8_t sub0_4 = 4;
static uint8_t sub0_8 = 8;

/*******************************************************************************
 * Persistent communication parameters
 ******************************************************************************/
OD_ATTR_PERSIST_COMM OD_PERSIST_COMM_t OD_PERSIST_COMM = {
    .x1000_deviceType = 0x00000000,
    .x1005_COB_ID_SYNCMessage = 0x00000080,
    .x1006_communicationCyclePeriod = 0x00000000,
    .x1007_synchronousWindowLength = 0x00000000,
    .x1012_COB_IDTimeStampObject = 0x00000100,
    .x1014_COB_ID_EMCY = 0x00000080,
    .x1015_inhibitTimeEMCY = 0x0000,
    .x1016_consumerHeartbeatTime_sub0 = 0x08,
    .x1016_consumerHeartbeatTime = {0,0,0,0,0,0,0,0},
    .x1017_producerHeartbeatTime = 0x03E8,
    .x1018_identity = {
        .highestSub_indexSupported = 0x04,
        .vendor_ID       = 0x00001234,
        .productCode     = 0x00000001,
        .revisionNumber  = 0x00000001,
        .serialNumber    = 0x00000001
    },
    .x1019_synchronousCounterOverflowValue = 0x00,
    .x1280_SDOClientParameter = {
        .highestSub_indexSupported = 0x03,
        .COB_IDClientToServerTx = 0x80000000,
        .COB_IDServerToClientRx = 0x80000000,
        .node_IDOfTheSDOServer  = 0x01
    },
    /* RPDO comm params (4 RPDOs — 1400..1403)
     * Master uses these to receive TPDO1 from slaves at node-ids 10..13.
     * COB-ID = 0x180 + slaveNodeId.  Slave's TPDO1 carries motor position+status.
     * On slaves these RPDOs stay disabled (bit 31 set) unless reconfigured. */
    .x1400_RPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x05,
        .COB_IDUsedByRPDO = 0x0000018A,  /* 0x180 + node 10 (slave slot 0) */
        .transmissionType = 0xFE,
        .eventTimer = 0x0000
    },
    .x1401_RPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x05,
        .COB_IDUsedByRPDO = 0x0000018B,  /* 0x180 + node 11 (slave slot 1) */
        .transmissionType = 0xFE,
        .eventTimer = 0x0000
    },
    .x1402_RPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x05,
        .COB_IDUsedByRPDO = 0x0000018C,  /* 0x180 + node 12 (slave slot 2) */
        .transmissionType = 0xFE,
        .eventTimer = 0x0000
    },
    .x1403_RPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x05,
        .COB_IDUsedByRPDO = 0x0000018D,  /* 0x180 + node 13 (slave slot 3) */
        .transmissionType = 0xFE,
        .eventTimer = 0x0000
    },
    /* RPDO mapping: each RPDO maps the incoming TPDO payload into a different
     * OD sub-index so 4 slaves can coexist without overwriting each other.
     * Layout matches slave TPDO1: i32 pos (0x2001 sub N) + u8 status (0x2004 sub N). */
    .x1600_RPDOMappingParameter = { .n=2, .o1=0x20010120, .o2=0x20040108, .o3=0,.o4=0,.o5=0,.o6=0,.o7=0,.o8=0 },
    .x1601_RPDOMappingParameter = { .n=2, .o1=0x20010220, .o2=0x20040208, .o3=0,.o4=0,.o5=0,.o6=0,.o7=0,.o8=0 },
    .x1602_RPDOMappingParameter = { .n=2, .o1=0x20010320, .o2=0x20040308, .o3=0,.o4=0,.o5=0,.o6=0,.o7=0,.o8=0 },
    .x1603_RPDOMappingParameter = { .n=2, .o1=0x20010420, .o2=0x20040408, .o3=0,.o4=0,.o5=0,.o6=0,.o7=0,.o8=0 },
    /* TPDO comm params (1800..1803)
     * TPDO1 (0x1800): motor state push — event-driven (type 254).
     * COB-ID uses dynamic node-id: bit 30 set = stack adds node-id at init.
     * inhibitTime = 100 (100 * 100 µs = 10 ms min gap between frames).
     * eventTimer = 500 ms heartbeat fallback so master detects stale slaves. */
    .x1800_TPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x06,
        .COB_IDUsedByTPDO = 0x40000180,  /* bit30=1: stack adds node-id */
        .transmissionType = 0xFE,        /* event-driven */
        .inhibitTime = 0x0064,           /* 100 * 100us = 10 ms */
        .eventTimer  = 0x01F4,           /* 500 ms */
        .SYNCStartValue = 0x00
    },
    .x1801_TPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x06,
        .COB_IDUsedByTPDO = 0xC0000280,
        .transmissionType = 0xFE,
        .inhibitTime = 0x0000,
        .eventTimer  = 0x05DC,
        .SYNCStartValue = 0x00
    },
    .x1802_TPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x06,
        .COB_IDUsedByTPDO = 0x80000480,
        .transmissionType = 0xFE,
        .inhibitTime = 0x0000,
        .eventTimer  = 0x0000,
        .SYNCStartValue = 0x00
    },
    .x1803_TPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x06,
        .COB_IDUsedByTPDO = 0x80000580,
        .transmissionType = 0xFE,
        .inhibitTime = 0x0000,
        .eventTimer  = 0x0000,
        .SYNCStartValue = 0x00
    },
    /* TPDO mapping: TPDO1 carries motor actual position (i32) + status word (u8).
     * Slave writes all 4 sub-indices identically (single-motor slave); the master's
     * RPDOs pick the correct sub-index per slot.  Total payload = 5 bytes. */
    .x1A00_TPDOMappingParameter = { .n=2, .o1=0x20010120, .o2=0x20040108, .o3=0,.o4=0,.o5=0,.o6=0,.o7=0,.o8=0 },
    .x1A01_TPDOMappingParameter = { .n=0, .o1=0,.o2=0,.o3=0,.o4=0,.o5=0,.o6=0,.o7=0,.o8=0 },
    .x1A02_TPDOMappingParameter = { .n=0, .o1=0,.o2=0,.o3=0,.o4=0,.o5=0,.o6=0,.o7=0,.o8=0 },
    .x1A03_TPDOMappingParameter = { .n=0, .o1=0,.o2=0,.o3=0,.o4=0,.o5=0,.o6=0,.o7=0,.o8=0 },
};

/*******************************************************************************
 * Volatile RAM area (zero-initialised)
 ******************************************************************************/
OD_ATTR_RAM OD_RAM_t OD_RAM = {
    .x1001_errorRegister = 0x00,
    .x1003_pre_definedErrorField_sub0 = 0,
    .x1010_storeParameters_sub0 = OD_CNT_ARR_1010,
    .x1010_storeParameters = {0x00000001, 0x00000001, 0x00000001, 0x00000001},
    .x1011_restoreDefaultParameters_sub0 = OD_CNT_ARR_1011,
    .x1011_restoreDefaultParameters = {0x00000001, 0x00000001, 0x00000001, 0x00000001},
    .x1200_SDOServerParameter = {
        .highestSub_indexSupported = 0x02,
        .COB_IDClientToServerRx = 0x00000600,
        .COB_IDServerToClientTx = 0x00000580
    }
};

/*******************************************************************************
 * OD object descriptor structures (constant, placed in flash via CO_PROGMEM)
 ******************************************************************************/
typedef struct {
    /* CiA 301 communication */
    OD_obj_var_t    o_1000_deviceType;
    OD_obj_var_t    o_1001_errorRegister;
    OD_obj_array_t  o_1003_pre_definedErrorField;
    OD_obj_var_t    o_1005_COB_ID_SYNCMessage;
    OD_obj_var_t    o_1006_communicationCyclePeriod;
    OD_obj_var_t    o_1007_synchronousWindowLength;
    OD_obj_array_t  o_1010_storeParameters;
    OD_obj_array_t  o_1011_restoreDefaultParameters;
    OD_obj_var_t    o_1012_COB_IDTimeStampObject;
    OD_obj_var_t    o_1014_COB_ID_EMCY;
    OD_obj_var_t    o_1015_inhibitTimeEMCY;
    OD_obj_array_t  o_1016_consumerHeartbeatTime;
    OD_obj_var_t    o_1017_producerHeartbeatTime;
    OD_obj_record_t o_1018_identity[5];
    OD_obj_var_t    o_1019_synchronousCounterOverflowValue;
    OD_obj_record_t o_1200_SDOServerParameter[3];
    OD_obj_record_t o_1280_SDOClientParameter[4];
    /* RPDO comm+map (x4) */
    OD_obj_record_t o_1400_RPDOCommunicationParameter[4];
    OD_obj_record_t o_1401_RPDOCommunicationParameter[4];
    OD_obj_record_t o_1402_RPDOCommunicationParameter[4];
    OD_obj_record_t o_1403_RPDOCommunicationParameter[4];
    OD_obj_record_t o_1600_RPDOMappingParameter[9];
    OD_obj_record_t o_1601_RPDOMappingParameter[9];
    OD_obj_record_t o_1602_RPDOMappingParameter[9];
    OD_obj_record_t o_1603_RPDOMappingParameter[9];
    /* TPDO comm+map (x4) */
    OD_obj_record_t o_1800_TPDOCommunicationParameter[6];
    OD_obj_record_t o_1801_TPDOCommunicationParameter[6];
    OD_obj_record_t o_1802_TPDOCommunicationParameter[6];
    OD_obj_record_t o_1803_TPDOCommunicationParameter[6];
    OD_obj_record_t o_1A00_TPDOMappingParameter[9];
    OD_obj_record_t o_1A01_TPDOMappingParameter[9];
    OD_obj_record_t o_1A02_TPDOMappingParameter[9];
    OD_obj_record_t o_1A03_TPDOMappingParameter[9];
    /* UC2 motor (0x2000-0x200A) */
    OD_obj_array_t  o_2000_motor_target_position;
    OD_obj_array_t  o_2001_motor_actual_position;
    OD_obj_array_t  o_2002_motor_speed;
    OD_obj_var_t    o_2003_motor_command_word;
    OD_obj_array_t  o_2004_motor_status_word;
    OD_obj_array_t  o_2005_motor_enable;
    OD_obj_array_t  o_2006_motor_acceleration;
    OD_obj_array_t  o_2007_motor_is_absolute;
    OD_obj_array_t  o_2008_motor_min_position;
    OD_obj_array_t  o_2009_motor_max_position;
    OD_obj_array_t  o_200A_motor_jerk;
    OD_obj_array_t  o_200B_motor_is_forever;
    /* UC2 homing (0x2010-0x2015) */
    OD_obj_array_t  o_2010_homing_command;
    OD_obj_array_t  o_2011_homing_speed;
    OD_obj_array_t  o_2012_homing_direction;
    OD_obj_array_t  o_2013_homing_timeout;
    OD_obj_array_t  o_2014_homing_endstop_release;
    OD_obj_array_t  o_2015_homing_endstop_polarity;
    /* UC2 TMC (0x2020-0x2027) */
    OD_obj_array_t  o_2020_tmc_microsteps;
    OD_obj_array_t  o_2021_tmc_rms_current;
    OD_obj_array_t  o_2022_tmc_stallguard_threshold;
    OD_obj_array_t  o_2023_tmc_coolstep_semin;
    OD_obj_array_t  o_2024_tmc_coolstep_semax;
    OD_obj_array_t  o_2025_tmc_blank_time;
    OD_obj_array_t  o_2026_tmc_toff;
    OD_obj_array_t  o_2027_tmc_stall_count;
    /* UC2 laser (0x2100-0x2106) */
    OD_obj_array_t  o_2100_laser_pwm_value;
    OD_obj_array_t  o_2101_laser_max_value;
    OD_obj_array_t  o_2102_laser_pwm_frequency;
    OD_obj_array_t  o_2103_laser_pwm_resolution;
    OD_obj_var_t    o_2106_laser_safety_state;
    /* UC2 LED (0x2200-0x2203) */
    OD_obj_var_t    o_2200_led_array_mode;
    OD_obj_var_t    o_2201_led_brightness;
    OD_obj_var_t    o_2202_led_uniform_colour;
    OD_obj_var_t    o_2203_led_pixel_count;
    /* UC2 digital I/O (0x2300-0x2301) */
    OD_obj_array_t  o_2300_digital_input_state;
    OD_obj_array_t  o_2301_digital_output_command;
    /* UC2 analog I/O (0x2310) */
    OD_obj_array_t  o_2310_analog_input_value;
    /* UC2 encoder (0x2340) */
    OD_obj_array_t  o_2340_encoder_position;
    /* UC2 system (0x2500-0x2507) */
    OD_obj_var_t    o_2503_uptime_seconds;
    OD_obj_var_t    o_2504_free_heap_bytes;
    OD_obj_var_t    o_2505_can_error_counter;
    OD_obj_var_t    o_2507_reboot_command;
} ODObjs_t;

/* Helpers — build a standard 4-entry RPDO comm descriptor (sub 0,1,2,5) */
#define _RPDO_COMM(fld) { \
    { .dataOrig=&OD_PERSIST_COMM.fld.highestSub_indexSupported, .subIndex=0, .attribute=ODA_SDO_R,        .dataLength=1 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.COB_IDUsedByRPDO,         .subIndex=1, .attribute=ODA_SDO_RW|ODA_MB,.dataLength=4 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.transmissionType,         .subIndex=2, .attribute=ODA_SDO_RW,        .dataLength=1 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.eventTimer,               .subIndex=5, .attribute=ODA_SDO_RW|ODA_MB,.dataLength=2 }, \
}

/* Helper — 9-entry RPDO/TPDO mapping descriptor */
#define _PDO_MAP(fld) { \
    { .dataOrig=&OD_PERSIST_COMM.fld.n,  .subIndex=0, .attribute=ODA_SDO_RW,        .dataLength=1 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.o1, .subIndex=1, .attribute=ODA_SDO_RW|ODA_MB, .dataLength=4 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.o2, .subIndex=2, .attribute=ODA_SDO_RW|ODA_MB, .dataLength=4 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.o3, .subIndex=3, .attribute=ODA_SDO_RW|ODA_MB, .dataLength=4 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.o4, .subIndex=4, .attribute=ODA_SDO_RW|ODA_MB, .dataLength=4 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.o5, .subIndex=5, .attribute=ODA_SDO_RW|ODA_MB, .dataLength=4 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.o6, .subIndex=6, .attribute=ODA_SDO_RW|ODA_MB, .dataLength=4 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.o7, .subIndex=7, .attribute=ODA_SDO_RW|ODA_MB, .dataLength=4 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.o8, .subIndex=8, .attribute=ODA_SDO_RW|ODA_MB, .dataLength=4 }, \
}

/* Helper — 6-entry TPDO comm descriptor (sub 0,1,2,3,5,6) */
#define _TPDO_COMM(fld) { \
    { .dataOrig=&OD_PERSIST_COMM.fld.highestSub_indexSupported, .subIndex=0, .attribute=ODA_SDO_R,        .dataLength=1 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.COB_IDUsedByTPDO,         .subIndex=1, .attribute=ODA_SDO_RW|ODA_MB,.dataLength=4 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.transmissionType,         .subIndex=2, .attribute=ODA_SDO_RW,        .dataLength=1 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.inhibitTime,              .subIndex=3, .attribute=ODA_SDO_RW|ODA_MB,.dataLength=2 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.eventTimer,               .subIndex=5, .attribute=ODA_SDO_RW|ODA_MB,.dataLength=2 }, \
    { .dataOrig=&OD_PERSIST_COMM.fld.SYNCStartValue,           .subIndex=6, .attribute=ODA_SDO_RW,        .dataLength=1 }, \
}

/* Helper — array of 4 x int32 (multi-byte) */
#define _ARR4_I32(ramfld, attr) { \
    .dataOrig0 = &sub0_4, \
    .dataOrig  = &OD_RAM.ramfld[0], \
    .attribute0 = ODA_SDO_R, \
    .attribute   = (attr) | ODA_MB, \
    .dataElementLength = 4, \
    .dataElementSizeof = sizeof(int32_t) \
}

/* Helper — array of 4 x uint32 (multi-byte) */
#define _ARR4_U32(ramfld, attr) { \
    .dataOrig0 = &sub0_4, \
    .dataOrig  = &OD_RAM.ramfld[0], \
    .attribute0 = ODA_SDO_R, \
    .attribute   = (attr) | ODA_MB, \
    .dataElementLength = 4, \
    .dataElementSizeof = sizeof(uint32_t) \
}

/* Helper — array of 4 x uint16 (multi-byte) */
#define _ARR4_U16(ramfld, attr) { \
    .dataOrig0 = &sub0_4, \
    .dataOrig  = &OD_RAM.ramfld[0], \
    .attribute0 = ODA_SDO_R, \
    .attribute   = (attr) | ODA_MB, \
    .dataElementLength = 2, \
    .dataElementSizeof = sizeof(uint16_t) \
}

/* Helper — array of 4 x uint8 (single-byte) */
#define _ARR4_U8(ramfld, attr) { \
    .dataOrig0 = &sub0_4, \
    .dataOrig  = &OD_RAM.ramfld[0], \
    .attribute0 = ODA_SDO_R, \
    .attribute   = (attr), \
    .dataElementLength = 1, \
    .dataElementSizeof = sizeof(uint8_t) \
}

/* Helper — array of 8 x uint8 (single-byte) */
#define _ARR8_U8(ramfld, attr) { \
    .dataOrig0 = &sub0_8, \
    .dataOrig  = &OD_RAM.ramfld[0], \
    .attribute0 = ODA_SDO_R, \
    .attribute   = (attr), \
    .dataElementLength = 1, \
    .dataElementSizeof = sizeof(uint8_t) \
}

/* Helper — array of 8 x uint16 (multi-byte) */
#define _ARR8_U16(ramfld, attr) { \
    .dataOrig0 = &sub0_8, \
    .dataOrig  = &OD_RAM.ramfld[0], \
    .attribute0 = ODA_SDO_R, \
    .attribute   = (attr) | ODA_MB, \
    .dataElementLength = 2, \
    .dataElementSizeof = sizeof(uint16_t) \
}

static CO_PROGMEM ODObjs_t ODObjs = {
    /* 0x1000 — Device type */
    .o_1000_deviceType = {
        .dataOrig = &OD_PERSIST_COMM.x1000_deviceType,
        .attribute = ODA_SDO_R | ODA_MB,
        .dataLength = 4
    },
    /* 0x1001 — Error register */
    .o_1001_errorRegister = {
        .dataOrig = &OD_RAM.x1001_errorRegister,
        .attribute = ODA_SDO_R | ODA_TPDO,
        .dataLength = 1
    },
    /* 0x1003 — Pre-defined error field */
    .o_1003_pre_definedErrorField = {
        .dataOrig0 = NULL,
        .dataOrig  = NULL,
        .attribute0 = ODA_SDO_RW,
        .attribute   = ODA_SDO_R | ODA_MB,
        .dataElementLength = 4,
        .dataElementSizeof = sizeof(uint32_t)
    },
    /* 0x1005 — COB-ID SYNC */
    .o_1005_COB_ID_SYNCMessage = {
        .dataOrig = &OD_PERSIST_COMM.x1005_COB_ID_SYNCMessage,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 4
    },
    /* 0x1006 — Comm cycle period */
    .o_1006_communicationCyclePeriod = {
        .dataOrig = &OD_PERSIST_COMM.x1006_communicationCyclePeriod,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 4
    },
    /* 0x1007 — Sync window */
    .o_1007_synchronousWindowLength = {
        .dataOrig = &OD_PERSIST_COMM.x1007_synchronousWindowLength,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 4
    },
    /* 0x1010 — Store parameters */
    .o_1010_storeParameters = {
        .dataOrig0 = &OD_RAM.x1010_storeParameters_sub0,
        .dataOrig  = &OD_RAM.x1010_storeParameters[0],
        .attribute0 = ODA_SDO_R,
        .attribute   = ODA_SDO_RW | ODA_MB,
        .dataElementLength = 4,
        .dataElementSizeof = sizeof(uint32_t)
    },
    /* 0x1011 — Restore defaults */
    .o_1011_restoreDefaultParameters = {
        .dataOrig0 = &OD_RAM.x1011_restoreDefaultParameters_sub0,
        .dataOrig  = &OD_RAM.x1011_restoreDefaultParameters[0],
        .attribute0 = ODA_SDO_R,
        .attribute   = ODA_SDO_RW | ODA_MB,
        .dataElementLength = 4,
        .dataElementSizeof = sizeof(uint32_t)
    },
    /* 0x1012 — COB-ID timestamp */
    .o_1012_COB_IDTimeStampObject = {
        .dataOrig = &OD_PERSIST_COMM.x1012_COB_IDTimeStampObject,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 4
    },
    /* 0x1014 — COB-ID EMCY */
    .o_1014_COB_ID_EMCY = {
        .dataOrig = &OD_PERSIST_COMM.x1014_COB_ID_EMCY,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 4
    },
    /* 0x1015 — Inhibit time EMCY */
    .o_1015_inhibitTimeEMCY = {
        .dataOrig = &OD_PERSIST_COMM.x1015_inhibitTimeEMCY,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 2
    },
    /* 0x1016 — Consumer heartbeat time */
    .o_1016_consumerHeartbeatTime = {
        .dataOrig0 = &OD_PERSIST_COMM.x1016_consumerHeartbeatTime_sub0,
        .dataOrig  = &OD_PERSIST_COMM.x1016_consumerHeartbeatTime[0],
        .attribute0 = ODA_SDO_R,
        .attribute   = ODA_SDO_RW | ODA_MB,
        .dataElementLength = 4,
        .dataElementSizeof = sizeof(uint32_t)
    },
    /* 0x1017 — Producer heartbeat time */
    .o_1017_producerHeartbeatTime = {
        .dataOrig = &OD_PERSIST_COMM.x1017_producerHeartbeatTime,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 2
    },
    /* 0x1018 — Identity */
    .o_1018_identity = {
        { .dataOrig=&OD_PERSIST_COMM.x1018_identity.highestSub_indexSupported, .subIndex=0, .attribute=ODA_SDO_R,        .dataLength=1 },
        { .dataOrig=&OD_PERSIST_COMM.x1018_identity.vendor_ID,                 .subIndex=1, .attribute=ODA_SDO_R|ODA_MB, .dataLength=4 },
        { .dataOrig=&OD_PERSIST_COMM.x1018_identity.productCode,               .subIndex=2, .attribute=ODA_SDO_R|ODA_MB, .dataLength=4 },
        { .dataOrig=&OD_PERSIST_COMM.x1018_identity.revisionNumber,            .subIndex=3, .attribute=ODA_SDO_R|ODA_MB, .dataLength=4 },
        { .dataOrig=&OD_PERSIST_COMM.x1018_identity.serialNumber,              .subIndex=4, .attribute=ODA_SDO_R|ODA_MB, .dataLength=4 }
    },
    /* 0x1019 — SYNC counter overflow */
    .o_1019_synchronousCounterOverflowValue = {
        .dataOrig = &OD_PERSIST_COMM.x1019_synchronousCounterOverflowValue,
        .attribute = ODA_SDO_RW,
        .dataLength = 1
    },
    /* 0x1200 — SDO server parameter */
    .o_1200_SDOServerParameter = {
        { .dataOrig=&OD_RAM.x1200_SDOServerParameter.highestSub_indexSupported, .subIndex=0, .attribute=ODA_SDO_R,              .dataLength=1 },
        { .dataOrig=&OD_RAM.x1200_SDOServerParameter.COB_IDClientToServerRx,   .subIndex=1, .attribute=ODA_SDO_R|ODA_TPDO|ODA_MB, .dataLength=4 },
        { .dataOrig=&OD_RAM.x1200_SDOServerParameter.COB_IDServerToClientTx,   .subIndex=2, .attribute=ODA_SDO_R|ODA_TPDO|ODA_MB, .dataLength=4 }
    },
    /* 0x1280 — SDO client parameter */
    .o_1280_SDOClientParameter = {
        { .dataOrig=&OD_PERSIST_COMM.x1280_SDOClientParameter.highestSub_indexSupported, .subIndex=0, .attribute=ODA_SDO_R,                 .dataLength=1 },
        { .dataOrig=&OD_PERSIST_COMM.x1280_SDOClientParameter.COB_IDClientToServerTx,    .subIndex=1, .attribute=ODA_SDO_RW|ODA_TRPDO|ODA_MB, .dataLength=4 },
        { .dataOrig=&OD_PERSIST_COMM.x1280_SDOClientParameter.COB_IDServerToClientRx,    .subIndex=2, .attribute=ODA_SDO_RW|ODA_TRPDO|ODA_MB, .dataLength=4 },
        { .dataOrig=&OD_PERSIST_COMM.x1280_SDOClientParameter.node_IDOfTheSDOServer,     .subIndex=3, .attribute=ODA_SDO_RW,                 .dataLength=1 }
    },
    /* RPDO comm (1400-1403) */
    .o_1400_RPDOCommunicationParameter = _RPDO_COMM(x1400_RPDOCommunicationParameter),
    .o_1401_RPDOCommunicationParameter = _RPDO_COMM(x1401_RPDOCommunicationParameter),
    .o_1402_RPDOCommunicationParameter = _RPDO_COMM(x1402_RPDOCommunicationParameter),
    .o_1403_RPDOCommunicationParameter = _RPDO_COMM(x1403_RPDOCommunicationParameter),
    /* RPDO mapping (1600-1603) */
    .o_1600_RPDOMappingParameter = _PDO_MAP(x1600_RPDOMappingParameter),
    .o_1601_RPDOMappingParameter = _PDO_MAP(x1601_RPDOMappingParameter),
    .o_1602_RPDOMappingParameter = _PDO_MAP(x1602_RPDOMappingParameter),
    .o_1603_RPDOMappingParameter = _PDO_MAP(x1603_RPDOMappingParameter),
    /* TPDO comm (1800-1803) */
    .o_1800_TPDOCommunicationParameter = _TPDO_COMM(x1800_TPDOCommunicationParameter),
    .o_1801_TPDOCommunicationParameter = _TPDO_COMM(x1801_TPDOCommunicationParameter),
    .o_1802_TPDOCommunicationParameter = _TPDO_COMM(x1802_TPDOCommunicationParameter),
    .o_1803_TPDOCommunicationParameter = _TPDO_COMM(x1803_TPDOCommunicationParameter),
    /* TPDO mapping (1A00-1A03) */
    .o_1A00_TPDOMappingParameter = _PDO_MAP(x1A00_TPDOMappingParameter),
    .o_1A01_TPDOMappingParameter = _PDO_MAP(x1A01_TPDOMappingParameter),
    .o_1A02_TPDOMappingParameter = _PDO_MAP(x1A02_TPDOMappingParameter),
    .o_1A03_TPDOMappingParameter = _PDO_MAP(x1A03_TPDOMappingParameter),
    /* -----------------------------------------------------------------------
     * UC2 motor (0x2000-0x200A) — arrays of 4
     * ----------------------------------------------------------------------- */
    .o_2000_motor_target_position = _ARR4_I32(x2000_motor_target_position, ODA_SDO_RW | ODA_RPDO),
    .o_2001_motor_actual_position = _ARR4_I32(x2001_motor_actual_position, ODA_SDO_RW | ODA_TRPDO),
    .o_2002_motor_speed           = _ARR4_U32(x2002_motor_speed,           ODA_SDO_RW | ODA_RPDO),
    .o_2003_motor_command_word = {
        .dataOrig = &OD_RAM.x2003_motor_command_word,
        .attribute = ODA_SDO_RW | ODA_RPDO,
        .dataLength = 1
    },
    .o_2004_motor_status_word  = _ARR4_U8(x2004_motor_status_word,  ODA_SDO_RW | ODA_TRPDO),
    .o_2005_motor_enable       = _ARR4_U8(x2005_motor_enable,       ODA_SDO_RW | ODA_RPDO),
    .o_2006_motor_acceleration = _ARR4_U32(x2006_motor_acceleration, ODA_SDO_RW | ODA_RPDO),
    .o_2007_motor_is_absolute  = _ARR4_U8(x2007_motor_is_absolute,   ODA_SDO_RW | ODA_RPDO),
    .o_2008_motor_min_position = _ARR4_I32(x2008_motor_min_position, ODA_SDO_RW),
    .o_2009_motor_max_position = _ARR4_I32(x2009_motor_max_position, ODA_SDO_RW),
    .o_200A_motor_jerk         = _ARR4_U32(x200A_motor_jerk,         ODA_SDO_RW),
    .o_200B_motor_is_forever   = _ARR4_U8(x200B_motor_is_forever,    ODA_SDO_RW | ODA_RPDO),
    /* -----------------------------------------------------------------------
     * UC2 homing (0x2010-0x2015)
     * ----------------------------------------------------------------------- */
    .o_2010_homing_command          = _ARR4_U8(x2010_homing_command,          ODA_SDO_RW | ODA_RPDO),
    .o_2011_homing_speed            = _ARR4_U32(x2011_homing_speed,           ODA_SDO_RW | ODA_RPDO),
    .o_2012_homing_direction        = _ARR4_U8(x2012_homing_direction,        ODA_SDO_RW | ODA_RPDO),
    .o_2013_homing_timeout          = _ARR4_U32(x2013_homing_timeout,         ODA_SDO_RW | ODA_RPDO),
    .o_2014_homing_endstop_release  = _ARR4_I32(x2014_homing_endstop_release, ODA_SDO_RW | ODA_RPDO),
    .o_2015_homing_endstop_polarity = _ARR4_U8(x2015_homing_endstop_polarity, ODA_SDO_RW | ODA_RPDO),
    /* -----------------------------------------------------------------------
     * UC2 TMC (0x2020-0x2027)
     * ----------------------------------------------------------------------- */
    .o_2020_tmc_microsteps          = _ARR4_U16(x2020_tmc_microsteps,          ODA_SDO_RW),
    .o_2021_tmc_rms_current         = _ARR4_U16(x2021_tmc_rms_current,         ODA_SDO_RW),
    .o_2022_tmc_stallguard_threshold= _ARR4_U8(x2022_tmc_stallguard_threshold, ODA_SDO_RW),
    .o_2023_tmc_coolstep_semin      = _ARR4_U8(x2023_tmc_coolstep_semin,       ODA_SDO_RW),
    .o_2024_tmc_coolstep_semax      = _ARR4_U8(x2024_tmc_coolstep_semax,       ODA_SDO_RW),
    .o_2025_tmc_blank_time          = _ARR4_U8(x2025_tmc_blank_time,           ODA_SDO_RW),
    .o_2026_tmc_toff                = _ARR4_U8(x2026_tmc_toff,                 ODA_SDO_RW),
    .o_2027_tmc_stall_count         = _ARR4_U32(x2027_tmc_stall_count,         ODA_SDO_R | ODA_TPDO),
    /* -----------------------------------------------------------------------
     * UC2 laser (0x2100-0x2106)
     * ----------------------------------------------------------------------- */
    .o_2100_laser_pwm_value      = _ARR4_U16(x2100_laser_pwm_value,      ODA_SDO_RW | ODA_RPDO),
    .o_2101_laser_max_value      = _ARR4_U16(x2101_laser_max_value,      ODA_SDO_RW),
    .o_2102_laser_pwm_frequency  = _ARR4_U32(x2102_laser_pwm_frequency,  ODA_SDO_RW),
    .o_2103_laser_pwm_resolution = _ARR4_U8(x2103_laser_pwm_resolution,  ODA_SDO_RW),
    .o_2106_laser_safety_state = {
        .dataOrig = &OD_RAM.x2106_laser_safety_state,
        .attribute = ODA_SDO_RW | ODA_TPDO,
        .dataLength = 1
    },
    /* -----------------------------------------------------------------------
     * UC2 LED (0x2200-0x2203)
     * ----------------------------------------------------------------------- */
    .o_2200_led_array_mode = {
        .dataOrig = &OD_RAM.x2200_led_array_mode,
        .attribute = ODA_SDO_RW | ODA_RPDO,
        .dataLength = 1
    },
    .o_2201_led_brightness = {
        .dataOrig = &OD_RAM.x2201_led_brightness,
        .attribute = ODA_SDO_RW | ODA_RPDO,
        .dataLength = 1
    },
    .o_2202_led_uniform_colour = {
        .dataOrig = &OD_RAM.x2202_led_uniform_colour,
        .attribute = ODA_SDO_RW | ODA_RPDO | ODA_MB,
        .dataLength = 4
    },
    .o_2203_led_pixel_count = {
        .dataOrig = &OD_RAM.x2203_led_pixel_count,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 2
    },
    /* -----------------------------------------------------------------------
     * UC2 digital I/O (0x2300-0x2301) — arrays of 8
     * ----------------------------------------------------------------------- */
    .o_2300_digital_input_state    = _ARR8_U8(x2300_digital_input_state,    ODA_SDO_RW | ODA_TPDO),
    .o_2301_digital_output_command = _ARR8_U8(x2301_digital_output_command, ODA_SDO_RW | ODA_RPDO),
    /* 0x2310 — Analog inputs (8 x uint16) */
    .o_2310_analog_input_value = _ARR8_U16(x2310_analog_input_value, ODA_SDO_R | ODA_TPDO),
    /* 0x2340 — Encoder position (4 x int32) */
    .o_2340_encoder_position = _ARR4_I32(x2340_encoder_position, ODA_SDO_RW | ODA_TPDO),
    /* -----------------------------------------------------------------------
     * UC2 system (0x2503-0x2507)
     * ----------------------------------------------------------------------- */
    .o_2503_uptime_seconds = {
        .dataOrig = &OD_RAM.x2503_uptime_seconds,
        .attribute = ODA_SDO_R | ODA_TPDO | ODA_MB,
        .dataLength = 4
    },
    .o_2504_free_heap_bytes = {
        .dataOrig = &OD_RAM.x2504_free_heap_bytes,
        .attribute = ODA_SDO_R | ODA_MB,
        .dataLength = 4
    },
    .o_2505_can_error_counter = {
        .dataOrig = &OD_RAM.x2505_can_error_counter,
        .attribute = ODA_SDO_R | ODA_MB,
        .dataLength = 4
    },
    .o_2507_reboot_command = {
        .dataOrig = &OD_RAM.x2507_reboot_command,
        .attribute = ODA_SDO_RW,
        .dataLength = 1
    }
};

/*******************************************************************************
 * Object Dictionary list — must be sorted by ascending index
 ******************************************************************************/
static OD_ATTR_OD OD_entry_t ODList[] = {
    /* CiA 301 communication */
    {0x1000, 0x01, ODT_VAR, &ODObjs.o_1000_deviceType,               NULL},
    {0x1001, 0x01, ODT_VAR, &ODObjs.o_1001_errorRegister,            NULL},
    {0x1003, 0x11, ODT_ARR, &ODObjs.o_1003_pre_definedErrorField,    NULL},
    {0x1005, 0x01, ODT_VAR, &ODObjs.o_1005_COB_ID_SYNCMessage,       NULL},
    {0x1006, 0x01, ODT_VAR, &ODObjs.o_1006_communicationCyclePeriod, NULL},
    {0x1007, 0x01, ODT_VAR, &ODObjs.o_1007_synchronousWindowLength,  NULL},
    {0x1010, 0x05, ODT_ARR, &ODObjs.o_1010_storeParameters,          NULL},
    {0x1011, 0x05, ODT_ARR, &ODObjs.o_1011_restoreDefaultParameters, NULL},
    {0x1012, 0x01, ODT_VAR, &ODObjs.o_1012_COB_IDTimeStampObject,    NULL},
    {0x1014, 0x01, ODT_VAR, &ODObjs.o_1014_COB_ID_EMCY,              NULL},
    {0x1015, 0x01, ODT_VAR, &ODObjs.o_1015_inhibitTimeEMCY,          NULL},
    {0x1016, 0x09, ODT_ARR, &ODObjs.o_1016_consumerHeartbeatTime,    NULL},
    {0x1017, 0x01, ODT_VAR, &ODObjs.o_1017_producerHeartbeatTime,    NULL},
    {0x1018, 0x05, ODT_REC, &ODObjs.o_1018_identity,                 NULL},
    {0x1019, 0x01, ODT_VAR, &ODObjs.o_1019_synchronousCounterOverflowValue, NULL},
    {0x1200, 0x03, ODT_REC, &ODObjs.o_1200_SDOServerParameter,       NULL},
    {0x1280, 0x04, ODT_REC, &ODObjs.o_1280_SDOClientParameter,       NULL},
    /* RPDOs */
    {0x1400, 0x04, ODT_REC, &ODObjs.o_1400_RPDOCommunicationParameter, NULL},
    {0x1401, 0x04, ODT_REC, &ODObjs.o_1401_RPDOCommunicationParameter, NULL},
    {0x1402, 0x04, ODT_REC, &ODObjs.o_1402_RPDOCommunicationParameter, NULL},
    {0x1403, 0x04, ODT_REC, &ODObjs.o_1403_RPDOCommunicationParameter, NULL},
    {0x1600, 0x09, ODT_REC, &ODObjs.o_1600_RPDOMappingParameter,       NULL},
    {0x1601, 0x09, ODT_REC, &ODObjs.o_1601_RPDOMappingParameter,       NULL},
    {0x1602, 0x09, ODT_REC, &ODObjs.o_1602_RPDOMappingParameter,       NULL},
    {0x1603, 0x09, ODT_REC, &ODObjs.o_1603_RPDOMappingParameter,       NULL},
    /* TPDOs */
    {0x1800, 0x06, ODT_REC, &ODObjs.o_1800_TPDOCommunicationParameter, NULL},
    {0x1801, 0x06, ODT_REC, &ODObjs.o_1801_TPDOCommunicationParameter, NULL},
    {0x1802, 0x06, ODT_REC, &ODObjs.o_1802_TPDOCommunicationParameter, NULL},
    {0x1803, 0x06, ODT_REC, &ODObjs.o_1803_TPDOCommunicationParameter, NULL},
    {0x1A00, 0x09, ODT_REC, &ODObjs.o_1A00_TPDOMappingParameter,       NULL},
    {0x1A01, 0x09, ODT_REC, &ODObjs.o_1A01_TPDOMappingParameter,       NULL},
    {0x1A02, 0x09, ODT_REC, &ODObjs.o_1A02_TPDOMappingParameter,       NULL},
    {0x1A03, 0x09, ODT_REC, &ODObjs.o_1A03_TPDOMappingParameter,       NULL},
    /* UC2 motor */
    {0x2000, 0x05, ODT_ARR, &ODObjs.o_2000_motor_target_position,      NULL},
    {0x2001, 0x05, ODT_ARR, &ODObjs.o_2001_motor_actual_position,      NULL},
    {0x2002, 0x05, ODT_ARR, &ODObjs.o_2002_motor_speed,                NULL},
    {0x2003, 0x01, ODT_VAR, &ODObjs.o_2003_motor_command_word,         NULL},
    {0x2004, 0x05, ODT_ARR, &ODObjs.o_2004_motor_status_word,          NULL},
    {0x2005, 0x05, ODT_ARR, &ODObjs.o_2005_motor_enable,               NULL},
    {0x2006, 0x05, ODT_ARR, &ODObjs.o_2006_motor_acceleration,         NULL},
    {0x2007, 0x05, ODT_ARR, &ODObjs.o_2007_motor_is_absolute,          NULL},
    {0x2008, 0x05, ODT_ARR, &ODObjs.o_2008_motor_min_position,         NULL},
    {0x2009, 0x05, ODT_ARR, &ODObjs.o_2009_motor_max_position,         NULL},
    {0x200A, 0x05, ODT_ARR, &ODObjs.o_200A_motor_jerk,                 NULL},
    {0x200B, 0x05, ODT_ARR, &ODObjs.o_200B_motor_is_forever,            NULL},
    /* UC2 homing */
    {0x2010, 0x05, ODT_ARR, &ODObjs.o_2010_homing_command,             NULL},
    {0x2011, 0x05, ODT_ARR, &ODObjs.o_2011_homing_speed,               NULL},
    {0x2012, 0x05, ODT_ARR, &ODObjs.o_2012_homing_direction,           NULL},
    {0x2013, 0x05, ODT_ARR, &ODObjs.o_2013_homing_timeout,             NULL},
    {0x2014, 0x05, ODT_ARR, &ODObjs.o_2014_homing_endstop_release,     NULL},
    {0x2015, 0x05, ODT_ARR, &ODObjs.o_2015_homing_endstop_polarity,    NULL},
    /* UC2 TMC */
    {0x2020, 0x05, ODT_ARR, &ODObjs.o_2020_tmc_microsteps,              NULL},
    {0x2021, 0x05, ODT_ARR, &ODObjs.o_2021_tmc_rms_current,            NULL},
    {0x2022, 0x05, ODT_ARR, &ODObjs.o_2022_tmc_stallguard_threshold,   NULL},
    {0x2023, 0x05, ODT_ARR, &ODObjs.o_2023_tmc_coolstep_semin,         NULL},
    {0x2024, 0x05, ODT_ARR, &ODObjs.o_2024_tmc_coolstep_semax,         NULL},
    {0x2025, 0x05, ODT_ARR, &ODObjs.o_2025_tmc_blank_time,             NULL},
    {0x2026, 0x05, ODT_ARR, &ODObjs.o_2026_tmc_toff,                   NULL},
    {0x2027, 0x05, ODT_ARR, &ODObjs.o_2027_tmc_stall_count,            NULL},
    /* UC2 laser */
    {0x2100, 0x05, ODT_ARR, &ODObjs.o_2100_laser_pwm_value,            NULL},
    {0x2101, 0x05, ODT_ARR, &ODObjs.o_2101_laser_max_value,            NULL},
    {0x2102, 0x05, ODT_ARR, &ODObjs.o_2102_laser_pwm_frequency,        NULL},
    {0x2103, 0x05, ODT_ARR, &ODObjs.o_2103_laser_pwm_resolution,       NULL},
    {0x2106, 0x01, ODT_VAR, &ODObjs.o_2106_laser_safety_state,         NULL},
    /* UC2 LED */
    {0x2200, 0x01, ODT_VAR, &ODObjs.o_2200_led_array_mode,             NULL},
    {0x2201, 0x01, ODT_VAR, &ODObjs.o_2201_led_brightness,             NULL},
    {0x2202, 0x01, ODT_VAR, &ODObjs.o_2202_led_uniform_colour,         NULL},
    {0x2203, 0x01, ODT_VAR, &ODObjs.o_2203_led_pixel_count,            NULL},
    /* UC2 digital I/O */
    {0x2300, 0x09, ODT_ARR, &ODObjs.o_2300_digital_input_state,        NULL},
    {0x2301, 0x09, ODT_ARR, &ODObjs.o_2301_digital_output_command,     NULL},
    /* UC2 analog I/O */
    {0x2310, 0x09, ODT_ARR, &ODObjs.o_2310_analog_input_value,         NULL},
    /* UC2 encoder */
    {0x2340, 0x05, ODT_ARR, &ODObjs.o_2340_encoder_position,           NULL},
    /* UC2 system */
    {0x2503, 0x01, ODT_VAR, &ODObjs.o_2503_uptime_seconds,             NULL},
    {0x2504, 0x01, ODT_VAR, &ODObjs.o_2504_free_heap_bytes,            NULL},
    {0x2505, 0x01, ODT_VAR, &ODObjs.o_2505_can_error_counter,          NULL},
    {0x2507, 0x01, ODT_VAR, &ODObjs.o_2507_reboot_command,             NULL},
    {0x0000, 0x00, 0,       NULL,                                       NULL}  /* terminator */
};

static OD_t _OD = {
    (sizeof(ODList) / sizeof(ODList[0])) - 1,
    &ODList[0]
};

OD_t *OD = &_OD;

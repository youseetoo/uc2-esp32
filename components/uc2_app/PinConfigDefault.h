#pragma once
// T4.1: PinConfig — pure-C board pin/parameter table (Phase 4 C conversion).
//
// The struct is the UNION of every field any board uses; a board only sets the
// fields it needs (in its apps/<app>/main/pinconfig.c), everything else stays
// at the disabled/zero default from pinconfig_defaults(). This makes
// `pinConfig` a plain C object readable by every C-converted module with no
// shim, while the 1181 existing `pinConfig.X` call sites compile unchanged.
//
// C-compatible: no default-member-initializers, no inheritance, no
// `static constexpr` (those became #defines below). `I2CControllerType` is a
// C enum. The single former `static constexpr I2C_ADDR_MCP4017` is a #define.
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "hal/gpio_types.h"

#ifndef MOTOR_AXIS_COUNT
#define MOTOR_AXIS_COUNT 4
#endif

// `disabled` sentinel for pin fields (was `const int8_t disabled = -1`).
#ifndef disabled
#define disabled (-1)
#endif

#define LED_BUILTIN 0 // for Xiao ESP32S3

// Former C++ `enum I2CControllerType : uint8_t`.
typedef enum {
    I2C_CONTROLLER_DISABLED = 0,
    I2C_CONTROLLER_ENCODER = 1,
    I2C_CONTROLLER_LINEAR_ENCODER = 2,
    I2C_CONTROLLER_HOME = 3,
    I2C_CONTROLLER_I2C = 4,
    I2C_CONTROLLER_TCA = 5,
    I2C_CONTROLLER_ANALOG = 6,
    I2C_CONTROLLER_DIGITAL = 7,
    I2C_CONTROLLER_BT = 8,
    I2C_CONTROLLER_LASER = 9,
    I2C_CONTROLLER_LED = 10,
    I2C_CONTROLLER_PID = 11,
    I2C_CONTROLLER_SCANNER = 12,
    I2C_CONTROLLER_WIFI = 13,
    I2C_CONTROLLER_HEAT = 14,
    I2C_CONTROLLER_GALVO = 15,
    I2C_CONTROLLER_MESSAGE = 16,
    I2C_CONTROLLER_DAC = 17,
    I2C_CONTROLLER_MOTOR = 18,
    I2C_CONTROLLER_DIAL = 19
} I2CControllerType;

// Former `static constexpr I2C_ADDR_MCP4017` (wiper chip address) — plain #define.
#define I2C_ADDR_MCP4017 0x2F

#ifdef __cplusplus
extern "C" {
#endif

struct PinConfig
{
    // ── Task / system ──
    bool          dumpHeap;
    uint16_t      DEFAULT_TASK_PRIORITY;
    uint16_t      MAIN_TASK_STACKSIZE;
    uint16_t      ANALOGJOYSTICK_TASK_STACKSIZE;
    uint16_t      STAGESCAN_TASK_STACKSIZE;
    uint16_t      HIDCONTROLLER_EVENT_STACK_SIZE;
    uint16_t      HTTP_MAX_URI_HANDLERS;
    uint16_t      BT_CONTROLLER_TASK_STACKSIZE;
    uint16_t      MOTOR_TASK_STACKSIZE;
    uint16_t      MOTOR_TASK_UPDATEWEBSOCKET_STACKSIZE;
    uint16_t      INTERRUPT_CONTROLLER_TASK_STACKSIZE;
    uint16_t      TCA_TASK_STACKSIZE;
    uint16_t      SCANNER_TASK_STACKSIZE;
    uint16_t      TEMPERATURE_TASK_STACKSIZE;
    unsigned long BAUDRATE;
    uint16_t      serialTimeout;
    const char   *pindefName;

    // ── CAN / debug ──
    bool          DEBUG_CAN_ISO_TP;
    uint32_t      CAN_ID_CURRENT;
    int8_t        AccelStepperMotorType;

    // ── Motor step/dir pins ──
    int8_t        MOTOR_X_DIR,  MOTOR_X_STEP;
    int8_t        MOTOR_Y_DIR,  MOTOR_Y_STEP;
    int8_t        MOTOR_Z_DIR,  MOTOR_Z_STEP;
    int8_t        MOTOR_A_DIR,  MOTOR_A_STEP;
    int8_t        MOTOR_B_DIR,  MOTOR_B_STEP;
    int8_t        MOTOR_C_DIR,  MOTOR_C_STEP;
    int8_t        MOTOR_D_DIR,  MOTOR_D_STEP;
    int8_t        MOTOR_E_DIR,  MOTOR_E_STEP;
    int8_t        MOTOR_F_DIR,  MOTOR_F_STEP;
    int8_t        MOTOR_G_DIR,  MOTOR_G_STEP;

    // ── Hard limits ──
    bool          hardLimitEnabledA, hardLimitPolarityA;
    bool          hardLimitEnabledX, hardLimitPolarityX;
    bool          hardLimitEnabledY, hardLimitPolarityY;
    bool          hardLimitEnabledZ, hardLimitPolarityZ;
    uint16_t      homeEndstopReleaseSteps;

    // ── Motor enable / homing ──
    int8_t        MOTOR_ENABLE;
    bool          MOTOR_ENABLE_INVERTED, MOTOR_AUTOENABLE;
    int8_t        HOME_X_DIR, HOME_Y_DIR, HOME_Z_DIR;
    int           HOME_X_SPEED, HOME_Y_SPEED, HOME_Z_SPEED;
    bool          HOME_X_SIG_INVERTED, HOME_Y_SIG_INVERTED, HOME_Z_SIG_INVERTED;

    // ── I2C / motor encoder pins ──
    gpio_num_t    I2C_INT;
    int8_t        MOTOR_X_0, MOTOR_X_1, MOTOR_Y_0, MOTOR_Y_1;
    int8_t        MOTOR_Z_0, MOTOR_Z_1, MOTOR_A_0, MOTOR_A_1;
    uint8_t       objectiveMotorAxis, focusMotorAxis;

    // ── LED ──
    int8_t        LED_PIN, LED_COUNT;
    bool          IS_STATUS_LED;
    uint16_t      RING_INNER_START,   RING_INNER_COUNT;
    uint16_t      RING_MIDDLE_START,  RING_MIDDLE_COUNT;
    uint16_t      RING_BIGGEST_START, RING_BIGGEST_COUNT;
    uint16_t      RING_OUTEST_START,  RING_OUTEST_COUNT;
    uint8_t       MATRIX_W, MATRIX_H;
    // Former `static constexpr` segment offsets (LED ring indexing, per-board in
    // pinconfig.c — all 0 by default = no illumination-board ring segments).
    uint16_t      SEGMENT_INNER_RIGHT_START,  SEGMENT_INNER_BOTTOM_START;
    uint16_t      SEGMENT_INNER_LEFT_START,   SEGMENT_INNER_TOP_START;
    uint16_t      SEGMENT_INNER_COUNT;
    uint16_t      SEGMENT_MIDDLE_RIGHT_START, SEGMENT_MIDDLE_BOTTOM_START;
    uint16_t      SEGMENT_MIDDLE_LEFT_START,  SEGMENT_MIDDLE_TOP_START;
    uint16_t      SEGMENT_MIDDLE_COUNT;
    uint16_t      SEGMENT_BIGGEST_RIGHT_START, SEGMENT_BIGGEST_BOTTOM_START;
    uint16_t      SEGMENT_BIGGEST_LEFT_START,  SEGMENT_BIGGEST_TOP_START;
    uint16_t      SEGMENT_BIGGEST_COUNT;
    uint16_t      SEGMENT_OUTEST_RIGHT_START, SEGMENT_OUTEST_BOTTOM_START;
    uint16_t      SEGMENT_OUTEST_LEFT_START,  SEGMENT_OUTEST_TOP_START;
    uint16_t      SEGMENT_OUTEST_COUNT;

    // ── Joystick / laser / digital out ──
    int8_t        ANLOG_JOYSTICK_X, ANLOG_JOYSTICK_Y;
    int8_t        LASER_1, LASER_2, LASER_3, LASER_4, LASER_0;
    int8_t        DIGITAL_OUT_1, DIGITAL_OUT_2, DIGITAL_OUT_3;
    bool          testLaserPinOnBoot;
    int           LASER_PWM_FREQUENCY, LASER_PWM_RESOLUTION;

    // ── Camera / digital in / DAC / analog ──
    int8_t        CAMERA_TRIGGER_PIN;
    bool          CAMERA_TRIGGER_INVERTED;
    int8_t        DIGITAL_IN_1, DIGITAL_IN_2, DIGITAL_IN_3;
    int8_t        dac_fake_1, dac_fake_2;
    int8_t        analogout_PIN_1, analogout_PIN_2, analogout_PIN_3;
    int8_t        analogin_PIN_0, analogin_PIN_1, analogin_PIN_2, analogin_PIN_3;
    int8_t        pid1, pid2, pid3;

    // ── PSX / joystick tuning ──
    const char   *PSX_MAC;
    int8_t        PSX_CONTROLLER_TYPE;
    int8_t        JOYSTICK_SPEED_MULTIPLIER, JOYSTICK_MAX_ILLU, JOYSTICK_SPEED_MULTIPLIER_Z;

    // ── WiFi / mDNS ──
    const char   *mSSID, *mPWD, *mSSIDAP, *hostname;
    bool          mAP;

    // ── Caliper / encoder ──
    int8_t        X_CAL_DATA, Y_CAL_DATA, Z_CAL_DATA;
    int8_t        X_CAL_CLK,  Y_CAL_CLK,  Z_CAL_CLK;
    int8_t        ENC_X_A, ENC_Y_A, ENC_Z_A, ENC_X_B, ENC_Y_B, ENC_Z_B;
    bool          ENC_X_encoderDirection, ENC_Y_encoderDirection, ENC_Z_encoderDirection;

    // ── TMC2209 ──
    int8_t        tmc_SW_RX, tmc_SW_TX, tmc_pin_diag;
    bool          TMC_DEBUG;
    int           tmc_microsteps, tmc_rms_current, tmc_stall_value, tmc_sgthrs;
    int           tmc_semin, tmc_semax, tmc_sedn, tmc_tcoolthrs, tmc_blank_time;
    int           tmc_toff, tmc_en_spreadcycle, tmc_tpwmthrs_sps, tmc_hstrt, tmc_hend;
    bool          tmc_internal_vref;
    int           tmc_hold_multiplier_pct;

    // ── Thermal (NTC) ──
    int8_t        thermal_PIN_0, thermal_PIN_1, thermal_PIN_2, thermal_PIN_3;
    int           thermal_series_r_ohm, thermal_supply_mv;
    float         thermal_warn_c, thermal_critical_c, thermal_recover_c;
    bool          thermal_enabled_default;

    // ── I2C / aux devices ──
    bool          isI2Cinitiated;
    int8_t        I2C_SCL, I2C_SDA, I2C_ADD_TCA;
    I2CControllerType I2C_CONTROLLER_TYPE;
    uint8_t       I2C_ADD_SLAVE, REMOTE_MOTOR_AXIS_ID, REMOTE_LASER_ID;
    uint8_t       I2C_ADD_MOT_X, I2C_ADD_MOT_Y, I2C_ADD_MOT_Z, I2C_ADD_MOT_A;
    uint8_t       I2C_ADD_LEX_MAT, I2C_ADD_LEX_PWM0, I2C_ADD_LEX_PWM1, I2C_ADD_LEX_PWM2, I2C_ADD_LEX_PWM3;
    uint8_t       I2C_ADD_M5_DIAL;

    // ── SPI / galvo ──
    int8_t        SPI_MOSI, SPI_MISO, SPI_SCK, SPI_CS;
    uint8_t       galvo_miso, galvo_sck, galvo_sdi, galvo_cs, galvo_ldac, galvo_laser;
    uint8_t       galvo_trig_pixel, galvo_trig_line, galvo_trig_frame;

    // ── Buzzer / temp / fan ──
    int8_t        BUZZER_PIN, DS28b20_PIN, FAN_TACHO_PIN;

    // ── CAN ──
    int8_t        CAN_TX, CAN_RX;
    uint8_t       CAN_ID_CENTRAL_NODE;
    uint8_t       CAN_ID_MOT_A, CAN_ID_MOT_X, CAN_ID_MOT_Y, CAN_ID_MOT_Z;
    uint8_t       CAN_ID_MOT_B, CAN_ID_MOT_C, CAN_ID_MOT_D, CAN_ID_MOT_E, CAN_ID_MOT_F, CAN_ID_MOT_G;
    uint8_t       CAN_ID_LASER_0, CAN_ID_LASER_1, CAN_ID_LASER_2, CAN_ID_LASER_3, CAN_ID_LASER_4;
    uint8_t       CAN_NODE_LASER[4];
    int8_t        CAN_SUBAXIS_LASER[4];
    uint8_t       CAN_ID_LED_0, CAN_ID_GALVO_0, CAN_ID_GPIO_0;
    int8_t        ROUTE_MOTOR[4], ROUTE_HOME[4], ROUTE_TMC[4], ROUTE_LASER[4];
    int8_t        ROUTE_LASER_4;
    uint8_t       CAN_NODE_LASER_4;
    int8_t        CAN_SUBAXIS_LASER_4, ROUTE_LED, ROUTE_GALVO;
    uint32_t      CAN_ID_SECONDARY;

    // ── Hybrid mode ──
    uint8_t       HYBRID_MOTOR_CAN_THRESHOLD, HYBRID_LASER_CAN_THRESHOLD;
    bool          HYBRID_LED_DUAL_OUTPUT;
    uint32_t      LED_AUTO_OFF_TIME_MS;
    uint16_t      LED_AUTO_OFF_INTENSITY_THRESHOLD;

    // ── E-stop / bus power ──
    int8_t        pinEmergencyExit, pinEmergencyExitPolarity;
    int8_t        BUSPOWER_OFF_PIN;

    // ── GPIO-CAN slave ──
    int8_t        GPIO_ESTOP_PIN, GPIO_COLLISION_ADC;
    uint16_t      GPIO_COLLISION_THRESHOLD_DEFAULT;
    uint8_t       GPIO_COLLISION_SENSITIVITY_DEFAULT;
    uint16_t      GPIO_ADC_FILTER_ALPHA_X1024;
    int8_t        MASTER_GPIO_SLAVE_NODE_ID, MASTER_PTZ_NODE_ID;
};

// Returns a PinConfig with every pin at `disabled` (-1) and every counter /
// routing / tuning value at its safe default. Each app's pinconfig.c starts
// from this and overrides only the fields the board actually uses.
struct PinConfig pinconfig_defaults(void);

extern const struct PinConfig pinConfig;

#ifdef __cplusplus
}  // extern "C"
#endif

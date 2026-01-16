#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"

#define CORE_DEBUG_LEVEL=5
#define LASER_CONTROLLER=1
#define DIGITAL_IN_CONTROLLER=1
#define MESSAGE_CONTROLLER=1
#define CAN_BUS_ENABLED=1
#define DIAL_CONTROLLER=1
#define MOTOR_CONTROLLER=1
#define HOME_MOTOR=1
#define BTHID=1 
#define BLUETOOTH=1	
#define TMC_CONTROLLER=1
#define OBJECTIVE_CONTROLLER=1
#define STAGE_SCAN=1
#define CAN_SEND_COMMANDS
#define MOTOR_AXIS_COUNT 10   
#define LED_CONTROLLER
#define GALVO_CONTROLLER
#define DAC_CONTROLLER
#define CAN_BUS_ENABLED 

struct UC2_3_CAN_HAT_Master_v2 : PinConfig
{
    // ---------------------------------------------------------------------
    // Board identity
    // ---------------------------------------------------------------------
    const char* pindefName = "UC2_3_CAN_HAT_Master_v2";
    const unsigned long BAUDRATE = 921600; //15200;

    // ---------------------------------------------------------------------
    // Core buses and addresses (per HAT design)
    // ---------------------------------------------------------------------
    // I2C (shared with 5V I2C header; solder-jumpers JP101/JP102 can bridge Pi<->ESP)
    int8_t I2C_SCL = GPIO_NUM_22;     // ESP32 I2C-1 SCL
    int8_t I2C_SDA = GPIO_NUM_21;     // ESP32 I2C-1 SDA

    // On-board I2C device addresses (for convenience)
    static constexpr uint8_t I2C_ADDR_INA226  = 0x46; // current sensor //TODO: implement via a temp_get (1,2,3...) command (via sensor_act/sensor_get)
    static constexpr uint8_t I2C_ADDR_TMP102_1 = 0x4A; // temperature sensor #1 //TODO: implement via a  sensor_act/sensor_get)
    static constexpr uint8_t I2C_ADDR_TMP102_2 = 0x4B; // temperature sensor #2 //TODO: implement via a  sensor_act/sensor_get)

    // Optional ALERT lines (open-drain on board) — leave disabled unless wired
    int8_t INA226_ALERT_PIN   = disabled;
    int8_t TMP102_1_ALERT_PIN = disabled;
    int8_t TMP102_2_ALERT_PIN = disabled;

    // ---------------------------------------------------------------------
    // Power / safety integration
    // ---------------------------------------------------------------------
    // Drives the high-current bus power MOSFET gate logic (HIGH = turn OFF)
    int8_t BUSPOWER_OFF_PIN = GPIO_NUM_4; // TODO: implement power control via status_act ((text "ESP pin\nHI turns device off")

    // Emergency STOP sense (input-only pin; HIGH = normal, LOW = E-STOP asserted)
    uint8_t pinEmergencyExit = GPIO_NUM_34; // TODO: Implement such that if we sense that the emergency button was pressed that we immediately stop all motors and switch off all lights and heaters, etc. we should print out a message to the log as well {"Emergency stop activated! Shutting down all systems."}
    uint8_t pinALERT = GPIO_NUM_35; // TODO: Implement => temperature sensor alert from the thermo in case it was previously configured 

    // TEMPERATURE : TMP102AIDRLR
    // I2C  ADDR:
    // GND = 0x48
    // V+ = 0x49
    // SDA = 0x4A
    // SCL = 0x4B
    // PCB: I2C addr: 0x4A
    // Ambient: I2C addr: 0x4B
    // FAN Control Pin I2C controllable 

    // Momentary local kill button exists on HAT; no pin here (handled in hardware gate)

    // ---------------------------------------------------------------------
    // CAN (ESP32 TWAI on-board; Pi-side MCP2515 is separate)
    // ---------------------------------------------------------------------
     // CAN
     int8_t CAN_TX = GPIO_NUM_17;
     int8_t CAN_RX = GPIO_NUM_18;
     uint32_t CAN_ID_CURRENT = CAN_ID_CENTRAL_NODE;
    bool DEBUG_CAN_ISO_TP = false;

    // ---------------------------------------------------------------------
    // Lighting / status
    // ---------------------------------------------------------------------
    int8_t LED_PIN   = GPIO_NUM_19; // on-board WS2812 data
    int16_t LED_COUNT = 1;
    bool IS_STATUS_LED = true; // on-board LED is used for status indication

    // ---------------------------------------------------------------------
    // Camera / trigger / panelboard IO
    // ---------------------------------------------------------------------
    // Camera I/O lines from schematic block
    int8_t CAM_IO0_IN  = GPIO_NUM_27; // “Cam In Line 0”
    int8_t CAM_IO1_OUT = disabled; // “Cam Out Line 1”
    int8_t CAM_IO2_IO  = disabled; // “Cam In/Out Line 2”

    // Use the dedicated OUT line as the default external camera trigger
    int8_t CAMERA_TRIGGER_PIN = CAM_IO0_IN;
    bool   CAMERA_TRIGGER_INVERTED = false;

    // Panelboard fan tachometer (available on connector; route to ESP if populated)
    int8_t FAN_TACHO_PIN = disabled;

    // ---------------------------------------------------------------------
    // Motors / encoders (directions preserved; step/dir/ena via expander on this rev)
    // ---------------------------------------------------------------------
    int8_t MOTOR_A_STEP = GPIO_NUM_0; // routed via expander on this electronics
    int8_t MOTOR_X_STEP = GPIO_NUM_0;
    int8_t MOTOR_Y_STEP = GPIO_NUM_0;
    int8_t MOTOR_Z_STEP = GPIO_NUM_0;

    bool isDualAxisZ = false;

    bool ENC_A_encoderDirection = true;
    bool ENC_X_encoderDirection = true;
    bool ENC_Y_encoderDirection = true;
    bool ENC_Z_encoderDirection = true;

    bool ENC_A_motorDirection = true;
    bool ENC_X_motorDirection = true;
    bool ENC_Y_motorDirection = true;
    bool ENC_Z_motorDirection = true;

    bool MOTOR_ENABLE_INVERTED = true;
    bool MOTOR_AUTOENABLE = true;
    int8_t AccelStepperMotorType = 1;

    // Digital inputs (reserved)
    int8_t DIGITAL_IN_1 = disabled;
    int8_t DIGITAL_IN_2 = disabled;
    int8_t DIGITAL_IN_3 = disabled;

    // DAC placeholders (legacy)
    int8_t dac_fake_1 = disabled; // RESET-ABORT stub
    int8_t dac_fake_2 = disabled; // Coolant stub

    // BUZZER (the HAT v2 has a buzzer on the board that can be turned on via GPIO25)
    int8_t BUZZER_PIN = GPIO_NUM_25; // active HIGH // TODO: implement buzzer control via new buzzer controller interface (buzzer_act)

    // ---------------------------------------------------------------------
    // Encoders (external calipers etc.)
    // ---------------------------------------------------------------------
    int8_t ENC_X_A = disabled;
    int8_t ENC_Y_A = disabled;
    int8_t ENC_Z_A = disabled;
    int8_t ENC_X_B = disabled;
    int8_t ENC_Y_B = disabled;
    int8_t ENC_Z_B = disabled;

    // ---------------------------------------------------------------------
    // Motion tuning
    // ---------------------------------------------------------------------
    const int32_t MAX_ACCELERATION_A = 600000;
    const int32_t DEFAULT_ACCELERATION = 100000;

    // ---------------------------------------------------------------------
    // Objective defaults
    // ---------------------------------------------------------------------
    uint8_t  objectiveMotorAxis = 0; // 0=A, 1=X, 2=Y, 3=Z
    uint8_t  focusMotorAxis     = 3;
    uint32_t objectivePositionX0 = 5000;
    uint32_t objectivePositionX1 = 35000;
    int8_t   objectiveHomeDirection        = -1;
    int8_t   objectiveHomeEndStopPolarity  = 0;

    // ---------------------------------------------------------------------
    // WiFi defaults
    // ---------------------------------------------------------------------
    const char* mSSID   = "UC2x-CAN-HAT";
    const char* mPWD    = "";
    bool        mAP     = true;
    const char* mSSIDAP = "UC2";
    const char* hostname = "youseetoo";

    // ---------------------------------------------------------------------
    // UI / joystick
    // ---------------------------------------------------------------------
    int8_t JOYSTICK_SPEED_MULTIPLIER   = 10;
    int8_t JOYSTICK_MAX_ILLU           = 255;
    int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 1;

    // ---------------------------------------------------------------------
    // Lasers (not wired on this HAT)
    // ---------------------------------------------------------------------
    int8_t LASER_1 = disabled;
    int8_t LASER_2 = disabled;
    int8_t LASER_3 = disabled;
};

const UC2_3_CAN_HAT_Master_v2 pinConfig;

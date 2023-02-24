#pragma once

// default Pin structure
struct PinConfig
{
    // motor x direction pin
    int MOTOR_X_DIR = 0;
    // motor x step pin
    int MOTOR_X_STEP = 0;
    // motor y direction pin
    int MOTOR_Y_DIR = 0;
    // motor y step pin
    int MOTOR_Y_STEP = 0;
    // motor z direction pin
    int MOTOR_Z_DIR = 0;
    // motor z step pin
    int MOTOR_Z_STEP = 0;
    // motor a direction pin
    int MOTOR_A_DIR = 0;
    // motor a step pin
    int MOTOR_A_STEP = 0;
    // motor enable power
    int MOTOR_ENABLE = 0;
    // motor power pin is inverted
    bool MOTOR_ENABLE_INVERTED = false;

    // led control pin
    int LED_PIN = 0;
    // led count from the strip
    int LED_COUNT = 0;

    // anlog joystick x pin
    int ANLOG_JOYSTICK_X = 0;
    // analog joystick y pin
    int ANLOG_JOYSTICK_Y = 0;

    int LASER_1 = 0;
    int LASER_2 = 0;
    int LASER_3 = 0; // GPIO_NUM_21

    int PIN_DEF_END_X = 0;
    int PIN_DEF_END_Y = 0;
    int PIN_DEF_END_Z = 0;

    int DIGITAL_OUT_1 = 0;
    int DIGITAL_OUT_2 = 0;
    int DIGITAL_OUT_3 = 0;

    int DIGITAL_IN_1 = 0;
    int DIGITAL_IN_2 = 0;
    int DIGITAL_IN_3 = 0;

    // GALVos are always connected to 25/26 
    int dac_fake_1 = 0; // RESET-ABORT just toggles between 1 and 0
    int dac_fake_2 = 0; // Coolant

    // analogout out (e.g. Lenses)
    int analogout_PIN_1;
    int analogout_PIN_2;
    int analogout_PIN_3;

    int analogin_PIN_0 = 0;
    int analogin_PIN_1 = 0;
    int analogin_PIN_2 = 0;
    int analogin_PIN_3 = 0;

    int pid1 = 0;
    int pid2 = 0;
    int pid3 = 0;

    String PSX_MAC = "";
    int PSX_CONTROLLER_TYPE = 0; // 1 = ps3, 2 =ps4

    bool enableScanner = false;
    bool enableBlueTooth = false;
    bool enableWifi = false;
};

struct XYZ_MOTOR_JOYSTICK : PinConfig
{
    int MOTOR_X_DIR = 16;
    int MOTOR_X_STEP = 26;
    int MOTOR_Y_DIR = 27;
    int MOTOR_Y_STEP = 25;
    int MOTOR_Z_DIR = 14;
    int MOTOR_Z_STEP = 17;
    int MOTOR_ENABLE = 12;
    bool MOTOR_ENABLE_INVERTED = true;

    int ANLOG_JOYSTICK_X = 35;
    int ANLOG_JOYSTICK_Y = 34;

    String PSX_MAC = "4c:63:71:cd:31:a0";
    int PSX_CONTROLLER_TYPE = 2;
};

struct XYZ_MOTOR_ENDSTOP_JOYSTICK : XYZ_MOTOR_JOYSTICK
{
    int PIN_DEF_END_X = 12;
    int PIN_DEF_END_Y = 13;
    int PIN_DEF_END_Z = 5;
};

struct X_MOTOR_64LED : PinConfig
{
    int MOTOR_X_DIR = 21;
    int MOTOR_X_STEP = 19;
    int MOTOR_ENABLE = 18;
    bool MOTOR_ENABLE_INVERTED = true;

    int LED_PIN = 27;
    int LED_COUNT = 64;
};

struct HoLiSheet : PinConfig
{
    int MOTOR_Z_DIR = GPIO_NUM_14;
    int MOTOR_Z_STEP = GPIO_NUM_17;
    int MOTOR_ENABLE = GPIO_NUM_12;
    bool MOTOR_ENABLE_INVERTED = true;

    int LASER_1 = GPIO_NUM_18;
    int LASER_2 = GPIO_NUM_19;
    int LASER_3 = 0; // GPIO_NUM_21

    int LED_PIN = GPIO_NUM_4;
    int LED_COUNT = 64;

    int PIN_DEF_END_X = GPIO_NUM_13;
    int PIN_DEF_END_Y = GPIO_NUM_5;
    int PIN_DEF_END_Z = GPIO_NUM_23;

    String PSX_MAC = "1a:2b:3c:01:01:01";
    int PSX_CONTROLLER_TYPE = 2;
};

struct UC2_1 : PinConfig
{
    // UC2 STandalone V1
    int MOTOR_A_DIR = GPIO_NUM_21;
    int MOTOR_X_DIR = GPIO_NUM_33;
    int MOTOR_Y_DIR = GPIO_NUM_16;
    int MOTOR_Z_DIR = GPIO_NUM_14;
    int MOTOR_A_STEP = GPIO_NUM_22;
    int MOTOR_X_STEP = GPIO_NUM_2;
    int MOTOR_Y_STEP = GPIO_NUM_27;
    int MOTOR_Z_STEP = GPIO_NUM_12;
    int MOTOR_ENABLE = GPIO_NUM_13;
    bool MOTOR_ENABLE_INVERTED = true;

    int LASER_1 = GPIO_NUM_4;
    int LASER_2 = GPIO_NUM_15;
    int LASER_3 = 0; // GPIO_NUM_21

    int LED_PIN = GPIO_NUM_17;
    int LED_COUNT = 25;

    int PIN_DEF_END_X = GPIO_NUM_10;
    int PIN_DEF_END_Y = GPIO_NUM_11;
    int PIN_DEF_END_Z = 0;

    String PSX_MAC = "1a:2b:3c:01:01:01";
    int PSX_CONTROLLER_TYPE = 2;
};

struct UC2_2 : PinConfig
{
    // UC2 STandalone V2
    int MOTOR_A_DIR = GPIO_NUM_21;
    int MOTOR_X_DIR = GPIO_NUM_33;
    int MOTOR_Y_DIR = GPIO_NUM_16;
    int MOTOR_Z_DIR = GPIO_NUM_14;
    int MOTOR_A_STEP = GPIO_NUM_22;
    int MOTOR_X_STEP = GPIO_NUM_2;
    int MOTOR_Y_STEP = GPIO_NUM_27;
    int MOTOR_Z_STEP = GPIO_NUM_12;
    int MOTOR_ENABLE = GPIO_NUM_13;
    bool MOTOR_ENABLE_INVERTED = true;

    int LASER_1 = GPIO_NUM_17;
    int LASER_2 = GPIO_NUM_4;
    int LASER_3 = GPIO_NUM_15;

    int LED = GPIO_NUM_32;
    int LED_NUM = 25;

    int PIN_DEF_END_X = GPIO_NUM_34;
    int PIN_DEF_END_Y = GPIO_NUM_35;
    int PIN_DEF_END_Z = 0;

    String PSX_MAC = "1a:2b:3c:01:01:01";
    int PSX_CONTROLLER_TYPE = 2;
};

struct WEMOS : PinConfig
{
    // ESP32-WEMOS D1 R32
    int MOTOR_A_DIR = GPIO_NUM_23; // Bridge from Endstop Z to Motor A (GPIO_NUM_23)
    int MOTOR_X_DIR = GPIO_NUM_16;
    int MOTOR_Y_DIR = GPIO_NUM_27;
    int MOTOR_Z_DIR = GPIO_NUM_14;
    int MOTOR_A_STEP = GPIO_NUM_5; // Bridge from Endstop Y to Motor A (GPIO_NUM_5)
    int MOTOR_X_STEP = GPIO_NUM_26;
    int MOTOR_Y_STEP = GPIO_NUM_25;
    int MOTOR_Z_STEP = GPIO_NUM_17;
    int MOTOR_ENABLE = GPIO_NUM_12;
    bool MOTOR_ENABLE_INVERTED = true;

    int LASER_1 = GPIO_NUM_18;
    int LASER_2 = GPIO_NUM_19;
    int LASER_3 = 0; // GPIO_NUM_21

    int LED = GPIO_NUM_4;
    int LED_NUM = 64;

    int PIN_DEF_END_X = GPIO_NUM_13;
    int PIN_DEF_END_Y = 0; // GPIO_NUM_5;
    int PIN_DEF_END_Z = 0; // GPIO_NUM_23;

    String PSX_MAC = "1a:2b:3c:01:01:01";
    int PSX_CONTROLLER_TYPE = 2;
};

//static XYZ_MOTOR_JOYSTICK pinConfig;
static XYZ_MOTOR_ENDSTOP_JOYSTICK pinConfig;
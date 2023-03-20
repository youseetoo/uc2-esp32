#pragma once

// default Pin structure
struct PinConfig
{
    String pindefName = "pindef";
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

    // LED_PINcontrol pin
    int LED_PIN = 0;
    // LED_PINcount from the strip
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
    String pindefName = "XYZ_MOTOR_JOYSTICK";
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
    String pindefName = "XYZ_MOTOR_ENDSTOP_JOYSTICK";
    int PIN_DEF_END_X = 12;
    int PIN_DEF_END_Y = 13;
    int PIN_DEF_END_Z = 5;
};

struct X_MOTOR_64LED_PIN: PinConfig
{
    String pindefName = "X_Motor_64LED";
    int MOTOR_X_DIR = 21;
    int MOTOR_X_STEP = 19;
    int MOTOR_ENABLE = 18;
    bool MOTOR_ENABLE_INVERTED = true;

    int LED_PIN = 27;
    int LED_COUNT = 64;
};

struct HoLiSheet : PinConfig
{
    String pindefName = "HoLiSheet";
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
    String pindefName = "UC2_1";
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



struct UC2_Insert : PinConfig
{
    String pindefName = "UC2UC2_Insert";

    // UC2 STandalone V2
    int MOTOR_A_DIR = 0;
    int MOTOR_X_DIR = GPIO_NUM_19;
    int MOTOR_Y_DIR = 0;
    int MOTOR_Z_DIR = 0;
    int MOTOR_A_STEP = 0;
    int MOTOR_X_STEP = GPIO_NUM_35;
    int MOTOR_Y_STEP = 0;
    int MOTOR_Z_STEP = 0;
    int MOTOR_ENABLE = GPIO_NUM_33;
    bool MOTOR_ENABLE_INVERTED = true;

    int LASER_1 = 0;
    int LASER_2 = 0;
    int LASER_3 = 0;

    int LED_PIN= GPIO_NUM_4;
    int LED_COUNT = 64;

    int PIN_DEF_END_X = 0;
    int PIN_DEF_END_Y = 0;
    int PIN_DEF_END_Z = 0;

    String PSX_MAC = "1a:2b:3c:01:01:01";
    int PSX_CONTROLLER_TYPE = 2;
    boolean enableBlueTooth = true;


};


struct UC2_2 : PinConfig
{
    String pindefName = "UC2_2";

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

    int LED_PIN= GPIO_NUM_32;
    int LED_COUNT = 64;

    int PIN_DEF_END_X = GPIO_NUM_34;
    int PIN_DEF_END_Y = GPIO_NUM_35;
    int PIN_DEF_END_Z = 0;

    String PSX_MAC = "1a:2b:3c:01:01:01";
    int PSX_CONTROLLER_TYPE = 2; // 1: PS3, 2: PS4
    boolean enableBlueTooth = true;


};

struct UC2e : PinConfig
{
    String pindefName = "UC2e";

    // UC2e (UC2-express-bus) pinout of the SUES system (modules and baseplane) dictated by the spec for the ESP32 module pinout
    // that document maps ESP32 pins to UC2e pins (and pin locations on the physical PCIe x1 connector
    // and assigns functions compatible with the ESP32 GPIO's hardware capabilities.
    // See https://github.com/openUC2/UC2-SUES/blob/main/uc2-express-bus.md#esp32-module-pinout

    // Stepper Motor control pins
    int MOTOR_X_DIR = GPIO_NUM_12; // STEP-1_DIR, PP_05, PCIe A8
    int MOTOR_Y_DIR = GPIO_NUM_13; // STEP-2_DIR, PP_06, PCIe A9
    int MOTOR_Z_DIR = GPIO_NUM_14; // STEP-3_DIR, PP_07, PCIe A10
    int MOTOR_A_DIR = GPIO_NUM_15; // STEP-4_DIR, PP_08, PCIe A11
    int MOTOR_X_STEP = GPIO_NUM_0; // STEP-1_STP, PP_01, PCIe A1
    int MOTOR_Y_STEP = GPIO_NUM_2; // STEP-2_STP, PP_02, PCIe A5
    int MOTOR_Z_STEP = GPIO_NUM_4; // STEP-3_STP, PP_03, PCIe A6
    int MOTOR_A_STEP = GPIO_NUM_5; // STEP-4_STP, PP_04, PCIe A7
    int MOTOR_ENABLE = GPIO_NUM_17; // STEP_EN, PP_09, PCIe A13. Enables/disables all stepper drivers' output.
    bool MOTOR_ENABLE_INVERTED = true; // When set to a logic high, the outputs are disabled.

    // LED pins (addressable - WS2812 and equivalent)
    int NEOPIXEL_PIN = GPIO_NUM_18; // NEOPIXEL, PP_10, PCIe A14. I changed the name from LED_PIN to indicate that it's for an adressable LED chain
    int LED_COUNT = 64; // I havent touched this

    // Main power switch for all the AUX modules of the baseplane
    int BASEPLANE_ENABLE = GPIO_NUM_16; // UC2e_EN, ENABLE, PCIe A17. ESP32 module in Controller slot of baseplane switches power off all AUX modules. HIGH activates AUX modules.

    // Pulse Width Modulation pins
    int PWM_1 = GPIO_NUM_19; // PWM_1, PP_11, PCIe A16
    int PWM_2 = GPIO_NUM_23; // PWM_2, PP_12, PCIe B8
    int PWM_3 = GPIO_NUM_27; // PWM_3, PP_15, PCIe B11
    int PWM_4 = GPIO_NUM_32; // PWM_4, PP_16, PCIe B12

    // Digital-to-Analog converter pins
    int DAC_1 = GPIO_NUM_25; // DAC_1, PP_13, PCIe B9
    int DAC_2 = GPIO_NUM_27; // DAC_2, PP_14, PCIe B10

    // Other pins that sense PCIe/housekeeping things
    int UC2E_OWN_ADDR = GPIO_NUM_33; // Analog input! The voltage indicates into which slot of the baseplane the ESP32 module is plugged.
    int UC2E_VOLTAGE = GPIO_NUM_34; // Analog input! Can measure the (12V) baseplane power via a voltage divider.

    // Sensor pins, Analog-to-digital converter enabled
    int SENSOR_1 = GPIO_NUM_35; // SENSOR_1, PP_17, PCIe B14
    int SENSOR_2 = GPIO_NUM_36; // SENSOR_2, PP_18, PCIe B15
    int SENSOR_3 = GPIO_NUM_39; // SENSOR_3, PP_19, PCIe B17

    // I2C pins, digital bus to control various functions of the baseplane and SUES modules
    int I2C_SDA = GPIO_NUM_21; // I2C_SDA, SDA, PCIe B6
    int I2C_SCL = GPIO_NUM_22; // I2C_SCL, SCL, PCIe B5

};

struct UC2_WEMOS : PinConfig
{
    String pindefName = "UC2_WEMOS";
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

    int LED_PIN= GPIO_NUM_4;
    int LED_COUNT = 64;

    int PIN_DEF_END_X = GPIO_NUM_13;
    int PIN_DEF_END_Y = 0; // GPIO_NUM_5;
    int PIN_DEF_END_Z = 0; // GPIO_NUM_23;

    String PSX_MAC = "1a:2b:3c:01:01:01";
    int PSX_CONTROLLER_TYPE = 2;
};

//static XYZ_MOTOR_JOYSTICK pinConfig;
static UC2_2 pinConfig; //_WEMOS pinConfig; //_2 pinConfig; //_2 pinConfig;

//{"task":"/state_get"}
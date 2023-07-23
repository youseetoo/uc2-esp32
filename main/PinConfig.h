#pragma once
#include "esp_err.h"
#include "Arduino.h"
// default Pin structure
struct PinConfig
{
     String pindefName = "pindef";
    // motor x direction pin
     uint8_t MOTOR_X_DIR = 0;
    // motor x step pin
     uint8_t MOTOR_X_STEP = 0;
    // motor y direction pin
     uint8_t MOTOR_Y_DIR = 0;
    // motor y step pin
     uint8_t MOTOR_Y_STEP = 0;
    // motor z direction pin
     uint8_t MOTOR_Z_DIR = 0;
    // motor z step pin
     uint8_t MOTOR_Z_STEP = 0;
    // motor a direction pin
     uint8_t MOTOR_A_DIR = 0;
    // motor a step pin
     uint8_t MOTOR_A_STEP = 0;
    // motor enable power
     uint8_t MOTOR_ENABLE = 0;
    // motor power pin is inverted
     bool MOTOR_ENABLE_INVERTED = false;
    // keep motors on after idle time?
     bool MOTOR_AUTOENABLE = true;

    // Rototar settings for 28byj-48
     bool ROTATOR_ENABLE = true;
     uint8_t ROTATOR_A_0 = 0;
     uint8_t ROTATOR_A_1 = 0;
     uint8_t ROTATOR_A_2 = 0;
     uint8_t ROTATOR_A_3 = 0;
     uint8_t ROTATOR_X_0 = 0;
     uint8_t ROTATOR_X_1 = 0;
     uint8_t ROTATOR_X_2 = 0;
     uint8_t ROTATOR_X_3 = 0;
     uint8_t ROTATOR_Y_0 = 0;
     uint8_t ROTATOR_Y_1 = 0;
     uint8_t ROTATOR_Y_2 = 0;
     uint8_t ROTATOR_Y_3 = 0;
     uint8_t ROTATOR_Z_0 = 0;
     uint8_t ROTATOR_Z_1 = 0;
     uint8_t ROTATOR_Z_2 = 0;
     uint8_t ROTATOR_Z_3 = 0;
    
    // LED_PINcontrol pin
     uint8_t LED_PIN = 0;
    // LED_PINcount from the strip
     uint8_t LED_COUNT = 0;

    // anlog joystick x pin
     uint8_t ANLOG_JOYSTICK_X = 0;
    // analog joystick y pin
     uint8_t ANLOG_JOYSTICK_Y = 0;

     uint8_t LASER_1 = 0;
     uint8_t LASER_2 = 0;
     uint8_t LASER_3 = 0; // GPIO_NUM_21

     uint8_t PIN_DEF_END_X = 0;
     uint8_t PIN_DEF_END_Y = 0;
     uint8_t PIN_DEF_END_Z = 0;

     uint8_t DIGITAL_OUT_1 = 0;
     uint8_t DIGITAL_OUT_2 = 0;
     uint8_t DIGITAL_OUT_3 = 0;

     uint8_t DIGITAL_IN_1 = 0;
     uint8_t DIGITAL_IN_2 = 0;
     uint8_t DIGITAL_IN_3 = 0;

    // GALVos are always connected to 25/26
     uint8_t dac_fake_1 = 0; // RESET-ABORT just toggles between 1 and 0
     uint8_t dac_fake_2 = 0; // Coolant

    // analogout out (e.g. Lenses)
     uint8_t analogout_PIN_1 = 0;
     uint8_t analogout_PIN_2 = 0;
     uint8_t analogout_PIN_3 = 0;

     uint8_t analogin_PIN_0 = 0;
     uint8_t analogin_PIN_1 = 0;
     uint8_t analogin_PIN_2 = 0;
     uint8_t analogin_PIN_3 = 0;

     uint8_t pid1 = 0;
     uint8_t pid2 = 0;
     uint8_t pid3 = 0;

     String PSX_MAC = "";
     uint8_t PSX_CONTROLLER_TYPE = 0; // 1 = ps3, 2 =ps4

     bool enableScanner = false;
     bool enableBlueTooth = true;
     bool useBtHID = false; //enabling this disable psxcontroller
     bool enableWifi = true;

     uint8_t JOYSTICK_SPEED_MULTIPLIER = 10;
     uint8_t JOYSTICK_MAX_ILLU = 100;
     uint8_t JOYSTICK_SPEED_MULTIPLIER_Z  = 10;

    // WIFI
     String mSSID = "Uc2";
	 String mPWD = "";
     bool mAP = true;
     String mSSIDAP = "UC2";
	 String hostname = "youseetoo";

     // for caliper
     uint8_t X_CAL_DATA = 0;
     uint8_t Y_CAL_DATA = 0;
     uint8_t Z_CAL_DATA = 0;

    // I2c
     uint8_t I2C_SCL = 0;
     uint8_t I2C_SDA = 0;
     uint8_t I2C_ADD = 0;

    // SPI
     uint8_t SPI_MOSI = 0;
     uint8_t SPI_MISO = 0;
     uint8_t SPI_SCK = 0;
     uint8_t SPI_CS = 0;
};

struct XYZ_MOTOR_JOYSTICK : PinConfig
{
     String pindefName = "XYZ_MOTOR_JOYSTICK";
     uint8_t MOTOR_X_DIR = 16;
     uint8_t MOTOR_X_STEP = 26;
     uint8_t MOTOR_Y_DIR = 27;
     uint8_t MOTOR_Y_STEP = 25;
     uint8_t MOTOR_Z_DIR = 14;
     uint8_t MOTOR_Z_STEP = 17;
     uint8_t MOTOR_ENABLE = 12;
     bool MOTOR_ENABLE_INVERTED = true;

     uint8_t ANLOG_JOYSTICK_X = 0;
     uint8_t ANLOG_JOYSTICK_Y = 0;

     bool useBtHID = true;
};

struct XYZ_MOTOR_ENDSTOP_JOYSTICK : XYZ_MOTOR_JOYSTICK
{
     String pindefName = "XYZ_MOTOR_ENDSTOP_JOYSTICK";
     uint8_t PIN_DEF_END_X = 12;
     uint8_t PIN_DEF_END_Y = 13;
     uint8_t PIN_DEF_END_Z = 5;
};

struct X_MOTOR_64LED_PIN: PinConfig
{
     String pindefName = "X_Motor_64LED";
     uint8_t MOTOR_X_DIR = 21;
     uint8_t MOTOR_X_STEP = 19;
     uint8_t MOTOR_ENABLE = 18;
     bool MOTOR_ENABLE_INVERTED = true;

     uint8_t LED_PIN = 27;
     uint8_t LED_COUNT = 64;
};

struct HoLiSheet : PinConfig
{
     String pindefName = "HoLiSheet";
     uint8_t MOTOR_Z_DIR = GPIO_NUM_14;
     uint8_t MOTOR_Z_STEP = GPIO_NUM_17;
     uint8_t MOTOR_ENABLE = GPIO_NUM_12;
     bool MOTOR_ENABLE_INVERTED = true;

     uint8_t LASER_1 = GPIO_NUM_18;
     uint8_t LASER_2 = GPIO_NUM_19;
     uint8_t LASER_3 = 0; // GPIO_NUM_21

     uint8_t LED_PIN = GPIO_NUM_4;
     uint8_t LED_COUNT = 64;

     uint8_t PIN_DEF_END_X = GPIO_NUM_13;
     uint8_t PIN_DEF_END_Y = GPIO_NUM_5;
     uint8_t PIN_DEF_END_Z = GPIO_NUM_23;

     String PSX_MAC = "1a:2b:3c:01:01:01";
     uint8_t PSX_CONTROLLER_TYPE = 2;
};

struct UC2_1 : PinConfig
{
     String pindefName = "UC2_1";
    // UC2 STandalone V1
     uint8_t MOTOR_A_DIR = GPIO_NUM_21;
     uint8_t MOTOR_X_DIR = GPIO_NUM_33;
     uint8_t MOTOR_Y_DIR = GPIO_NUM_16;
     uint8_t MOTOR_Z_DIR = GPIO_NUM_14;
     uint8_t MOTOR_A_STEP = GPIO_NUM_22;
     uint8_t MOTOR_X_STEP = GPIO_NUM_2;
     uint8_t MOTOR_Y_STEP = GPIO_NUM_27;
     uint8_t MOTOR_Z_STEP = GPIO_NUM_12;
     uint8_t MOTOR_ENABLE = GPIO_NUM_13;
     bool MOTOR_ENABLE_INVERTED = true;

     uint8_t LASER_1 = GPIO_NUM_4;
     uint8_t LASER_2 = GPIO_NUM_15;
     uint8_t LASER_3 = 0; // GPIO_NUM_21

     uint8_t LED_PIN = GPIO_NUM_17;
     uint8_t LED_COUNT = 25;

     uint8_t PIN_DEF_END_X = GPIO_NUM_10;
     uint8_t PIN_DEF_END_Y = GPIO_NUM_11;
     uint8_t PIN_DEF_END_Z = 0;

     String PSX_MAC = "1a:2b:3c:01:01:05";
     uint8_t PSX_CONTROLLER_TYPE = 1;

     uint8_t JOYSTICK_SPEED_MULTIPLIER = 20;
     boolean enableBlueTooth = true;

};



struct UC2_Insert : PinConfig
{
     String pindefName = "UC2UC2_Insert";

    // UC2 STandalone V2
     uint8_t MOTOR_A_DIR = 0;
     uint8_t MOTOR_X_DIR = GPIO_NUM_19;
     uint8_t MOTOR_Y_DIR = 0;
     uint8_t MOTOR_Z_DIR = 0;
     uint8_t MOTOR_A_STEP = 0;
     uint8_t MOTOR_X_STEP = GPIO_NUM_35;
     uint8_t MOTOR_Y_STEP = 0;
     uint8_t MOTOR_Z_STEP = 0;
     uint8_t MOTOR_ENABLE = GPIO_NUM_33;
     bool MOTOR_ENABLE_INVERTED = true;

     uint8_t LASER_1 = 0;
     uint8_t LASER_2 = 0;
     uint8_t LASER_3 = 0;

     uint8_t LED_PIN= GPIO_NUM_4;
     uint8_t LED_COUNT = 64;

     uint8_t PIN_DEF_END_X = 0;
     uint8_t PIN_DEF_END_Y = 0;
     uint8_t PIN_DEF_END_Z = 0;

     String PSX_MAC = "1a:2b:3c:01:01:01";
     uint8_t PSX_CONTROLLER_TYPE = 2;
     boolean enableBlueTooth = true;


};


struct UC2_2 : PinConfig
{
     String pindefName = "UC2_2";

    // UC2 STandalone V2
     uint8_t MOTOR_A_DIR = GPIO_NUM_21;
     uint8_t MOTOR_X_DIR = GPIO_NUM_33;
     uint8_t MOTOR_Y_DIR = GPIO_NUM_16;
     uint8_t MOTOR_Z_DIR = GPIO_NUM_14;
     uint8_t MOTOR_A_STEP = GPIO_NUM_22;
     uint8_t MOTOR_X_STEP = GPIO_NUM_2;
     uint8_t MOTOR_Y_STEP = GPIO_NUM_27;
     uint8_t MOTOR_Z_STEP = GPIO_NUM_12;
     uint8_t MOTOR_ENABLE = GPIO_NUM_13;
     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = true;

     uint8_t LASER_1 = GPIO_NUM_17;
     uint8_t LASER_2 = GPIO_NUM_4;
     uint8_t LASER_3 = GPIO_NUM_15;

     uint8_t LED_PIN= GPIO_NUM_32;
     uint8_t LED_COUNT = 64;

    // FIXME: Is this redudant?! 
     uint8_t PIN_DEF_END_X = GPIO_NUM_34;
     uint8_t PIN_DEF_END_Y = GPIO_NUM_39;
     uint8_t PIN_DEF_END_Z = 0;
     uint8_t DIGITAL_IN_1 = PIN_DEF_END_X;
     uint8_t DIGITAL_IN_2 = PIN_DEF_END_Y;
     uint8_t DIGITAL_IN_3 = PIN_DEF_END_Z;

     String PSX_MAC = "1a:2b:3c:01:01:04";
     uint8_t PSX_CONTROLLER_TYPE = 2; // 1: PS3, 2: PS4
     bool enableBlueTooth = true;


     uint8_t JOYSTICK_SPEED_MULTIPLIER = 30;
     uint8_t JOYSTICK_MAX_ILLU = 100;
     uint8_t JOYSTICK_SPEED_MULTIPLIER_Z  = 30;


};

struct UC2e : PinConfig
{
     String pindefName = "UC2e";

    // UC2e (UC2-express-bus) pinout of the SUES system (modules and baseplane) dictated by the spec for the ESP32 module pinout
    // that document maps ESP32 pins to UC2e pins (and pin locations on the physical PCIe x1 connector
    // and assigns functions compatible with the ESP32 GPIO's hardware capabilities.
    // See https://github.com/openUC2/UC2-SUES/blob/main/uc2-express-bus.md#esp32-module-pinout

    // Stepper Motor control pins
     uint8_t MOTOR_X_DIR = GPIO_NUM_12; // STEP-1_DIR, PP_05, PCIe A8
     uint8_t MOTOR_Y_DIR = GPIO_NUM_13; // STEP-2_DIR, PP_06, PCIe A9
     uint8_t MOTOR_Z_DIR = GPIO_NUM_14; // STEP-3_DIR, PP_07, PCIe A10
     uint8_t MOTOR_A_DIR = GPIO_NUM_15; // STEP-4_DIR, PP_08, PCIe A11
     uint8_t MOTOR_X_STEP = GPIO_NUM_0; // STEP-1_STP, PP_01, PCIe A1
     uint8_t MOTOR_Y_STEP = GPIO_NUM_2; // STEP-2_STP, PP_02, PCIe A5
     uint8_t MOTOR_Z_STEP = GPIO_NUM_4; // STEP-3_STP, PP_03, PCIe A6
     uint8_t MOTOR_A_STEP = GPIO_NUM_5; // STEP-4_STP, PP_04, PCIe A7
     uint8_t MOTOR_ENABLE = GPIO_NUM_17; // STEP_EN, PP_09, PCIe A13. Enables/disables all stepper drivers' output.
     bool MOTOR_ENABLE_INVERTED = true; // When set to a logic high, the outputs are disabled.

    // LED pins (addressable - WS2812 and equivalent)
     uint8_t NEOPIXEL_PIN = GPIO_NUM_18; // NEOPIXEL, PP_10, PCIe A14. I changed the name from LED_PIN to indicate that it's for an adressable LED chain
     uint8_t LED_COUNT = 64; // I havent touched this

    // Main power switch for all the AUX modules of the baseplane
     uint8_t BASEPLANE_ENABLE = GPIO_NUM_16; // UC2e_EN, ENABLE, PCIe A17. ESP32 module in Controller slot of baseplane switches power off all AUX modules. HIGH activates AUX modules.

    // Pulse Width Modulation pins
     uint8_t PWM_1 = GPIO_NUM_19; // PWM_1, PP_11, PCIe A16
     uint8_t PWM_2 = GPIO_NUM_23; // PWM_2, PP_12, PCIe B8
     uint8_t PWM_3 = GPIO_NUM_27; // PWM_3, PP_15, PCIe B11
     uint8_t PWM_4 = GPIO_NUM_32; // PWM_4, PP_16, PCIe B12

    // Digital-to-Analog converter pins
     uint8_t DAC_1 = GPIO_NUM_25; // DAC_1, PP_13, PCIe B9
     uint8_t DAC_2 = GPIO_NUM_27; // DAC_2, PP_14, PCIe B10

    // Other pins that sense PCIe/housekeeping things
     uint8_t UC2E_OWN_ADDR = GPIO_NUM_33; // Analog input! The voltage indicates into which slot of the baseplane the ESP32 module is plugged.
     uint8_t UC2E_VOLTAGE = GPIO_NUM_34; // Analog input! Can measure the (12V) baseplane power via a voltage divider.

    // Sensor pins, Analog-to-digital converter enabled
     uint8_t SENSOR_1 = GPIO_NUM_35; // SENSOR_1, PP_17, PCIe B14
     uint8_t SENSOR_2 = GPIO_NUM_36; // SENSOR_2, PP_18, PCIe B15
     uint8_t SENSOR_3 = GPIO_NUM_39; // SENSOR_3, PP_19, PCIe B17

    // I2C pins, digital bus to control various functions of the baseplane and SUES modules
     uint8_t I2C_SDA = GPIO_NUM_21; // I2C_SDA, SDA, PCIe B6
     uint8_t I2C_SCL = GPIO_NUM_22; // I2C_SCL, SCL, PCIe B5

};

struct UC2_WEMOS : PinConfig
{
     String pindefName = "UC2_WEMOS";
    // ESP32-WEMOS D1 R32
     uint8_t MOTOR_A_DIR = GPIO_NUM_23; // Bridge from Endstop Z to Motor A (GPIO_NUM_23)
     uint8_t MOTOR_X_DIR = GPIO_NUM_16;
     uint8_t MOTOR_Y_DIR = GPIO_NUM_27;
     uint8_t MOTOR_Z_DIR = GPIO_NUM_14;
     uint8_t MOTOR_A_STEP = GPIO_NUM_5; // Bridge from Endstop Y to Motor A (GPIO_NUM_5)
     uint8_t MOTOR_X_STEP = GPIO_NUM_26;
     uint8_t MOTOR_Y_STEP = GPIO_NUM_25;
     uint8_t MOTOR_Z_STEP = GPIO_NUM_17;
     uint8_t MOTOR_ENABLE = GPIO_NUM_12;
     bool MOTOR_ENABLE_INVERTED = true;

     uint8_t LASER_1 = GPIO_NUM_18;
     uint8_t LASER_2 = GPIO_NUM_19;
     uint8_t LASER_3 = 0; // GPIO_NUM_21

     uint8_t LED_PIN= GPIO_NUM_4;
     uint8_t LED_COUNT = 64;

     uint8_t PIN_DEF_END_X = GPIO_NUM_13;
     uint8_t PIN_DEF_END_Y = 0; // GPIO_NUM_5;
     uint8_t PIN_DEF_END_Z = 0; // GPIO_NUM_23;

     String PSX_MAC = "1a:2b:3c:01:01:03";
     uint8_t PSX_CONTROLLER_TYPE = 2;
     boolean enableBlueTooth = true;

     uint8_t JOYSTICK_SPEED_MULTIPLIER = 5;
     uint8_t JOYSTICK_SPEED_MULTIPLIER_Z = 3;

     String mSSID = "Blynk";
	 String mPWD = "12345678";
     bool mAP = false;

};


struct UC2_OMNISCOPE : PinConfig
{
     String pindefName = "UC2_WEMOS";
    // ESP32-WEMOS D1 R32
     uint8_t MOTOR_A_DIR = GPIO_NUM_23; // Bridge from Endstop Z to Motor A (GPIO_NUM_23)
     uint8_t MOTOR_X_DIR = GPIO_NUM_16;
     uint8_t MOTOR_Y_DIR = GPIO_NUM_27;
     uint8_t MOTOR_Z_DIR = GPIO_NUM_14;
     uint8_t MOTOR_A_STEP = GPIO_NUM_5; // Bridge from Endstop Y to Motor A (GPIO_NUM_5)
     uint8_t MOTOR_X_STEP = GPIO_NUM_26;
     uint8_t MOTOR_Y_STEP = GPIO_NUM_25;
     uint8_t MOTOR_Z_STEP = GPIO_NUM_17;
     uint8_t MOTOR_ENABLE = GPIO_NUM_12;
     bool MOTOR_ENABLE_INVERTED = true;

     uint8_t LED_PIN= GPIO_NUM_4;
     uint8_t LED_COUNT = 128;

     uint8_t PIN_DEF_END_X = 0;
     uint8_t PIN_DEF_END_Y = 0; // GPIO_NUM_5;
     uint8_t PIN_DEF_END_Z = 0; // GPIO_NUM_23;

     String PSX_MAC = "1a:2b:3c:01:01:03";
     uint8_t PSX_CONTROLLER_TYPE = 2;
     boolean enableBlueTooth = false;

     String mSSID = "Blynk"; //"omniscope";
	 String mPWD = "12345678"; //"omniscope";
     bool mAP = false;

};


struct UC2_CassetteRecorder : PinConfig
{
     String pindefName = "CassetteRecorder";
    // ESP32-WEMOS D1 R32
     uint8_t ROTATOR_X_0 = GPIO_NUM_13; 
     uint8_t ROTATOR_X_1 = GPIO_NUM_14; 
     uint8_t ROTATOR_X_2 = GPIO_NUM_12; 
     uint8_t ROTATOR_X_3 = GPIO_NUM_27; 
     bool ROTATOR_ENABLE = true; 
};


struct UC2_XYZRotator : PinConfig
{
     String pindefName = "UC2_XYZRotator";
    // ESP32-WEMOS D1 R32
     uint8_t ROTATOR_X_0 = GPIO_NUM_13; 
     uint8_t ROTATOR_X_1 = GPIO_NUM_14; 
     uint8_t ROTATOR_X_2 = GPIO_NUM_12; 
     uint8_t ROTATOR_X_3 = GPIO_NUM_27; 
     uint8_t ROTATOR_Y_0 = GPIO_NUM_26; 
     uint8_t ROTATOR_Y_1 = GPIO_NUM_33; 
     uint8_t ROTATOR_Y_2 = GPIO_NUM_25; 
     uint8_t ROTATOR_Y_3 = GPIO_NUM_32;    
     uint8_t ROTATOR_Z_0 = GPIO_NUM_5; 
     uint8_t ROTATOR_Z_1 = GPIO_NUM_19; 
     uint8_t ROTATOR_Z_2 = GPIO_NUM_18; 
     uint8_t ROTATOR_Z_3 = GPIO_NUM_21;         
     bool ROTATOR_ENABLE = true; 

     String PSX_MAC = "1a:2b:3c:01:01:01";
     uint8_t PSX_CONTROLLER_TYPE = 1;
     bool enableBlueTooth = false;

};

struct UC2_3 : PinConfig
{
    /* 
    This is the newest electronics where direction/enable are on a seperate port extender
    */
    /*
     String pindefName = "UC2_3";
    
    Y_Cal-Data 34
    X_Cal-Data 32
    Z_Cal-Data 36
    I2C_SCL 22
    I2C_SDA 21
    
    SPI_MOSI 23
    SPI_MISO 19
    SPI_SCK 18
    SPI_CS 5
    X_Cal-Clk 33
    Y_Cal-Clk 35
    Z_CAL-CLK 17
    IOexp_ uint8_t 27
    
    A_STEP 13
    X_STEP 16
    Y_STEP 14
    Z_STEP 0
    LED_1 13
    
    PWM_1 12
    PWM_2 4
    PWM_3 2
    
    In_1 - 39
    */
    // UC2 STandalone V3
    // https://github.com/openUC2/UC2-SUES
    // 05998e057ac97c1e101c9ccc1f17070f89dd3f7c

    /*
    IOExpander
    We are using a port extender on I2c to control
    STEP_ENABLE P0
    X_DIR P1
    Y_DIR P2
    Z_DIR P3
    A_DIR P4
    X_LIMIT P5
    Y_LIMIT P6
    Z_LIMIT P7
     uint8_t MOTOR_ENABLE = -1;
    i2c addr should be 0x27
    http://www.ti.com/lit/ds/symlink/tca9535.pdf
    C561273
    */

     uint8_t MOTOR_A_DIR = 0;
     uint8_t MOTOR_X_DIR = 0;
     uint8_t MOTOR_Y_DIR = 0;
     uint8_t MOTOR_Z_DIR = 0;

     uint8_t MOTOR_A_STEP = GPIO_NUM_13;
     uint8_t MOTOR_X_STEP = GPIO_NUM_16;
     uint8_t MOTOR_Y_STEP = GPIO_NUM_14;
     uint8_t MOTOR_Z_STEP = GPIO_NUM_0;

     uint8_t MOTOR_ENABLE = 0;
     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = true;

     uint8_t LASER_1 = GPIO_NUM_12;
     uint8_t LASER_2 = GPIO_NUM_4;
     uint8_t LASER_3 = GPIO_NUM_2;

     uint8_t LED_PIN= GPIO_NUM_13;
     uint8_t LED_COUNT = 64;



    // FIXME: Is this redudant?! 
     uint8_t PIN_DEF_END_X = 0;
     uint8_t PIN_DEF_END_Y = 0;
     uint8_t PIN_DEF_END_Z = 0;
     uint8_t DIGITAL_IN_1 = PIN_DEF_END_X;
     uint8_t DIGITAL_IN_2 = PIN_DEF_END_Y;
     uint8_t DIGITAL_IN_3 = PIN_DEF_END_Z;

     String PSX_MAC = "1a:2b:3c:01:01:04";
     uint8_t PSX_CONTROLLER_TYPE = 2; // 1: PS3, 2: PS4
     bool enableBlueTooth = true;

     uint8_t JOYSTICK_SPEED_MULTIPLIER = 30;
     uint8_t JOYSTICK_MAX_ILLU = 100;
     uint8_t JOYSTICK_SPEED_MULTIPLIER_Z  = 30;

    // for caliper
     uint8_t X_CAL_DATA = GPIO_NUM_32;
     uint8_t Y_CAL_DATA = GPIO_NUM_34;
     uint8_t Z_CAL_DATA = GPIO_NUM_36;

    // I2c
     uint8_t I2C_SCL = GPIO_NUM_22;
     uint8_t I2C_SDA = GPIO_NUM_21;
     uint8_t I2C_ADD = 0x27;

    // SPI
     uint8_t SPI_MOSI = GPIO_NUM_23;
     uint8_t SPI_MISO = GPIO_NUM_19;
     uint8_t SPI_SCK = GPIO_NUM_18;
     uint8_t SPI_CS = GPIO_NUM_5;


};

const UC2_3 pinConfig; //UC2_1 pinConfig; //_WEMOS pinConfig; //_2 pinConfig; //_2 pinConfig; OMNISCOPE;
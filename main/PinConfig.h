#pragma once
#include "esp_err.h"
#include "Arduino.h"
// default Pin structure

const int8_t disabled = -1;

struct PinConfig
{
     const bool dumpHeap = false;
     const uint16_t MAIN_TASK_STACKSIZE = 4096; //8128;
     const uint16_t ANALOGJOYSTICK_TASK_STACKSIZE = 2024;
     const uint16_t HIDCONTROLLER_EVENT_STACK_SIZE = 4096;//8096;
     const uint16_t HTTP_MAX_URI_HANDLERS = 25;
     const uint16_t INTERRUPT_TASK_STACKSIZE = 4096; //8096;
     const uint16_t TEMPERATURE_TASK_STACKSIZE = 1024; //8096;
     const uint16_t STAGESCAN_TASK_STACKSIZE = 1024; //8096;
     const char *pindefName = "pindef";

     // by default Focusmotor use fastaccelstepper. if false Accelstepper lib get used
     bool useFastAccelStepper = true;
     // see AccelStepper.h MotorInterfaceType
     /*
     typedef enum
     {
          FUNCTION  = 0, ///< Use the functional interface, implementing your own driver functions (internal use only)
          DRIVER    = 1, ///< Stepper Driver, 2 driver pins required
          FULL2WIRE = 2, ///< 2 wire stepper, 2 motor pins required
          FULL3WIRE = 3, ///< 3 wire stepper, such as HDD spindle, 3 motor pins required
          FULL4WIRE = 4, ///< 4 wire full stepper, 4 motor pins required
          HALF3WIRE = 6, ///< 3 wire half stepper, such as HDD spindle, 3 motor pins required
          HALF4WIRE = 8  ///< 4 wire half stepper, 4 motor pins required
     } MotorInterfaceType;
     */
     int8_t AccelStepperMotorType = 8;
     // motor x direction pin
     int8_t MOTOR_X_DIR = disabled;
     // motor x step pin
     int8_t MOTOR_X_STEP = disabled;
     // motor y direction pin
     int8_t MOTOR_Y_DIR = disabled;
     // motor y step pin
     int8_t MOTOR_Y_STEP = disabled;
     // motor z direction pin
     int8_t MOTOR_Z_DIR = disabled;
     // motor z step pin
     int8_t MOTOR_Z_STEP = disabled;
     // motor a direction pin
     int8_t MOTOR_A_DIR = disabled;
     // motor a step pin
     int8_t MOTOR_A_STEP = disabled;
     // motor enable power
     int8_t MOTOR_ENABLE = disabled;
     // motor power pin is inverted
     bool MOTOR_ENABLE_INVERTED = false;
     // keep motors on after idle time?
     bool MOTOR_AUTOENABLE = false;

     gpio_num_t  I2C_INT = GPIO_NUM_NC;
     int8_t MOTOR_X_0 = disabled;
     int8_t MOTOR_X_1 = disabled;
     int8_t MOTOR_Y_0 = disabled;
     int8_t MOTOR_Y_1 = disabled;
     int8_t MOTOR_Z_0 = disabled;
     int8_t MOTOR_Z_1 = disabled;
     int8_t MOTOR_A_0 = disabled;
     int8_t MOTOR_A_1 = disabled;

     // LED_PINcontrol pin
     int8_t LED_PIN = disabled;
     // LED_PINcount from the strip
     int8_t LED_COUNT = disabled;

     // anlog joystick x pin
     int8_t ANLOG_JOYSTICK_X = disabled;
     // analog joystick y pin
     int8_t ANLOG_JOYSTICK_Y = disabled;

     int8_t LASER_0 = disabled;
     int8_t LASER_1 = disabled;
     int8_t LASER_2 = disabled;
     int8_t LASER_3 = disabled; // GPIO_NUM_21

     int pwm_resolution = 10; //8bit 256, 10bit  1024, 12bit 4096;
     int pwm_frequency =  19000; // 5000; //12000

     int8_t PIN_DEF_END_X = disabled;
     int8_t PIN_DEF_END_Y = disabled;
     int8_t PIN_DEF_END_Z = disabled;

     int8_t DIGITAL_OUT_1 = disabled;
     int8_t DIGITAL_OUT_2 = disabled;
     int8_t DIGITAL_OUT_3 = disabled;

     int8_t DIGITAL_IN_1 = disabled;
     int8_t DIGITAL_IN_2 = disabled;
     int8_t DIGITAL_IN_3 = disabled;

     // GALVos are always connected to 25/26
     int8_t dac_fake_1 = disabled; // RESET-ABORT just toggles between 1 and 0
     int8_t dac_fake_2 = disabled; // Coolant

     // analogout out (e.g. Lenses)
     int8_t analogout_PIN_1 = disabled;
     int8_t analogout_PIN_2 = disabled;
     int8_t analogout_PIN_3 = disabled;

     int8_t analogin_PIN_0 = disabled;
     int8_t analogin_PIN_1 = disabled;
     int8_t analogin_PIN_2 = disabled;
     int8_t analogin_PIN_3 = disabled;

     int8_t pid1 = disabled;
     int8_t pid2 = disabled;
     int8_t pid3 = disabled;

     int8_t X_ENC_PWM = disabled;
     int8_t Y_ENC_PWM = disabled;
     int8_t Z_ENC_PWM = disabled;
     int8_t X_ENC_IND = disabled;
     int8_t Y_ENC_IND = disabled;
     int8_t Z_ENC_IND = disabled;

     const char *PSX_MAC = "";
     int8_t PSX_CONTROLLER_TYPE = 0; // 1 = ps3, 2 =ps4

     bool enableScanner = false;
     bool enableBlueTooth = true;
     bool useBtHID = false; // enabling this disable psxcontroller
     bool enableWifi = true;

     int8_t JOYSTICK_SPEED_MULTIPLIER = 10;
     int8_t JOYSTICK_MAX_ILLU = 100;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 10;

     // WIFI
     const char *mSSID = "Uc2";
     const char *mPWD = "";
     bool mAP = true;
     const char *mSSIDAP = "UC2";
     const char *hostname = "youseetoo";

     // for caliper
     int8_t X_CAL_DATA = disabled;
     int8_t Y_CAL_DATA = disabled;
     int8_t Z_CAL_DATA = disabled;
     int8_t X_CAL_CLK = disabled;
     int8_t Y_CAL_CLK = disabled;
     int8_t Z_CAL_CLK = disabled;

     // for AS5311
     int8_t ENC_X_A = disabled;
     int8_t ENC_Y_A = disabled;
     int8_t ENC_Z_A = disabled;
     int8_t ENC_X_B = disabled;
     int8_t ENC_Y_B = disabled;
     int8_t ENC_Z_B = disabled;

     // I2c
     int8_t I2C_SCL = disabled;
     int8_t I2C_SDA = disabled;
     int8_t I2C_ADD = disabled;

     // SPI
     int8_t SPI_MOSI = disabled;
     int8_t SPI_MISO = disabled;
     int8_t SPI_SCK = disabled;
     int8_t SPI_CS = disabled;

     // Temperature Sensor
     int8_t DS28b20_PIN = disabled;

     // dual motor for Z?
     bool isDualAxisZ = false;
};

struct XYZ_MOTOR_JOYSTICK : PinConfig
{
     const char *pindefName = "XYZ_MOTOR_JOYSTICK";
     int8_t MOTOR_X_DIR = 16;
     int8_t MOTOR_X_STEP = 26;
     int8_t MOTOR_Y_DIR = 27;
     int8_t MOTOR_Y_STEP = 25;
     int8_t MOTOR_Z_DIR = 14;
     int8_t MOTOR_Z_STEP = 17;
     int8_t MOTOR_ENABLE = 12;
     bool MOTOR_ENABLE_INVERTED = true;

     int8_t ANLOG_JOYSTICK_X = 0;
     int8_t ANLOG_JOYSTICK_Y = 0;

     bool useBtHID = true;
};

struct XYZ_MOTOR_ENDSTOP_JOYSTICK : XYZ_MOTOR_JOYSTICK
{
     const char *pindefName = "XYZ_MOTOR_ENDSTOP_JOYSTICK";
     int8_t PIN_DEF_END_X = 12;
     int8_t PIN_DEF_END_Y = 13;
     int8_t PIN_DEF_END_Z = 5;
};

struct X_MOTOR_64LED_PIN : PinConfig
{
     const char *pindefName = "X_Motor_64LED";
     int8_t MOTOR_X_DIR = 21;
     int8_t MOTOR_X_STEP = 19;
     int8_t MOTOR_ENABLE = 18;
     bool MOTOR_ENABLE_INVERTED = true;

     int8_t LED_PIN = 27;
     int8_t LED_COUNT = 64;
};

struct HoLiSheet : PinConfig
{
     const char *pindefName = "HoLiSheet";
     int8_t MOTOR_Z_DIR = GPIO_NUM_14;
     int8_t MOTOR_Z_STEP = GPIO_NUM_17;
     int8_t MOTOR_ENABLE = GPIO_NUM_12;
     bool MOTOR_ENABLE_INVERTED = true;

     int8_t LASER_1 = GPIO_NUM_18;
     int8_t LASER_2 = GPIO_NUM_19;
     int8_t LASER_3 = disabled; // GPIO_NUM_21

     int8_t LED_PIN = GPIO_NUM_4;
     int8_t LED_COUNT = 64;

     int8_t PIN_DEF_END_X = GPIO_NUM_13;
     int8_t PIN_DEF_END_Y = GPIO_NUM_5;
     int8_t PIN_DEF_END_Z = GPIO_NUM_23;

     const char *PSX_MAC = "1a:2b:3c:01:01:01";
     int8_t PSX_CONTROLLER_TYPE = 2;
};

struct UC2_1 : PinConfig
{
     const char *pindefName = "UC2_1";
     // UC2 STandalone V1
     int8_t MOTOR_A_DIR = GPIO_NUM_21;
     int8_t MOTOR_X_DIR = GPIO_NUM_33;
     int8_t MOTOR_Y_DIR = GPIO_NUM_16;
     int8_t MOTOR_Z_DIR = GPIO_NUM_14;
     int8_t MOTOR_A_STEP = GPIO_NUM_22;
     int8_t MOTOR_X_STEP = GPIO_NUM_2;
     int8_t MOTOR_Y_STEP = GPIO_NUM_27;
     int8_t MOTOR_Z_STEP = GPIO_NUM_12;
     int8_t MOTOR_ENABLE = GPIO_NUM_13;
     bool MOTOR_ENABLE_INVERTED = true;

     int8_t LASER_1 = GPIO_NUM_4;
     int8_t LASER_2 = GPIO_NUM_15;
     int8_t LASER_3 = disabled; // GPIO_NUM_21

     int8_t LED_PIN = GPIO_NUM_17;
     int8_t LED_COUNT = 25;

     int8_t PIN_DEF_END_X = GPIO_NUM_10;
     int8_t PIN_DEF_END_Y = GPIO_NUM_11;
     int8_t PIN_DEF_END_Z = disabled;

     const char *PSX_MAC = "1a:2b:3c:01:01:05";
     int8_t PSX_CONTROLLER_TYPE = 1;

     int8_t JOYSTICK_SPEED_MULTIPLIER = 20;
     boolean enableBlueTooth = true;
};

struct UC2_Insert : PinConfig
{
     const char *pindefName = "UC2UC2_Insert";

     // UC2 STandalone V2
     int8_t MOTOR_A_DIR = disabled;
     int8_t MOTOR_X_DIR = GPIO_NUM_19;
     int8_t MOTOR_Y_DIR = disabled;
     int8_t MOTOR_Z_DIR = disabled;
     int8_t MOTOR_A_STEP = disabled;
     int8_t MOTOR_X_STEP = GPIO_NUM_35;
     int8_t MOTOR_Y_STEP = disabled;
     int8_t MOTOR_Z_STEP = disabled;
     int8_t MOTOR_ENABLE = GPIO_NUM_33;
     bool MOTOR_ENABLE_INVERTED = true;

     int8_t LASER_1 = disabled;
     int8_t LASER_2 = disabled;
     int8_t LASER_3 = disabled;

     int8_t LED_PIN = GPIO_NUM_4;
     int8_t LED_COUNT = 64;

     int8_t PIN_DEF_END_X = disabled;
     int8_t PIN_DEF_END_Y = disabled;
     int8_t PIN_DEF_END_Z = disabled;

     const char *PSX_MAC = "1a:2b:3c:01:01:01";
     int8_t PSX_CONTROLLER_TYPE = 2;
     boolean enableBlueTooth = true;
};

struct UC2_2 : PinConfig
{
     const char *pindefName = "UC2_2";

     // UC2 STandalone V2
     int8_t MOTOR_A_DIR = GPIO_NUM_21;
     int8_t MOTOR_X_DIR = GPIO_NUM_33;
     int8_t MOTOR_Y_DIR = GPIO_NUM_16;
     int8_t MOTOR_Z_DIR = GPIO_NUM_14;
     int8_t MOTOR_A_STEP = GPIO_NUM_22;
     int8_t MOTOR_X_STEP = GPIO_NUM_2;
     int8_t MOTOR_Y_STEP = GPIO_NUM_27;
     int8_t MOTOR_Z_STEP = GPIO_NUM_12;
     int8_t MOTOR_ENABLE = GPIO_NUM_13;
     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = true;

     int8_t LASER_1 = GPIO_NUM_17;
     int8_t LASER_2 = GPIO_NUM_4;
     int8_t LASER_3 = GPIO_NUM_15;

     int8_t LED_PIN = GPIO_NUM_32;
     int8_t LED_COUNT = 64;

     // FIXME: Is this redudant?!
     int8_t PIN_DEF_END_X = GPIO_NUM_34;
     int8_t PIN_DEF_END_Y = GPIO_NUM_39;
     int8_t PIN_DEF_END_Z = disabled;
     int8_t DIGITAL_IN_1 = PIN_DEF_END_X;
     int8_t DIGITAL_IN_2 = PIN_DEF_END_Y;
     int8_t DIGITAL_IN_3 = PIN_DEF_END_Z;

     const char *PSX_MAC = "1a:2b:3c:01:01:04";
     int8_t PSX_CONTROLLER_TYPE = 2; // 1: PS3, 2: PS4
     bool enableBlueTooth = true;

     int8_t JOYSTICK_SPEED_MULTIPLIER = 30;
     int8_t JOYSTICK_MAX_ILLU = 100;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 30;

     int8_t DAC_1 = disabled;
     int8_t DAC_2 = disabled;
     int8_t ds28b20_PIN = disabled; 
};

struct UC2e : PinConfig
{
     const char *pindefName = "UC2e";

     // UC2e (UC2-express-bus) pinout of the SUES system (modules and baseplane) dictated by the spec for the ESP32 module pinout
     // that document maps ESP32 pins to UC2e pins (and pin locations on the physical PCIe x1 connector
     // and assigns functions compatible with the ESP32 GPIO's hardware capabilities.
     // See https://github.com/openUC2/UC2-SUES/blob/main/uc2-express-bus.md#esp32-module-pinout

     // Stepper Motor control pins
     int8_t MOTOR_X_DIR = GPIO_NUM_12;  // STEP-1_DIR, PP_05, PCIe A8
     int8_t MOTOR_Y_DIR = GPIO_NUM_13;  // STEP-2_DIR, PP_06, PCIe A9
     int8_t MOTOR_Z_DIR = GPIO_NUM_14;  // STEP-3_DIR, PP_07, PCIe A10
     int8_t MOTOR_A_DIR = GPIO_NUM_15;  // STEP-4_DIR, PP_08, PCIe A11
     int8_t MOTOR_X_STEP = GPIO_NUM_0;  // STEP-1_STP, PP_01, PCIe A1
     int8_t MOTOR_Y_STEP = GPIO_NUM_2;  // STEP-2_STP, PP_02, PCIe A5
     int8_t MOTOR_Z_STEP = GPIO_NUM_4;  // STEP-3_STP, PP_03, PCIe A6
     int8_t MOTOR_A_STEP = GPIO_NUM_5;  // STEP-4_STP, PP_04, PCIe A7
     int8_t MOTOR_ENABLE = GPIO_NUM_17; // STEP_EN, PP_09, PCIe A13. Enables/disables all stepper drivers' output.
     bool MOTOR_ENABLE_INVERTED = true; // When set to a logic high, the outputs are disabled.

     // LED pins (addressable - WS2812 and equivalent)
     int8_t NEOPIXEL_PIN = GPIO_NUM_18; // NEOPIXEL, PP_10, PCIe A14. I changed the name from LED_PIN to indicate that it's for an adressable LED chain
     int8_t LED_COUNT = 64;             // I havent touched this

     // Main power switch for all the AUX modules of the baseplane
     int8_t BASEPLANE_ENABLE = GPIO_NUM_16; // UC2e_EN, ENABLE, PCIe A17. ESP32 module in Controller slot of baseplane switches power off all AUX modules. HIGH activates AUX modules.

     // Pulse Width Modulation pins
     int8_t PWM_1 = GPIO_NUM_19; // PWM_1, PP_11, PCIe A16
     int8_t PWM_2 = GPIO_NUM_23; // PWM_2, PP_12, PCIe B8
     int8_t PWM_3 = GPIO_NUM_27; // PWM_3, PP_15, PCIe B11
     int8_t PWM_4 = GPIO_NUM_32; // PWM_4, PP_16, PCIe B12

     // Digital-to-Analog converter pins
     int8_t DAC_1 = GPIO_NUM_25; // DAC_1, PP_13, PCIe B9
     int8_t DAC_2 = GPIO_NUM_27; // DAC_2, PP_14, PCIe B10

     // Other pins that sense PCIe/housekeeping things
     int8_t UC2E_OWN_ADDR = GPIO_NUM_33; // Analog input! The voltage indicates into which slot of the baseplane the ESP32 module is plugged.
     int8_t UC2E_VOLTAGE = GPIO_NUM_34;  // Analog input! Can measure the (12V) baseplane power via a voltage divider.

     // Sensor pins, Analog-to-digital converter enabled
     int8_t SENSOR_1 = GPIO_NUM_35; // SENSOR_1, PP_17, PCIe B14
     int8_t SENSOR_2 = GPIO_NUM_36; // SENSOR_2, PP_18, PCIe B15
     int8_t SENSOR_3 = GPIO_NUM_39; // SENSOR_3, PP_19, PCIe B17

     // I2C pins, digital bus to control various functions of the baseplane and SUES modules
     int8_t I2C_SDA = GPIO_NUM_21; // I2C_SDA, SDA, PCIe B6
     int8_t I2C_SCL = GPIO_NUM_22; // I2C_SCL, SCL, PCIe B5
};

struct UC2_WEMOS : PinConfig // also used for cellSTORM wellplateformat
{
     const char *pindefName = "UC2_WEMOS";
     const char *mSSIDAP = "UC2_WEMOS";
     const char *mSSID = "UC2_WEMOS";
     const char *mPWD = "12345678";

     int pwm_resolution = 12; //8bit 256, 10bit  1024, 12bit 4096;
     int pwm_frequency =  19000; // 5000; //12000
     
     bool mAP = true;
     const char *hostname = "youseetoo";

     // ESP32-WEMOS D1 R32
     int8_t MOTOR_A_DIR = GPIO_NUM_23; // Bridge from Endstop Z to Motor A (GPIO_NUM_23)
     int8_t MOTOR_X_DIR = GPIO_NUM_16;
     int8_t MOTOR_Y_DIR = GPIO_NUM_27;
     int8_t MOTOR_Z_DIR = GPIO_NUM_14;
     int8_t MOTOR_A_STEP = GPIO_NUM_5; // Bridge from Endstop Y to Motor A (GPIO_NUM_5)
     int8_t MOTOR_X_STEP = GPIO_NUM_26;
     int8_t MOTOR_Y_STEP = GPIO_NUM_25;
     int8_t MOTOR_Z_STEP = GPIO_NUM_17;
     int8_t MOTOR_ENABLE = GPIO_NUM_12;
     bool MOTOR_ENABLE_INVERTED = true;

     int8_t LASER_1 = GPIO_NUM_18; // WEMOS_D1_R32_SPINDLE_ENABLE_PIN
     int8_t LASER_2 = GPIO_NUM_19; // WEMOS_D1_R32_SPINDLEPWMPIN
     int8_t LASER_3 = GPIO_NUM_13; // WEMOS_D1_R32_X_LIMIT_PIN

     int8_t LED_PIN = GPIO_NUM_4;
     int8_t LED_COUNT = 64;

     int8_t PIN_DEF_END_X = 0;
     int8_t PIN_DEF_END_Y = disabled; // GPIO_NUM_5;
     int8_t PIN_DEF_END_Z = disabled; // GPIO_NUM_23;

     const char *PSX_MAC = "1a:2b:3c:01:01:03";
     int8_t PSX_CONTROLLER_TYPE = 2;
     boolean enableBlueTooth = true;

     int8_t JOYSTICK_SPEED_MULTIPLIER = 5;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 3;


};

struct UC2_OMNISCOPE : PinConfig
{
     const char *pindefName = "UC2_WEMOS";
     // ESP32-WEMOS D1 R32
     int8_t MOTOR_A_DIR = GPIO_NUM_23; // Bridge from Endstop Z to Motor A (GPIO_NUM_23)
     int8_t MOTOR_X_DIR = GPIO_NUM_16;
     int8_t MOTOR_Y_DIR = GPIO_NUM_27;
     int8_t MOTOR_Z_DIR = GPIO_NUM_14;
     int8_t MOTOR_A_STEP = GPIO_NUM_5; // Bridge from Endstop Y to Motor A (GPIO_NUM_5)
     int8_t MOTOR_X_STEP = GPIO_NUM_26;
     int8_t MOTOR_Y_STEP = GPIO_NUM_25;
     int8_t MOTOR_Z_STEP = GPIO_NUM_17;
     int8_t MOTOR_ENABLE = GPIO_NUM_12;
     bool MOTOR_ENABLE_INVERTED = true;

     int8_t LED_PIN = GPIO_NUM_4;
     int8_t LED_COUNT = 128;

     int8_t PIN_DEF_END_X = disabled;
     int8_t PIN_DEF_END_Y = disabled; // GPIO_NUM_5;
     int8_t PIN_DEF_END_Z = disabled; // GPIO_NUM_23;

     const char *PSX_MAC = "1a:2b:3c:01:01:03";
     int8_t PSX_CONTROLLER_TYPE = 2;
     boolean enableBlueTooth = false;

     const char *mSSID = "Blynk";   //"omniscope";
     const char *mPWD = "12345678"; //"omniscope";
     bool mAP = false;
};

struct UC2_CassetteRecorder : PinConfig
{
     const char *pindefName = "CassetteRecorder";
     bool useFastAccelStepper = false;
     int8_t AccelStepperMotorType = 8;
     // ESP32-WEMOS D1 R32
     int8_t MOTOR_X_STEP = GPIO_NUM_13;
     int8_t MOTOR_X_DIR = GPIO_NUM_14;
     int8_t MOTOR_X_0 = GPIO_NUM_12;
     int8_t MOTOR_X_1 = GPIO_NUM_27;
     int MOTOR_ENABLE = GPIO_NUM_27;
};



struct UC2_XYZRotator : PinConfig
{
     const char *pindefName = "UC2_XYZRotator";

     bool useFastAccelStepper = false;
     int8_t AccelStepperMotorType = 8;
     // ESP32-WEMOS D1 R32
     int8_t MOTOR_X_STEP = GPIO_NUM_13;
     int8_t MOTOR_X_DIR = GPIO_NUM_14;
     int8_t MOTOR_X_0 = GPIO_NUM_12;
     int8_t MOTOR_X_1 = GPIO_NUM_27;
     int8_t MOTOR_Y_STEP = GPIO_NUM_26;
     int8_t MOTOR_Y_DIR = GPIO_NUM_33;
     int8_t MOTOR_Y_0 = GPIO_NUM_25;
     int8_t MOTOR_Y_1 = GPIO_NUM_32;
     int8_t MOTOR_Z_STEP = GPIO_NUM_5;
     int8_t MOTOR_Z_DIR = GPIO_NUM_19;
     int8_t MOTOR_Z_0 = GPIO_NUM_18;
     int8_t MOTOR_Z_1 = GPIO_NUM_21;

     const char *PSX_MAC = "1a:2b:3c:01:01:01";
     int8_t PSX_CONTROLLER_TYPE = 1;
     bool enableBlueTooth = false;
};

struct UC2_3 : PinConfig
{
     /*
     This is the newest electronics where direction/enable are on a seperate port extender
     */
     /*


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
     IOexp_ int8_t 27

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
      int8_t MOTOR_ENABLE = -1;
     i2c addr should be 0x27
     http://www.ti.com/lit/ds/symlink/tca9535.pdf
     C561273
     */

     const char * pindefName = "UC2_3";
     const char *mSSID = "UC2_3_new";
     bool mAP = true;
     const char *mPWD = "12345678";

     int8_t MOTOR_A_STEP = GPIO_NUM_15;
     int8_t MOTOR_X_STEP = GPIO_NUM_16;
     int8_t MOTOR_Y_STEP = GPIO_NUM_14;
     int8_t MOTOR_Z_STEP = GPIO_NUM_0;

     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = true;
     bool useFastAccelStepper = true;
     int8_t AccelStepperMotorType = 1;

     int8_t LASER_1 = GPIO_NUM_12;
     int8_t LASER_2 = GPIO_NUM_4;
     int8_t LASER_3 = GPIO_NUM_2;

     int8_t LED_PIN = GPIO_NUM_13;
     int8_t LED_COUNT = 64;

     // FIXME: Is this redudant?!
     int8_t PIN_DEF_END_X = 105; // I2C TCA
     int8_t PIN_DEF_END_Y = 106;
     int8_t PIN_DEF_END_Z = 107;
     int8_t DIGITAL_IN_1 = PIN_DEF_END_X;
     int8_t DIGITAL_IN_2 = PIN_DEF_END_Y;
     int8_t DIGITAL_IN_3 = PIN_DEF_END_Z;

     // const char * PSX_MAC = "1a:2b:3c:01:01:04";
     // int8_t PSX_CONTROLLER_TYPE = 2; // 1: PS3, 2: PS4
     bool enableBlueTooth = false;
     bool useBtHID = false;

     int8_t JOYSTICK_SPEED_MULTIPLIER = 30;
     int8_t JOYSTICK_MAX_ILLU = 255;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 10;

     bool dumpHeap = true;

     // for AS5311
     int8_t ENC_X_A = GPIO_NUM_32;
     int8_t ENC_Y_A = GPIO_NUM_34;
     int8_t ENC_Z_A = GPIO_NUM_36;
     int8_t ENC_X_B = GPIO_NUM_33;
     int8_t ENC_Y_B = GPIO_NUM_35;
     int8_t ENC_Z_B = GPIO_NUM_17;

     // Serial 2    
     int8_t SERIAL2_TX = GPIO_NUM_26;
     int8_t SERIAL2_RX = GPIO_NUM_25;
     
     // I2c
     int8_t I2C_SCL = GPIO_NUM_22;
     int8_t I2C_SDA = GPIO_NUM_21;
     int8_t I2C_ADD = 0x27;
     gpio_num_t I2C_INT = GPIO_NUM_27;

     // SPI
     int8_t SPI_MOSI = GPIO_NUM_23;
     int8_t SPI_MISO = GPIO_NUM_19;
     int8_t SPI_SCK = GPIO_NUM_18;
     int8_t SPI_CS = GPIO_NUM_5;

     // Temperature
     int8_t DS28b20_PIN = disabled; //GPIO_NUM_25;
     int8_t LASER_0 = disabled; //GPIO_NUM_26;

     // dual axis Z
     bool isDualAxisZ = true;


};


struct UC2_linencoder : PinConfig
{
     /*
     This is the newest electronics where direction/enable are on a seperate port extender
     */
     /*


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
     IOexp_ int8_t 27

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
      int8_t MOTOR_ENABLE = -1;
     i2c addr should be 0x27
     http://www.ti.com/lit/ds/symlink/tca9535.pdf
     C561273
     */

     const char * pindefName = "UC2_3";
     const char *mSSID = "UC2_3";
     bool mAP = true;
     const char *mPWD = "12345678";

     int8_t MOTOR_A_STEP = GPIO_NUM_15;
     int8_t MOTOR_X_STEP = GPIO_NUM_14;
     int8_t MOTOR_Y_STEP = disabled;
     int8_t MOTOR_Z_STEP = GPIO_NUM_0;
     int8_t MOTOR_ENABLE = GPIO_NUM_13;
     int8_t MOTOR_A_DIR = GPIO_NUM_21;
     int8_t MOTOR_X_DIR = GPIO_NUM_16;
     int8_t MOTOR_Y_DIR = disabled;
     int8_t MOTOR_Z_DIR = disabled;

     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = true;
     bool useFastAccelStepper = true;
     int8_t AccelStepperMotorType = 1;

     int8_t LASER_1 = GPIO_NUM_12;
     int8_t LASER_2 = GPIO_NUM_4;
     int8_t LASER_3 = GPIO_NUM_2;

     int8_t LED_PIN = GPIO_NUM_13;
     int8_t LED_COUNT = 64;

     // FIXME: Is this redudant?!
     int8_t PIN_DEF_END_X = 105; // I2C TCA
     int8_t PIN_DEF_END_Y = 106;
     int8_t PIN_DEF_END_Z = 107;
     int8_t DIGITAL_IN_1 = PIN_DEF_END_X;
     int8_t DIGITAL_IN_2 = PIN_DEF_END_Y;
     int8_t DIGITAL_IN_3 = PIN_DEF_END_Z;

     // const char * PSX_MAC = "1a:2b:3c:01:01:04";
     // int8_t PSX_CONTROLLER_TYPE = 2; // 1: PS3, 2: PS4
     bool enableBlueTooth = true;
     bool useBtHID = true;

     int8_t JOYSTICK_SPEED_MULTIPLIER = 30;
     int8_t JOYSTICK_MAX_ILLU = 100;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 1;

     // I2c
/*     int8_t I2C_SCL = GPIO_NUM_22;
     int8_t I2C_SDA = GPIO_NUM_21;
     int8_t I2C_ADD = 0x27;
     gpio_num_t I2C_INT = GPIO_NUM_27;*/

     // linear encoder
     int8_t X_ENC_PWM = GPIO_NUM_32;
     int8_t X_ENC_IND = GPIO_NUM_33;

};


struct UC2_LineFrameTrigger : PinConfig
{
     /*
     This is the newest electronics where direction/enable are on a seperate port extender
     */
     /*

     // based on UC2_3
     */

     const char * pindefName = "UC2_LineFrameTrigger";
     const char *mSSID = "UC2_LineFrameTrigger";
     bool mAP = true;
     const char *mPWD = "12345678";

     int8_t MOTOR_A_STEP = GPIO_NUM_15;
     int8_t MOTOR_X_STEP = GPIO_NUM_16;
     int8_t MOTOR_Y_STEP = GPIO_NUM_14;
     int8_t MOTOR_Z_STEP = GPIO_NUM_0;
     int8_t MOTOR_ENABLE = 105;
     int8_t MOTOR_A_DIR = 104;
     int8_t MOTOR_X_DIR = 101;
     int8_t MOTOR_Y_DIR = 102;
     int8_t MOTOR_Z_DIR = 103;

     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = true;
     int8_t AccelStepperMotorType = 1;

     int8_t LASER_1 = disabled;
     int8_t LASER_2 = disabled;
     int8_t LASER_3 = disabled;

     int8_t DIGITAL_OUT_1 = GPIO_NUM_12;
     int8_t DIGITAL_OUT_2 = GPIO_NUM_4;
     int8_t DIGITAL_OUT_3 = GPIO_NUM_2;
     
     int8_t LED_PIN = GPIO_NUM_13;
     int8_t LED_COUNT = 64;

     // FIXME: Is this redudant?!
     int8_t PIN_DEF_END_X = 105; // I2C TCA
     int8_t PIN_DEF_END_Y = 106;
     int8_t PIN_DEF_END_Z = 107;
     int8_t DIGITAL_IN_1 = PIN_DEF_END_X;
     int8_t DIGITAL_IN_2 = PIN_DEF_END_Y;
     int8_t DIGITAL_IN_3 = PIN_DEF_END_Z;

     // const char * PSX_MAC = "1a:2b:3c:01:01:04";
     // int8_t PSX_CONTROLLER_TYPE = 2; // 1: PS3, 2: PS4
     bool enableBlueTooth = true;
     bool useBtHID = true;

     int8_t JOYSTICK_SPEED_MULTIPLIER = 30;
     int8_t JOYSTICK_MAX_ILLU = 255;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 10;


     // for AS5311
     int8_t ENC_X_A = GPIO_NUM_32;
     int8_t ENC_Y_A = GPIO_NUM_34;
     int8_t ENC_Z_A = GPIO_NUM_36;
     int8_t ENC_X_B = GPIO_NUM_33;
     int8_t ENC_Y_B = GPIO_NUM_35;
     int8_t ENC_Z_B = GPIO_NUM_17;

     
     // I2c
     int8_t I2C_SCL = GPIO_NUM_22;
     int8_t I2C_SDA = GPIO_NUM_21;
     int8_t I2C_ADD = 0x27;
     gpio_num_t I2C_INT = GPIO_NUM_27;

     // SPI
     int8_t SPI_MOSI = GPIO_NUM_23;
     int8_t SPI_MISO = GPIO_NUM_19;
     int8_t SPI_SCK = GPIO_NUM_18;
     int8_t SPI_CS = GPIO_NUM_5;

     // Temperature
     int8_t DS28b20_PIN = GPIO_NUM_25;

     // dual axis Z
     bool isDualAxisZ = false;
     bool useFastAccelStepper = false;
};

     

const UC2_3 pinConfig; // UC2_3 UC2_LineFrameTrigger UC2_1 pinConfig; //_WEMOS pinConfig; //_2 pinConfig; //_2 pinConfig; OMNISCOPE;

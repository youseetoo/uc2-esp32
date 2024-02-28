#pragma once
#include "esp_err.h"
#include "Arduino.h"
// default Pin structure

/*
available modules. to enable a module define them inside your config

#define ANALOG_IN_CONTROLLER
#define ANALOG_JOYSTICK
#define ANALOG_OUT_CONTROLLER
#define BLUETOOTH
#define DAC_CONTROLLER
#define DIGITAL_IN_CONTROLLER
#define DIGITAL_OUT_CONTROLLER
#define ENCODER_CONTROLLER
#define HOME_MOTOR
#define LASER_CONTROLLER
#define LED_CONTROLLER
#define FOCUS_MOTOR
#define PID_CONTROLLER
#define SCANNER_CONTROLLER
#define WIFI
*/

/*
bt defines
#define BTHID
#define PSXCONTROLLER
*/

/*
motor defines
#define USE_TCA9535
*/


const int8_t disabled = -1;

struct PinConfig
{
     const bool dumpHeap = false;
     const uint16_t MAIN_TASK_STACKSIZE = 8128;
     const uint16_t ANALOGJOYSTICK_TASK_STACKSIZE = 2024;
     const uint16_t HIDCONTROLLER_EVENT_STACK_SIZE = 8096;
     const uint16_t HTTP_MAX_URI_HANDLERS = 35;
     const uint16_t BT_CONTROLLER_TASK_STACKSIZE = 4*1024;
     
     const char *pindefName = "pindef";
     
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

     int8_t LASER_1 = disabled;
     int8_t LASER_2 = disabled;
     int8_t LASER_3 = disabled; // GPIO_NUM_21

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

     const char *PSX_MAC = "";
     int8_t PSX_CONTROLLER_TYPE = 0; // 1 = ps3, 2 =ps4

     int8_t JOYSTICK_SPEED_MULTIPLIER = 10;
     int8_t JOYSTICK_MAX_ILLU = 100;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 10;

     // WIFI
     const char *mSSID = "Uc2";
     const char *mPWD = "12345678";
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

     // I2c
     int8_t I2C_SCL = disabled;
     int8_t I2C_SDA = disabled;
     int8_t I2C_ADD = disabled;

     // SPI
     int8_t SPI_MOSI = disabled;
     int8_t SPI_MISO = disabled;
     int8_t SPI_SCK = disabled;
     int8_t SPI_CS = disabled;
};


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
#define MOTOR_CONTROLLER
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


// will be used to differentiate between the different controllers for I2C 
enum I2CControllerType : uint8_t
{
     mDISABLED = 0,
     mENCODER = 1,
     mLINEAR_ENCODER = 2,
     mHOME = 3,
     mI2C = 4,
     mTCA = 5,
     mmANALOG = 6,
     mDIGITAL = 7,
     mBT = 8,
     mLASER = 9,
     mLED = 10,
     mPID = 11,
     mSCANNER = 12,
     mWIFI = 13,
     mHEAT = 14,
     mGALVO = 15,
     mMESSAGE = 16,
     mDAC = 17,
     mMOTOR = 18, 
     mDIAL = 19
};

struct PinConfig
{
     const bool dumpHeap = false;
     const uint16_t DEFAULT_TASK_PRIORITY = 0;
     const uint16_t MAIN_TASK_STACKSIZE = 8128;
     const uint16_t ANALOGJOYSTICK_TASK_STACKSIZE = 1024;
     const uint16_t HIDCONTROLLER_EVENT_STACK_SIZE = 2048; // Don't go below 2048
     const uint16_t HTTP_MAX_URI_HANDLERS = 35;
     const uint16_t BT_CONTROLLER_TASK_STACKSIZE = 4 * 1024; // TODO check if this is ending in stackoverflow 
     const uint16_t MOTOR_TASK_STACKSIZE = 4 * 1024;
     const uint16_t MOTOR_TASK_UPDATEWEBSOCKET_STACKSIZE = 6 * 1024;
     const uint16_t INTERRUPT_CONTROLLER_TASK_STACKSIZE = 6 * 1024;
     const uint16_t TCA_TASK_STACKSIZE = 2048;
     const uint16_t SCANNER_TASK_STACKSIZE = 10000;
     const uint16_t TEMPERATURE_TASK_STACKSIZE = 1024; //8096;
     const unsigned long BAUDRATE = 115200;

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

     int8_t HOME_X_DIR = -1;
     int8_t HOME_Y_DIR = -1;
     int8_t HOME_Z_DIR = -1;
     int HOME_X_SPEED = 3000;
     int HOME_Y_SPEED = 3000;
     int HOME_Z_SPEED = 3000;
     bool HOME_X_SIG_INVERTED = false;
     bool HOME_Y_SIG_INVERTED = false;
     bool HOME_Z_SIG_INVERTED = false;

     gpio_num_t I2C_INT = GPIO_NUM_NC;
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
     int8_t LASER_0 = disabled;

     int8_t DIGITAL_OUT_1 = disabled;
     int8_t DIGITAL_OUT_2 = disabled;
     int8_t DIGITAL_OUT_3 = disabled;

     int8_t DIGITAL_IN_1 = disabled;
     int8_t DIGITAL_IN_2 = disabled;
     int8_t DIGITAL_IN_3 = disabled;

     // GALVos are always connected to 25/26
     int8_t dac_fake_1 = disabled; //
     int8_t dac_fake_2 = disabled; //

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

     // for AS5311
     int8_t ENC_X_A = disabled;
     int8_t ENC_Y_A = disabled;
     int8_t ENC_Z_A = disabled;
     int8_t ENC_X_B = disabled;
     int8_t ENC_Y_B = disabled;
     int8_t ENC_Z_B = disabled;
     
     bool isDualAxisZ = false;

     // I2c
     int8_t I2C_SCL = disabled;      // This is the poart that connects to all other slaves
     int8_t I2C_SDA = disabled;

     // Auxilarry I2C devices
     int8_t I2C_ADD_TCA = disabled; // this is the port extender on the PCB that controls the direction of the motors
     I2CControllerType I2C_CONTROLLER_TYPE = I2CControllerType::mDISABLED;
     int8_t I2C_ADD_SLAVE = -1;    // I2C address of the ESP32 if it's a slave
     int8_t I2C_MOTOR_AXIS = 0;   // On the slave we have one motor axis per slave
     int8_t I2C_ADD_MOT_X = 0x40;
     int8_t I2C_ADD_MOT_Y = 0x41;
     int8_t I2C_ADD_MOT_Z = 0x42;
     int8_t I2C_ADD_MOT_A = 0x43;
     int8_t I2C_ADD_LEX_MAT = 0x50;
     int8_t I2C_ADD_LEX_PWM1 = 0x51;
     int8_t I2C_ADD_LEX_PWM2 = 0x52;
     int8_t I2C_ADD_LEX_PWM3 = 0x53;
     int8_t I2C_ADD_LEX_PWM4 = 0x54;

     // inputs
     int8_t I2C_ADD_M5_DIAL = 0x60;


     // SPI
     int8_t SPI_MOSI = disabled;
     int8_t SPI_MISO = disabled;
     int8_t SPI_SCK = disabled;
     int8_t SPI_CS = disabled;

      // Temperature Sensor
     int8_t DS28b20_PIN = disabled;
     
};

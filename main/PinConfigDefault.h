#pragma once
#include "esp_err.h"
#include "Arduino.h"

#define MOTOR_AXIS_COUNT 4

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

#define LED_BUILTIN 0 // for Xiao ESP32S3
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
     const uint16_t STAGESCAN_TASK_STACKSIZE = 4 * 2048;
     const uint16_t HIDCONTROLLER_EVENT_STACK_SIZE = 2 * 2048; // Don't go below 2048
     const uint16_t HTTP_MAX_URI_HANDLERS = 35;
     const uint16_t BT_CONTROLLER_TASK_STACKSIZE = 4 * 2048; // TODO check if this is ending in stackoverflow
     const uint16_t MOTOR_TASK_STACKSIZE = 4 * 1024;
     const uint16_t MOTOR_TASK_UPDATEWEBSOCKET_STACKSIZE = 6 * 1024;
     const uint16_t INTERRUPT_CONTROLLER_TASK_STACKSIZE = 6 * 1024;
     const uint16_t TCA_TASK_STACKSIZE = 2048;
     const uint16_t SCANNER_TASK_STACKSIZE = 10000;
     const uint16_t TEMPERATURE_TASK_STACKSIZE = 1024; // 8096;
     const unsigned long BAUDRATE = 115200;
     const uint16_t serialTimeout = 50;
     const char *pindefName = "pindef";

     bool DEBUG_CAN_ISO_TP = 0;
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

     // additional dummy pins
     int8_t MOTOR_B_DIR = disabled;
     int8_t MOTOR_B_STEP = disabled;
     int8_t MOTOR_C_DIR = disabled;
     int8_t MOTOR_C_STEP = disabled;
     int8_t MOTOR_D_DIR = disabled;
     int8_t MOTOR_D_STEP = disabled;
     int8_t MOTOR_E_DIR = disabled;
     int8_t MOTOR_E_STEP = disabled;
     int8_t MOTOR_F_DIR = disabled;
     int8_t MOTOR_F_STEP = disabled;
     int8_t MOTOR_G_DIR = disabled;
     int8_t MOTOR_G_STEP = disabled;

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

     const uint16_t RING_INNER_START = 0; // Inner ring: 20 LEDs (indices 0-19)
     const uint16_t RING_INNER_COUNT = 0;
     const uint16_t RING_MIDDLE_START = 0; // Middle ring: 28 LEDs (indices 20-47)
     const uint16_t RING_MIDDLE_COUNT = 0;
     const uint16_t RING_BIGGEST_START = 0; // Biggest ring: 40 LEDs (indices 48-87)
     const uint16_t RING_BIGGEST_COUNT = 0;
     const uint16_t RING_OUTEST_START = 0; // Outest ring: 48 LEDs (indices 88-135)
     const uint16_t RING_OUTEST_COUNT = 0;

     const uint8_t MATRIX_W = 8; // width
     const uint8_t MATRIX_H = 8; // height

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

     int8_t CAMERA_TRIGGER_PIN = disabled;
     bool CAMERA_TRIGGER_INVERTED = false;
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
     bool ENC_X_encoderDirection = true;
     bool ENC_Y_encoderDirection = true;
     bool ENC_Z_encoderDirection = true;
     bool isDualAxisZ = false;

     // TMC 2209 stuff
     int8_t tmc_SW_RX = disabled;    // GPIO_NUM_44; // D7 -> GPIO44
     int8_t tmc_SW_TX = disabled;    // GPIO_NUM_43; // D6 -> GPIO43
     int8_t tmc_pin_diag = disabled; // D3 -> GPIO4

     int tmc_microsteps = 16;
     int tmc_rms_current = 500;
     int tmc_stall_value = 100;
     int tmc_sgthrs = 100;
     int tmc_semin = 5;
     int tmc_semax = 2;
     int tmc_sedn = 0b01;
     int tmc_tcoolthrs = 0xFFFFF;
     int tmc_blank_time = 24;
     int tmc_toff = 4;

     // I2c
     bool isI2Cinitiated = false;
     int8_t I2C_SCL = disabled; // This is the poart that connects to all other slaves
     int8_t I2C_SDA = disabled;

     // Auxilarry I2C devices
     int8_t I2C_ADD_TCA = disabled; // this is the port extender on the PCB that controls the direction of the motors
     I2CControllerType I2C_CONTROLLER_TYPE = I2CControllerType::mDISABLED;
     uint8_t I2C_ADD_SLAVE = -1;       // I2C address of the ESP32 if it's a slave
     uint8_t REMOTE_MOTOR_AXIS_ID = 1; // On the slave we have one motor axis per slave
     uint8_t REMOTE_LASER_ID = 0;      // On the slave we have one laser axis per slave
     uint8_t I2C_ADD_MOT_X = 0x40;
     uint8_t I2C_ADD_MOT_Y = 0x41;
     uint8_t I2C_ADD_MOT_Z = 0x42;
     uint8_t I2C_ADD_MOT_A = 0x43;
     uint8_t I2C_ADD_LEX_MAT = 0x60;
     uint8_t I2C_ADD_LEX_PWM0 = 0x50;
     uint8_t I2C_ADD_LEX_PWM1 = 0x51;
     uint8_t I2C_ADD_LEX_PWM2 = 0x52;
     uint8_t I2C_ADD_LEX_PWM3 = 0x53;

     // inputs
     uint8_t I2C_ADD_M5_DIAL = 0x60;

     // SPI
     int8_t SPI_MOSI = disabled;
     int8_t SPI_MISO = disabled;
     int8_t SPI_SCK = disabled;
     int8_t SPI_CS = disabled;

     // Temperature Sensor
     int8_t DS28b20_PIN = disabled;

     // CAN
     int8_t CAN_TX = 5;
     int8_t CAN_RX = 44;

     // Source ID Assignment Scheme
     // Device Type          Starting ID  Range       Total Devices
     // Gateway (Master)     0x00         0x00        1
     // Lasers               0         0 - 9 10
     // Motors               0x20         0x20 - 0x29 10
     // LEDs                 0x30         0x30 - 0x39 10
     // Sensors              0x40         0x40 - 0x49 10
     // Reserved             0xF0         0xF0 - 0xFF Management, Heartbeats

     // Variables for different IDs
     // addresses
     uint8_t CAN_ID_CENTRAL_NODE = 1; // this is similar to the master address

     uint8_t CAN_ID_MOT_A = 10;
     uint8_t CAN_ID_MOT_X = 11;
     uint8_t CAN_ID_MOT_Y = 12;
     uint8_t CAN_ID_MOT_Z = 13;
     uint8_t CAN_ID_MOT_B = 14;
     uint8_t CAN_ID_MOT_C = 15;
     uint8_t CAN_ID_MOT_D = 16;
     uint8_t CAN_ID_MOT_E = 17;
     uint8_t CAN_ID_MOT_F = 18;
     uint8_t CAN_ID_MOT_G = 19;

     uint8_t CAN_ID_LASER_0 = 20;
     uint8_t CAN_ID_LASER_1 = 21;
     uint8_t CAN_ID_LASER_2 = 22;
     uint8_t CAN_ID_LASER_3 = 23;

     uint8_t CAN_ID_LED_0 = 30;

     uint8_t CAN_ID_GALVO_0 = 40;

     // Secondary CAN ID for devices that listen to multiple addresses (e.g., illumination board)
     // Set to 0 to disable secondary address listening
     uint32_t CAN_ID_SECONDARY = 0;

     // Emergency stop
     int8_t pinEmergencyExit = disabled;
};

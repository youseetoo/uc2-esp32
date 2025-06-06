#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"

#define MOTOR_CONTROLLER
#define USE_TCA9535
#define BLUETOOTH
#define BTHID
//#define WIFI
#define HOME_MOTOR
#define LASER_CONTROLLER
#define DIGITAL_IN_CONTROLLER 
#define LED_CONTROLLER

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
     const unsigned long BAUDRATE = 115200;

     int8_t MOTOR_A_STEP = GPIO_NUM_15;
     int8_t MOTOR_X_STEP = GPIO_NUM_16;
     int8_t MOTOR_Y_STEP = GPIO_NUM_14;
     int8_t MOTOR_Z_STEP = GPIO_NUM_0;
     
     // THIS LIVES ON TCA
     int8_t MOTOR_ENABLE = 0; 
     int8_t MOTOR_X_DIR = 1;
     int8_t MOTOR_Y_DIR = 2;
     int8_t MOTOR_Z_DIR = 3;
     int8_t MOTOR_A_DIR = 4;

     
     /*
     WARNING PLEASE HANDLE WITH CARE
     */
    const bool dumpHeap = false;
    const uint16_t DEFAULT_TASK_PRIORITY = 0;
    const uint16_t MAIN_TASK_STACKSIZE = 8128;
    const uint16_t ANALOGJOYSTICK_TASK_STACKSIZE = 0;
    const uint16_t HIDCONTROLLER_EVENT_STACK_SIZE = 4* 2048; // Don't go below 2048
    const uint16_t HTTP_MAX_URI_HANDLERS = 35;
    const uint16_t BT_CONTROLLER_TASK_STACKSIZE = 4 * 2048; // TODO check if this is ending in stackoverflow
    const uint16_t MOTOR_TASK_STACKSIZE = 4 * 1024;
    const uint16_t MOTOR_TASK_UPDATEWEBSOCKET_STACKSIZE = 0;
    const uint16_t INTERRUPT_CONTROLLER_TASK_STACKSIZE = 0;
    const uint16_t TCA_TASK_STACKSIZE = 1024;
    const uint16_t SCANNER_TASK_STACKSIZE = 0;
    const uint16_t TEMPERATURE_TASK_STACKSIZE = 0; // 8096;



     bool isDualAxisZ = false;
     
     bool ENC_A_encoderDirection = true;  // true = count up, false = count down -> invert polarity
     bool ENC_X_encoderDirection = true; 
     bool ENC_Y_encoderDirection = true; 
     bool ENC_Z_encoderDirection = true; 
     bool ENC_A_motorDirection = true;  // true = count up, false = count down -> invert polarity
     bool ENC_X_motorDirection = true;
     bool ENC_Y_motorDirection = true;
     bool ENC_Z_motorDirection = true;

     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = true;
     int8_t AccelStepperMotorType = 1;

     int8_t LASER_1 = GPIO_NUM_12;
     int8_t LASER_2 = GPIO_NUM_4;
     int8_t LASER_3 = GPIO_NUM_2;

     int8_t LED_PIN = GPIO_NUM_13;
     int8_t LED_COUNT = 64;

     // FIXME: Is this redudant?!

     // THESE LIVE ON THE TCA
     int8_t DIGITAL_IN_1 = 5;
     int8_t DIGITAL_IN_2 = 6;
     int8_t DIGITAL_IN_3 = 7;

     int8_t dac_fake_1 = disabled; //GPIO_NUM_25; // RESET-ABORT just toggles between 1 and 0
     int8_t dac_fake_2 = disabled; //GPIO_NUM_26; // Coolant


     // const char * PSX_MAC = "1a:2b:3c:01:01:04";
     // int8_t PSX_CONTROLLER_TYPE = 2; // 1: PS3, 2: PS4

     int8_t JOYSTICK_SPEED_MULTIPLIER = 2;
     int8_t JOYSTICK_MAX_ILLU = 255;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 1;
     
     // for caliper
     int8_t ENC_X_A = GPIO_NUM_32;
     int8_t ENC_Y_A = GPIO_NUM_34;
     int8_t ENC_Z_A = GPIO_NUM_36;
     int8_t ENC_X_B = GPIO_NUM_33;
     int8_t ENC_Y_B = GPIO_NUM_35;
     int8_t ENC_Z_B = GPIO_NUM_17;

     // I2c
     int8_t I2C_SCL = GPIO_NUM_22; // This is for slave and master the same for the ESP32 board
     int8_t I2C_SDA = GPIO_NUM_21;
     int8_t I2C_ADD_TCA = 0x27;
     gpio_num_t I2C_INT = GPIO_NUM_27;


     // SPI
     int8_t SPI_MOSI = GPIO_NUM_17; // GPIO_NUM_23;
     int8_t SPI_MISO = GPIO_NUM_19;
     int8_t SPI_SCK = GPIO_NUM_18;
     int8_t SPI_CS = GPIO_NUM_5;

     // TMC - testing
     int8_t tmc_SW_TX = SPI_CS;// GPIO_NUM_44; // D7 -> GPIO44
     int8_t tmc_SW_RX = SPI_SCK;// GPIO_NUM_43; // D6 -> GPIO43
     int8_t tmc_pin_diag = SPI_MISO; // D3 -> GPIO4
     
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



     // WIFI - specific to SEEED microscope
     const char *mSSID = "UC2xSeeed-";
     const char *mPWD =  "";
     bool mAP = true; // false;
     const char *mSSIDAP = "UC2";            
     const char *hostname = "youseetoo";     

     // Temperature
     int8_t DS28b20_PIN = GPIO_NUM_25;
     int8_t LASER_0 = GPIO_NUM_26;
};
const UC2_3 pinConfig;

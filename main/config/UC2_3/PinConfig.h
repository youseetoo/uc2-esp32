#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
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
     /*#define FOCUS_MOTOR
     #define USE_TCA9535
     #define BLUETOOTH
     #define BTHID
     #define WIFI
     #define LED_CONTROLLER
     #define HOME_MOTOR
     #define LASER_CONTROLLER
     #define DIGITAL_IN_CONTROLLER*/
     const char * pindefName = "UC2_3";
     const unsigned long BAUDRATE = 500000;

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
     int8_t PIN_DEF_END_X = disabled;
     int8_t PIN_DEF_END_Y = disabled;
     int8_t PIN_DEF_END_Z = disabled;
     int8_t DIGITAL_IN_1 = PIN_DEF_END_X;
     int8_t DIGITAL_IN_2 = PIN_DEF_END_Y;
     int8_t DIGITAL_IN_3 = PIN_DEF_END_Z;

     // const char * PSX_MAC = "1a:2b:3c:01:01:04";
     // int8_t PSX_CONTROLLER_TYPE = 2; // 1: PS3, 2: PS4

     int8_t JOYSTICK_SPEED_MULTIPLIER = 30;
     int8_t JOYSTICK_MAX_ILLU = 100;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 30;
     

     // for caliper
     int8_t X_CAL_DATA = GPIO_NUM_32;
     int8_t Y_CAL_DATA = GPIO_NUM_34;
     int8_t Z_CAL_DATA = GPIO_NUM_36;
     int8_t X_CAL_CLK = GPIO_NUM_33;
     int8_t Y_CAL_CLK = GPIO_NUM_35;
     int8_t Z_CAL_CLK = GPIO_NUM_17;

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
};
const UC2_3 pinConfig;
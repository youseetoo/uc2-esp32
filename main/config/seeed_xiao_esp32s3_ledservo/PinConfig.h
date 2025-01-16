#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER
struct UC2_ESP32S3_XIAO_LEDSERVO : PinConfig
{
     /*
     D0: 1
     D1: 3
     D2: 3
     D3: 4
     D4: 5
     D5: 6
     D6: 43
     D7: 44
     D8: 7
     D9: 8
     D10: 9

     # XIAO

     {"task":"/ledarr_act", "qid":1, "led":{"LEDArrMode":1, "led_array":[{"id":0, "r":255, "g":255, "b":255}]}}
     {"task":"/ledarr_act", "qid":1, "led":{"LEDArrMode":1, "led_array":[{"id":0, "r":0, "g":0, "b":255}]}}

     {"task": "/laser_act", "LASERid":1, "LASERval": 500, "LASERdespeckle": 500,  "LASERdespecklePeriod": 100}
     {"task": "/laser_act", "LASERid":2, "LASERval": 0}
     {"task": "/laser_act", "LASERid":1, "LASERval": 1024}
     {"task": "/laser_act", "LASERid":1, "LASERval": 0}
     {"task": "/laser_act", "LASERid":2, "LASERval": 1024}
     */
     const char *pindefName = "UC2_esp32s3_xiao_ledservo";
     const unsigned long BAUDRATE = 115200;
     bool ENC_A_encoderDirection = true; // true = count up, false = count down -> invert polarity
     bool ENC_X_encoderDirection = true;
     bool ENC_Y_encoderDirection = true;
     bool ENC_Z_encoderDirection = true;
     bool ENC_A_motorDirection = true; // true = count up, false = count down -> invert polarity
     bool ENC_X_motorDirection = true;
     bool ENC_Y_motorDirection = true;
     bool ENC_Z_motorDirection = true;

     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = true;
     int8_t AccelStepperMotorType = 1;

     // Laser control pins (using updated GPIO values)
     int8_t LASER_0 = 44;         // D7 -> GPIO20 (Servo PWM 1) GPIO44 (U0RXD),
     int8_t LASER_1 = GPIO_NUM_4; // D3 => Motor 1/2  -> GPIO7 (Neopixel pin)
     int8_t LASER_2 = GPIO_NUM_3; // D2 => Motor 1/1
     int8_t LASER_3 = GPIO_NUM_7; // D8 -> GPIO6 (Servo PWM 2)

     int8_t DIGITAL_OUT_1 = GPIO_NUM_8; // D9     (Motor 2/2)
     int8_t DIGITAL_OUT_2 = GPIO_NUM_9; // D10    (Motor 2/1)
     int8_t DIGITAL_OUT_3 = disabled;

     int8_t LED_PIN = 43; // D6 Neopixel, GPIO43(U0TXD)
     int8_t LED_COUNT = 1;

     // Digital inputs (currently disabled) // TODO implement TOUCH
     int8_t DIGITAL_IN_1 = disabled;
     int8_t DIGITAL_IN_2 = disabled;
     int8_t DIGITAL_IN_3 = disabled;

     // Joystick configuration
     int8_t JOYSTICK_SPEED_MULTIPLIER = 30;
     int8_t JOYSTICK_MAX_ILLU = 100;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 30;

     // Caliper (currently disabled)
     int8_t ENC_X_A = disabled;
     int8_t ENC_Y_A = disabled;
     int8_t ENC_Z_A = disabled;
     int8_t ENC_X_B = disabled;
     int8_t ENC_Y_B = disabled;
     int8_t ENC_Z_B = disabled;

     // I2C configuration (using updated GPIO values)
     int8_t I2C_SCL = GPIO_NUM_6; // D5 -> GPIO6
     int8_t I2C_SDA = GPIO_NUM_5; // D4 -> GPIO5
};
const UC2_ESP32S3_XIAO_LEDSERVO pinConfig;

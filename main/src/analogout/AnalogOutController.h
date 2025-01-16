#pragma once
#include <PinConfig.h>
#include "cJSON.h"
#include "Arduino.h"

/*
    class is used to control up to 3 leds with esp32 LED PWM Controller on analogout pins
*/
namespace AnalogOutController
{
    static bool isDEBUG = false;
#define J1772_LEDC_TIMER LEDC_TIMER_0
#define J1772_LEDC_CHANNEL LEDC_CHANNEL_0
#define J1772_LEDC_TIMER_RES LEDC_TIMER_9_BIT
#define J1772_DUTY_MAX ((1 << LEDC_TIMER_9_BIT) - 1)
#define J1772_PWM_FREQUENCY_HZ 1000
#define J1772_LEDC_SPEEDMODE LEDC_HIGH_SPEED_MODE

    // PWM Stuff - ESP only
    static int pwm_resolution = 15;
    static int pwm_frequency = 80000; // 19000; //12000
    static int pwm_max = (int)pow(2, pwm_resolution);

    static int analogout_val_1 = 0;
    static int analogout_val_2 = 0;
    static int analogout_val_3 = 0;
    static int analogout_VIBRATE = 0;

    static int analogout_SOFI_1 = 0;
    static int analogout_SOFI_2 = 0;

    static int PWM_CHANNEL_analogout_1 = 4;
    static int PWM_CHANNEL_analogout_2 = 5;
    static int PWM_CHANNEL_analogout_3 = 6;

    int act(cJSON*  jsonDocument);
    cJSON* get(cJSON*  jsonDocument);

    void setup();
    void btcontroller_event(int left, int right, bool r1, bool r2 , bool l1, bool l2);
};
#include "../../config.h"
#pragma once
#include "../../config.h"
#include "ArduinoJson.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/periph_ctrl.h"
#include "soc/ledc_reg.h"
#include "../wifi/WifiController.h"
#include "../../Module.h"
#include "AnalogOutPins.h"

namespace RestApi
{
    void AnalogOut_act();
    void AnalogOut_get();
    void AnalogOut_set();
};

/*
    class is used to control up to 3 leds with esp32 LED PWM Controller on analogout pins
*/
class AnalogOutController : public Module
{
public:
    AnalogOutController();
    ~AnalogOutController();
    AnalogOutPins pins;
    bool DEBUG = false;
#define J1772_LEDC_TIMER LEDC_TIMER_0
#define J1772_LEDC_CHANNEL LEDC_CHANNEL_0
#define J1772_LEDC_TIMER_RES LEDC_TIMER_9_BIT
#define J1772_DUTY_MAX ((1 << LEDC_TIMER_9_BIT) - 1)
#define J1772_PWM_FREQUENCY_HZ 1000
#define J1772_LEDC_SPEEDMODE LEDC_HIGH_SPEED_MODE

    // PWM Stuff - ESP only
    int pwm_resolution = 15;
    int pwm_frequency = 80000; // 19000; //12000
    int pwm_max = (int)pow(2, pwm_resolution);

    int analogout_val_1 = 0;
    int analogout_val_2 = 0;
    int analogout_val_3 = 0;
    int analogout_VIBRATE = 0;

    int analogout_SOFI_1 = 0;
    int analogout_SOFI_2 = 0;

    int PWM_CHANNEL_analogout_1 = 4;
    int PWM_CHANNEL_analogout_2 = 5;
    int PWM_CHANNEL_analogout_3 = 6;

    DynamicJsonDocument act(DynamicJsonDocument  jsonDocument) override;
    DynamicJsonDocument set(DynamicJsonDocument  jsonDocument) override;
    DynamicJsonDocument get(DynamicJsonDocument  jsonDocument) override;

    void setup() override;
    void loop() override;
};
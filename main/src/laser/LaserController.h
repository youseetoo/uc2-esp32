#include <PinConfig.h>
#pragma once
#include "cJSON.h"
#include "Arduino.h"

namespace LaserController
{

    static bool isBusy;
    
    static int LASER_val_1 = 0;
    static int LASER_val_2 = 0;
    static int LASER_val_3 = 0;

    // PWM Stuff - ESP only

    /*
    int pwm_resolution = 15; 
    int pwm_frequency = 800000;  //19000; // 12000;
    int pwm_max = (int)pow(2,pwm_resolution);
    */

    static int pwm_resolution = 10; //8bit 256, 10bit  1024, 12bit 4096;
    static int pwm_frequency =  5000;//19000; //12000
    static long pwm_max = (int)pow(2, pwm_resolution);


    static int PWM_CHANNEL_LASER_1 = 0;
    static int PWM_CHANNEL_LASER_2 = 1;
    static int PWM_CHANNEL_LASER_3 = 2;

    // temperature dependent despeckeling?
    static int LASER_despeckle_1 = 0;
    static int LASER_despeckle_2 = 0;
    static int LASER_despeckle_3 = 0;
    static int PWM_CHANNEL_LASER_0 = 3;

    static int LASER_despeckle_period_1 = 20;
    static int LASER_despeckle_period_2 = 20;
    static int LASER_despeckle_period_3 = 20;

    static int minPulseWidth = 500;
    static int maxPulseWidth = 2500;

    static bool DEBUG = false;

    void LASER_despeckle(int LASERdespeckle, int LASERid, int LASERperiod);
    int act(cJSON * ob);
    void configurePWM(int servoPin, int resolution, int ledChannel, int frequency);
    void moveServo(int ledChannel, int angle, int frequency, int resolution);

    cJSON * get(cJSON *  ob);
    void setup();
    void loop();
    void setPWM(int pwmValue, int pwmChannel);

};


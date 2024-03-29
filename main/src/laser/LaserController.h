#pragma once
#include "../../Module.h"

class LaserController : public Module
{
private:
    /* data */
public:
    LaserController(/* args */);
    ~LaserController();

    bool isBusy;
    
    int LASER_val_1 = 0;
    int LASER_val_2 = 0;
    int LASER_val_3 = 0;

    // PWM Stuff - ESP only

    /*
    int pwm_resolution = 15; 
    int pwm_frequency = 800000;  //19000; // 12000;
    int pwm_max = (int)pow(2,pwm_resolution);
    */

    int pwm_resolution = pinConfig.pwm_resolution; //8bit 256, 10bit  1024, 12bit 4096;
    int pwm_frequency =  pinConfig.pwm_frequency; //19000; // 5000; //12000
    long pwm_max = (int)pow(2, pwm_resolution);

    int PWM_CHANNEL_LASER_1 = 0;
    int PWM_CHANNEL_LASER_2 = 1;
    int PWM_CHANNEL_LASER_3 = 2;
    int PWM_CHANNEL_LASER_0 = 3;

    // temperature dependent despeckeling?
    int LASER_despeckle_1 = 0;
    int LASER_despeckle_2 = 0;
    int LASER_despeckle_3 = 0;

    int LASER_despeckle_period_1 = 20;
    int LASER_despeckle_period_2 = 20;
    int LASER_despeckle_period_3 = 20;

    bool DEBUG = false;

    void LASER_despeckle(int LASERdespeckle, int LASERid, int LASERperiod);
    int act(cJSON * ob) override;

    void setPWM(int pwmValue, int pwmChannel);

    cJSON * get(cJSON *  ob) override;
    void setup() override;
    void loop() override;

};


#include <PinConfig.h>
#pragma once
#include "cJSON.h"
#include "Arduino.h"
#include "../bt/HidGamePad.h"

struct LaserData
{
    int LASERid;
    int LASERval;
    int LASERdespeckle;
    int LASERdespecklePeriod;
};

namespace LaserController
{

    static int LASER_val_0 = 0;
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

    int getLaserVal(int LASERid);
    bool setLaserVal(int LASERid, int LASERval);
    static int PWM_CHANNEL_LASER_0 = 4; // This is used for the Heating Unit 
    static int PWM_CHANNEL_LASER_1 = 1;
    static int PWM_CHANNEL_LASER_2 = 2;
    static int PWM_CHANNEL_LASER_3 = 3;

    // temperature dependent despeckeling?
    static int LASER_despeckle_0 = 0;
    static int LASER_despeckle_1 = 0;
    static int LASER_despeckle_2 = 0;
    static int LASER_despeckle_3 = 0;

    static int LASER_despeckle_period_0 = 20;
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
    void setupLaser(int laser_pin, int pwm_chan, int pwm_freq, int pwm_res);

    LaserData getLaserData();

    void dpad_changed_event(Dpad::Direction pressed);

};


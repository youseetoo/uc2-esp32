#include <PinConfig.h>
#pragma once
#include "cJSON.h"
#include "Arduino.h"
#include "../bt/HidGamePad.h"

struct LaserData
{
    int LASERid=0;
    int LASERval=0;
    int LASERdespeckle=0;
    int LASERdespecklePeriod=0;
};

namespace LaserController
{
    // Configuration constants
    static const int MAX_LASERS = 5+5; // Support for LASER IDs 0-4 // the upper 5 lasers are used for hybrid CAN mode

    // Arrays to store laser values and settings (indexed by LASERid)
    // Note: Using _arr suffix to avoid naming conflict with LASER_despeckle() function
    static int LASER_val_arr[MAX_LASERS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    static int LASER_despeckle_arr[MAX_LASERS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    static int LASER_despeckle_period_arr[MAX_LASERS] = {20, 20, 20, 20, 20, 20, 20, 20, 20, 20};
    
    // PWM channel mapping (indexed by LASERid)
    static int PWM_CHANNEL_LASER[MAX_LASERS] = {4, 1, 2, 3, 5, 0, 0, 0, 0, 0}; // 0=Heating, 1-4=Lasers

    // PWM configuration
    static int pwm_resolution = 10; //8bit 256, 10bit 1024, 12bit 4096;
    static int pwm_frequency = 5000; //19000; //12000
    static long pwm_max = (int)pow(2, pwm_resolution);

    // Helper functions to get pin and channel for a given LASERid
    int getLaserPin(int LASERid);
    int getPWMChannel(int LASERid);
    
    // Hybrid mode support: determines if a laser should use CAN or native driver
    // Returns true if laser should be routed to CAN bus, false for native driver
    bool shouldUseCANForLaser(int LASERid);
    
    // Hybrid mode support: converts internal hybrid laser ID (4,5,6,7) to CAN laser ID (0,1,2,3)
    // Used when sending commands to CAN satellites in hybrid mode
    int getCANLaserIdForHybrid(int LASERid);
    
    // Main API functions
    int getLaserVal(int LASERid);
    bool setLaserVal(int LASERid, int LASERval, int qid = 0);
    bool setLaserVal(int LASERid, int LASERval, int LASERdespeckle, int LASERdespecklePeriod, int qid = 0);
    
    // Backward compatibility - keep old defines for external code
    static int& PWM_CHANNEL_LASER_0 = PWM_CHANNEL_LASER[0];
    static int& PWM_CHANNEL_LASER_1 = PWM_CHANNEL_LASER[1];
    static int& PWM_CHANNEL_LASER_2 = PWM_CHANNEL_LASER[2];
    static int& PWM_CHANNEL_LASER_3 = PWM_CHANNEL_LASER[3];
    static int& PWM_CHANNEL_LASER_4 = PWM_CHANNEL_LASER[4];
    
    static int& LASER_val_0 = LASER_val_arr[0];
    static int& LASER_val_1 = LASER_val_arr[1];
    static int& LASER_val_2 = LASER_val_arr[2];
    static int& LASER_val_3 = LASER_val_arr[3];
    static int& LASER_val_4 = LASER_val_arr[4];
    
    static int& LASER_despeckle_0 = LASER_despeckle_arr[0];
    static int& LASER_despeckle_1 = LASER_despeckle_arr[1];
    static int& LASER_despeckle_2 = LASER_despeckle_arr[2];
    static int& LASER_despeckle_3 = LASER_despeckle_arr[3];
    static int& LASER_despeckle_4 = LASER_despeckle_arr[4];
    
    static int& LASER_despeckle_period_0 = LASER_despeckle_period_arr[0];
    static int& LASER_despeckle_period_1 = LASER_despeckle_period_arr[1];
    static int& LASER_despeckle_period_2 = LASER_despeckle_period_arr[2];
    static int& LASER_despeckle_period_3 = LASER_despeckle_period_arr[3];
    static int& LASER_despeckle_period_4 = LASER_despeckle_period_arr[4];

    static int minPulseWidth = 500;
    static int maxPulseWidth = 2500;

    static bool isDEBUG = false;

    void LASER_despeckle(int LASERdespeckle, int LASERid, int LASERperiod);
    int act(cJSON * ob);
    void configurePWM(int servoPin, int resolution, int ledChannel, int frequency);
    void moveServo(int ledChannel, int angle, int frequency, int resolution);

    cJSON * get(cJSON *  ob);
    void setup();
    void loop();
    void setPWM(int pwmValue, int pwmChannel);
    void setupLaser(int laser_pin, int pwm_chan, int pwm_freq, int pwm_res);
    void applyLaserValue(const LaserData& laserData);

    LaserData getLaserData();

    void dpad_changed_event(Dpad::Direction pressed);
    void processHoldActions(); // Process hold actions for continuous laser adjustment
    void handleShortClick(int direction); // Handle short click actions
    void executeHoldAction(int direction); // Execute hold increment/decrement actions
    void cross_changed_event(int pressed); // Handle Cross button for Laser 4 toggle
    
    // Send laser value update after setting new value (similar to sendMotorPos)
    void sendLaserValue(int LASERid, int qid = 0);

};


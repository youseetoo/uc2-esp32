#pragma once

#include "esp_err.h"
#include "PSController.h"
#include "cJSON.h"

namespace BtController
{
    static bool IS_PS_CONTROLER_LEDARRAY = false;
    static int offset_val = 1281; // make sure you do not accidentally turn on two directions at the same time
    static int stick_ly = 0;
    static int stick_lx = 0;
    static int stick_rx = 0;
    static int stick_ry = 0;

    static bool joystick_drive_X = false;
    static bool joystick_drive_Y = false;
    static bool joystick_drive_Z = false;
    static bool joystick_drive_A = false;
    static bool laser_on = false;
    static bool laser2_on = false;
    static bool led_on = false;

    static int speed_x = 0;
    static int speed_y = 0;
    static int speed_z = 0;

    static int logCounter;
    

    static int analogout_val_1 = 0;
    static int pwm_max = 0; // no idea how big it should be
    static int8_t sgn(int val);
    static PSController * psx = nullptr;
    void setupPS(char* mac, int type);
    void handelAxis(int value,int stepper);
    

    #define PAIR_MAX_DEVICES 20
    static char bda_str[18];

    static bool doConnect = false;
    static bool connected = false;
    static bool doScan = false;
    static bool ENABLE = false;
    static int BT_DISCOVER_TIME = 10000;

    void setup();
    void loop();
    cJSON * scanForDevices(cJSON *  doc);
    void removeAllPairedDevices();
    void setMacAndConnect(char* m);
    void connectPsxController(char* mac, int type);
    void removePairedDevice(char* pairedmac);
    cJSON * getPairedDevices(cJSON * doc);
    char * bda2str(const uint8_t *bda, char *str, size_t size);
    bool connectToServer();
    void btControllerLoop(void *p);
    
    
};
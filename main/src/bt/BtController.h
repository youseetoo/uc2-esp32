#pragma once

#include "esp_err.h"
#include "../../ModuleController.h"
#include "PSController.h"
#include "HidController.h"



void btControllerLoop(void *p);
class BtController : public Module
{
    private:
    bool IS_PS_CONTROLER_LEDARRAY = false;
    int offset_val = 2500; // make sure you do not accidentally turn on two directions at the same time
    int stick_ly = 0;
    int stick_lx = 0;
    int stick_rx = 0;
    int stick_ry = 0;

    bool joystick_drive_X = false;
    bool joystick_drive_Y = false;
    bool joystick_drive_Z = false;
    bool joystick_drive_A = false;
    bool laser_on = false;
    bool laser2_on = false;
    bool led_on = false;

    int speed_x = 0;
    int speed_y = 0;
    int speed_z = 0;

    int logCounter;
    

    int analogout_val_1 = 0;
    int pwm_max = 0; // no idea how big it should be
    int8_t sgn(int val);
    PSController * psx = nullptr;
    void setupPS(char* mac, int type);
    void handelAxis(int value,int stepper);
    

    public:
    BtController();
	~BtController();

    #define PAIR_MAX_DEVICES 20
    char bda_str[18];

    bool doConnect = false;
    bool connected = false;
    bool doScan = false;
    bool ENABLE = false;
    int BT_DISCOVER_TIME = 10000;

    void setup() override;
    void loop() override;
    int act(cJSON * doc) override;
    cJSON * get(cJSON * doc) override;
    cJSON * scanForDevices(cJSON *  doc);
    void removeAllPairedDevices();
    void setMacAndConnect(char* m);
    void connectPsxController(char* mac, int type);
    void removePairedDevice(char* pairedmac);
    cJSON * getPairedDevices(cJSON * doc);
    char * bda2str(const uint8_t *bda, char *str, size_t size);
    bool connectToServer();
    
    
    
};
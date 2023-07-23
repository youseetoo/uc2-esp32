#pragma once

#include "esp_err.h"
#include <ArduinoJson.h>
#include "../../ModuleController.h"
#include "PSController.h"
#include "../../PinConfig.h"
#include "HidController.h"

namespace RestApi
{
    /*
    returns an array that contains the visible bt devices
        endpoint:/bt_scan
        input[]
        output
        [
            {
                "name" :"HyperX",
                "mac": "01:02:03:04:05:06"
            }
            ,
            {
                "name": "",
                "mac": "01:02:03:04:05:06"
            },
        ]
    */
    void Bt_startScan();
    void Bt_connect();
    void Bt_getPairedDevices();
    void Bt_remove();
};

void btControllerLoop(void *p);
class BtController : public Module
{
    private:
    bool IS_PS_CONTROLER_LEDARRAY = false;
    int offset_val = 1281; // make sure you do not accidentally turn on two directions at the same time
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
    void setupPS(String mac, int type);
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
    int act(DynamicJsonDocument doc) override;
    DynamicJsonDocument get(DynamicJsonDocument doc) override;
    DynamicJsonDocument scanForDevices(DynamicJsonDocument  doc);
    void removeAllPairedDevices();
    void setMacAndConnect(String m);
    void connectPsxController(String mac, int type);
    void removePairedDevice(String pairedmac);
    DynamicJsonDocument getPairedDevices(DynamicJsonDocument doc);
    char * bda2str(const uint8_t *bda, char *str, size_t size);
    bool connectToServer();
    
    
    
};
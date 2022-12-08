#pragma once


#include "PsController.h"
#include "../../ModuleController.h"

static int8_t sgn(int val)
{
	if (val < 0)
		return -1;
	if (val == 0)
		return 0;
	return 1;
}

class PsXController
{
    private:
    PSController * psx = nullptr;
    bool IS_PS_CONTROLER_LEDARRAY = false;
    int offset_val = 5; // make sure you do not accidentally turn on two directions at the same time
    int stick_ly = 0;
    int stick_lx = 0;
    int stick_rx = 0;
    int stick_ry = 0;

    bool joystick_drive_X = false;
    bool joystick_drive_Y = false;
    bool joystick_drive_Z = false;
    bool joystick_drive_A = false;

    int speed_x = 0;
    int speed_y = 0;
    int speed_z = 0;
    int global_speed = 10; // multiplier for the speed

    int analogout_val_1 = 0;
    int pwm_max = 0; // no idea how big it should be
    public:
    /*
    fake mac address that is stored inside the ps controller for paring
    type 1 = ps3, 2 = ps4
    */
    void setup(String mac, int type);
    void loop();
};
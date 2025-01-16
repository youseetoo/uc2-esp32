
#pragma once
#include <PinConfig.h>
#include "esp_err.h"
#ifdef PSXCONTROLLER
#include "PSController.h"
#endif
#include "cJSON.h"
#include "HidGamePad.h"

namespace BtController
{

    static int8_t sgn(int val);
    #ifdef PSXCONTROLLER
    static PSController * psx = nullptr;
    void setupPS(char* mac, int type);
    void connectPsxController(char* mac, int type);
    #endif
    

    #define PAIR_MAX_DEVICES 20
    static char bda_str[18];


    void setup();
    void loop();
    cJSON * scanForDevices(cJSON *  doc);
    void removeAllPairedDevices();
    void setMacAndConnect(char* m);
    
    void removePairedDevice(char* pairedmac);
    cJSON * getPairedDevices(cJSON * doc);
    char * bda2str(const uint8_t *bda, char *str, size_t size);
    bool connectToServer();
    void btControllerLoop(void *p);
    void setCrossChangedEvent(void (*cross_changed_event)(int pressed));
    void setCircleChangedEvent(void(*circle_changed_event)(int pressed));
    void setTriangleChangedEvent(void (*triangle_changed_event)(int pressed));
    void setSquareChangedEvent(void (*square_changed_event)(int pressed));
    void setDpadChangedEvent(void (*dpad_changed_event)(Dpad::Direction pressed));
    void setXYZAChangedEvent(void (*xyza_changed_event)(int x, int y,int z, int a));
    void setAnalogControllerChangedEvent(void (*analogcontroller_event)(int left, int right, bool r1, bool r2, bool l1, bool l2));
};

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
    void handleAxis(int value,int stepper);
    

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
    
    void removePairedDevice(char* pairedmac);
    cJSON * getPairedDevices(cJSON * doc);
    char * bda2str(const uint8_t *bda, char *str, size_t size);
    bool connectToServer();
    void btControllerLoop(void *p);
    
    static void (*cross_changed_event)(uint8_t pressed);
    static void (*circle_changed_event)(uint8_t pressed);
    static void (*triangle_changed_event)(uint8_t pressed);
    static void (*square_changed_event)(uint8_t pressed);
    static void (*dpad_changed_event)(Dpad::Direction pressed);
    static void (*xyza_changed_event)(int x, int y,int z, int a);
    static void (*analogcontroller_event)(int left, int right, bool r1, bool r2, bool l1, bool l2);
};
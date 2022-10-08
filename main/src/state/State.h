#pragma once
#include "../../config.h"
#include "ArduinoJson.h"
#include "../../pinstruct.h"
#include "../config/JsonKeys.h"
#include "esp_log.h"
#if defined IS_PS3 || defined IS_PS4
#include "../gamepads/ps_3_4_controller.h"
#endif
#include "../wifi/WifiController.h"

static int8_t sgn(int val) {
  if (val < 0) return -1;
  if (val == 0) return 0;
  return 1;
}

class State
{
    public:
    State();
    ~State();
    bool DEBUG = false;
    PINDEF * pins;

    const char*  identifier_name = "UC2_Feather";
    const char*  identifier_id = "V1.2";
    const char*  identifier_date = __DATE__ "" __TIME__;
    const char*  identifier_author = "BD";
    const char* IDENTIFIER_NAME = "";


    // timing variables
    unsigned long startMillis;
    unsigned long currentMillis;
    bool isBusy = false;

    void act();
    void set();
    void get();
    void setup(PINDEF * pins);
    void printInfo();

    void getDefaultPinDef(PINDEF * pindef);
};

extern State state;
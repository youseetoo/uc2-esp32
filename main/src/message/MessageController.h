#include <PinConfig.h>
#pragma once
#include "Adafruit_NeoPixel.h"
#include "cJSON.h"





namespace MessageController
{
    // We use the strip instead of the matrix to ensure different dimensions; Convesion of the pattern has to be done on the cliet side!
    static bool DEBUG = false;
    static bool isBusy;
    static bool isOn = false;

    int act(cJSON * ob);
    cJSON * get(cJSON *  ob);
};

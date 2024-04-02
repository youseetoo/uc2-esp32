#include <PinConfig.h>
#pragma once
#include "cJSON.h"

namespace MessageController
{
    int act(cJSON * ob);
    cJSON * get(cJSON *  ob);
    void sendMesageSerial(int key, int value);
};

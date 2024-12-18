#pragma once

#include "cJSON.h"

namespace MotorJsonParser
{
    int act(cJSON *doc);
    cJSON *get(cJSON *docin);
};
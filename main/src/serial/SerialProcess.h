#pragma once
#include "cJSON.h"


namespace SerialProcess
{
    void jsonProcessor(char * task,cJSON * jsonDocument);
    void serialize(cJSON * doc);
    void serialize(int success);
    void setup();
    void loop();
};


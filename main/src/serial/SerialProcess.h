#pragma once
#include "cJSON.h"


class SerialProcess
{
private:
    void jsonProcessor(char * task,cJSON * jsonDocument);
    void serialize(cJSON * doc);
    void serialize(int success);
    /* data */
public:
    SerialProcess(/* args */);
    ~SerialProcess();
    void loop();
};

extern SerialProcess serial;

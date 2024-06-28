#pragma once
#include "cJSON.h"
#include "OneWire.h"; 
#include "DallasTemperature.h"; 



namespace DS18b20Controller
{
    
    float readTemperature();
    
    void setup();
    int act(cJSON* jsonDocument);
    cJSON* get(cJSON* jsonDocument);
    void readSensorTask(void* parameter);
    float getCurrentValueCelcius();
};


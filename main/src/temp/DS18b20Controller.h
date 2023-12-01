#pragma once
#include "../../Module.h"
#include "../../ModuleController.h"
#include "OneWire.h"; 
#include "DallasTemperature.h"; 



class DS18b20Controller : public Module
{
private:
    /* data */
public:
    DS18b20Controller(/* args */);
    ~DS18b20Controller();
    bool DEBUG = false;

    long lastReading = 0;
    float readingPeriod = 1000; // ms
    float currentValueCelcius = 0.0;
    
    bool isRequestingTemperature = false;
    long requestStartTime = 0;
    const long MAX_WAIT = 1000; // ms
    DeviceAddress *mDS18B20Addresses;
    OneWire *mOneWire; 
    DallasTemperature *mDS18B20;
    float readTemperature();
    TaskHandle_t temperatureTaskHandle;
    void setup() override;
    int act(cJSON* jsonDocument) override;
    cJSON* get(cJSON* jsonDocument) override;
    void loop() override;
    void readSensorTask(void* parameter);
};


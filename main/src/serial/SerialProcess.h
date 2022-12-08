#pragma once
#include "ArduinoJson.h"
#include "../analogin/AnalogInController.h"
#include "../scanner/ScannerController.h"
#include "../digitalout/DigitalOutController.h"
#include "../digitalin/DigitalInController.h"
#include "../dac/DacController.h"
#include "../config/ConfigController.h"
#include "../wifi/Endpoints.h"

class SerialProcess
{
private:
    void jsonProcessor(String task,JsonObject jsonDocument);
    void serialize(DynamicJsonDocument doc);
    void serialize(int success);
    /* data */
public:
    SerialProcess(/* args */);
    ~SerialProcess();
    void loop();
};

extern SerialProcess serial;

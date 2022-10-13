#include "../../config.h"
#pragma once
#include "ArduinoJson.h"
#include "../analog/AnalogController.h"
#include "../state/State.h"
#include "../scanner/ScannerController.h"
#include "../digital/DigitalController.h"
#include "../dac/DacController.h"
#include "../config/ConfigController.h"
#include "../wifi/Endpoints.h"

class SerialProcess
{
private:
    void jsonProcessor(String task,DynamicJsonDocument * jsonDocument);
    void tableProcessor(DynamicJsonDocument * jsonDocument);
    /* data */
public:
    SerialProcess(/* args */);
    ~SerialProcess();
    void loop(DynamicJsonDocument * jsonDocument);
};

extern SerialProcess serial;

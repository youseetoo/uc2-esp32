#include "../../config.h"
#pragma once
#include "ArduinoJson.h"
#include "../analogin/AnalogInController.h"
#include "../state/State.h"
#include "../scanner/ScannerController.h"
#include "../digitalout/DigitalOutController.h"
#include "../digitalin/DigitalInController.h"
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
    void loop();
};

extern SerialProcess serial;

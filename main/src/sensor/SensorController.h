#include "../../config.h"
#ifdef IS_READSENSOR
#pragma once
#include "ArduinoJson.h"
#include "../wifi/WifiController.h"
#include "../../Module.h"
#include "SensorPins.h"

class SensorController : public Module
{
private:
    /* data */
public:
    SensorController(/* args */);
    ~SensorController();
    bool DEBUG = false;
    SensorPins pins;

    int N_sensor_avg; //no idea if it should be equal to that that one inside PidController.h 

    void setup() override;
    void act() override;
    void set() override;
    void get() override;
    void loop() override;
};

#endif

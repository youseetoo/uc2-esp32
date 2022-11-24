#pragma once
#include <esp32-hal-log.h>
#include <ArduinoJson.h>

class Module
{
    public:
    Module();
	virtual ~Module();
    virtual void setup() = 0;
    virtual void loop() = 0;
    virtual DynamicJsonDocument act(DynamicJsonDocument doc) = 0;
    virtual DynamicJsonDocument set(DynamicJsonDocument doc) = 0;
    virtual DynamicJsonDocument get(DynamicJsonDocument doc) = 0;
};
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
    virtual void act(JsonObject job) = 0;
    virtual void set(JsonObject job) = 0;
    virtual void get(JsonObject job) = 0;
};
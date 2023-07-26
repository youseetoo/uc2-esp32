#pragma once
#include <ArduinoJson.h>
#include "PinConfig.h"
#include "JsonKeys.h"

class Module
{
    public:
    Module();
	virtual ~Module();
    virtual void setup() = 0;
    virtual void loop() = 0;
    virtual int act(DynamicJsonDocument doc) = 0;
    virtual DynamicJsonDocument get(DynamicJsonDocument doc) = 0;
};
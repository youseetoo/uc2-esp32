#pragma once
#include <esp32-hal-log.h>

class Module
{
    public:
    Module();
	virtual ~Module();
    virtual void setup() = 0;
    virtual void loop() = 0;
    virtual void act() = 0;
    virtual void set() = 0;
    virtual void get() = 0;
};
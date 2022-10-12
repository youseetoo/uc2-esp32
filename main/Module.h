#pragma once

class Module
{
    public:
    virtual void setup();
    virtual void begin();
    virtual void loop();
    virtual void act();
    virtual void set();
    virtual void get();
};
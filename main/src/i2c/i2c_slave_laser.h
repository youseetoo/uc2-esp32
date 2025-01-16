#pragma once

namespace i2c_slave_laser   
{
    void setup();
    void loop();

    void setI2CAddress(int address);
    int getI2CAddress();
};

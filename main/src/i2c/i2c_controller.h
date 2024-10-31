#pragma once
#include "Arduino.h"
#include "cJsonTool.h"
#include "JsonKeys.h"
#include "Wire.h"

namespace i2c_controller
{

    static const int MAX_I2C_DEVICES = 20;  // Maximum number of expected devices
    static byte i2cAddresses[MAX_I2C_DEVICES]; // Array to store found I2C addresses
    static int numDevices = 0; // Variable to keep track of number of devices found
    static int i2cRescanTick = 0; // Variable to keep track of number of devices found
    static int i2cRescanAfterNTicks = -1; // Variable to keep track of number of devices found
    void setup();
    int act(cJSON * ob);
    cJSON * get(cJSON *  ob);
    void sendJsonString(String jsonString, uint8_t slave_addr);
    void receiveEvent(int numBytes);
    void requestEvent();
    bool isAddressInI2CDevices(byte addressToCheck);
    void parseDialEvent(int numBytes);
    void parseMotorEvent(int numBytes);
};
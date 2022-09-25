#pragma once

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>

namespace BtController
{
    void setup();
    void scanForDevices(DynamicJsonDocument * jdoc);
}
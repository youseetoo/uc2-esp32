#include "BtController.h"

namespace BtController
{
    void setup()
    {
        BLEDevice::init("");
    }

    void scanForDevices(DynamicJsonDocument * jdoc)
    {
        log_i("Start scanning BT");
        BLEScan* pBLEScan = BLEDevice::getScan();
        pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
        pBLEScan->setInterval(100);
        pBLEScan->setWindow(99);  // less or equal setInterval value
        BLEScanResults foundDevices = pBLEScan->start(5, false);
        int counter = 0;
        while (foundDevices.getCount() == 0 && counter < 10)
        {
            delay(200);
            counter++;
            log_i("Scanning %i", counter);
        }
        log_i("Found devices:%i", foundDevices.getCount());
        (*jdoc).clear();
        for (int i = 0; i < foundDevices.getCount(); i++)
        {
            log_i("Device %i name: %s mac: %s", i, foundDevices.getDevice(i).getName().c_str(), foundDevices.getDevice(i).getAddress().toString().c_str());
            JsonObject ob = (*jdoc).createNestedObject();
            ob["name"] = foundDevices.getDevice(i).getName();
            ob["mac"] = foundDevices.getDevice(i).getAddress().toString();
        }
        pBLEScan->clearResults();
        pBLEScan->stop();
    }
}
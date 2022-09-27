#include "BtController.h"

namespace BtController
{

    BLEUUID HID_SERVICE_UUID("00001812-0000-1000-8000-00805f9b34fb");

    bool doConnect = false;
    bool connected = false;
    bool doScan = false;
    BLERemoteCharacteristic *pRemoteCharacteristic;
    BLEAdvertisedDevice *myDevice;
    BLEAddress *mac;
    bool ENABLE = false;

    void my_gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
    {
        log_i("custom gattc event handler, event: %d", (uint8_t)event);
    }

    void my_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gatts_cb_param_t *param)
    {
        log_i("custom gatts event handler, event: %d", (uint8_t)event);
    }

    void my_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
    {
        log_i("custom gap event handler, event: %d", (uint8_t)event);
    }

    void setup()
    {
        //BLEDevice::setCustomGapHandler(my_gap_event_handler);
        //BLEDevice::setCustomGattsHandler(my_gatts_event_handler);
        //BLEDevice::setCustomGattcHandler(my_gattc_event_handler);
        //BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
        BLEDevice::init("ESP32-BLE-1");
    }

    void scanForDevices(DynamicJsonDocument *jdoc)
    {
        log_i("Start scanning BT");
        BLEScan *pBLEScan = BLEDevice::getScan();
        BLEScanResults foundDevices = scanAndGetResult(pBLEScan);
        (*jdoc).clear();
        for (int i = 0; i < foundDevices.getCount(); i++)
        {
            log_i("Device %i %s", i, foundDevices.getDevice(i).toString().c_str());
            JsonObject ob = (*jdoc).createNestedObject();
            ob["name"] = foundDevices.getDevice(i).getName();
            ob["mac"] = foundDevices.getDevice(i).getAddress().toString();
        }
        //pBLEScan->clearResults();
        //pBLEScan->stop();
    }

    BLEScanResults scanAndGetResult(BLEScan *pBLEScan)
    {
        BLEScanResults foundDevices = pBLEScan->start(5, false);
        int counter = 0;
        while (foundDevices.getCount() == 0 && counter < 10)
        {
            delay(200);
            counter++;
            log_i("Scanning %i", counter);
        }
        log_i("Found devices:%i", foundDevices.getCount());
        return foundDevices;
    }

    void notifyCallback(
        BLERemoteCharacteristic *pBLERemoteCharacteristic,
        uint8_t *pData,
        size_t length,
        bool isNotify)
    {
        log_i("Notify callback for characteristic ");
        log_i("%s", pBLERemoteCharacteristic->getUUID().toString().c_str());
        log_i(" of data length ");
        log_i("%i", length);
        log_i("data: ");
        log_i("%s", pData);
    }

    void setMacAndConnect(String m)
    {

        mac = new BLEAddress(m.c_str());
        log_i("input mac %s  BLEAdress: %s", m.c_str(), mac->toString().c_str());
        BLEScan *pBLEScan = BLEDevice::getScan();
        BLEScanResults foundDevices = scanAndGetResult(pBLEScan);

        for (int i = 0; i < foundDevices.getCount(); i++)
        {
            BLEAdvertisedDevice advertisedDevice = foundDevices.getDevice(i);
            log_i("advertisedDevice %i %s ", i, advertisedDevice.toString().c_str());
            log_i("haveServiceUUID %s isHidService %s  have macAdress: %s",
                  boolToChar(advertisedDevice.haveServiceUUID()),
                  boolToChar(advertisedDevice.isAdvertisingService(HID_SERVICE_UUID)),
                  boolToChar(advertisedDevice.getAddress().equals((*mac))));
            if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(HID_SERVICE_UUID) && advertisedDevice.getAddress().equals((*mac)))
            {
                log_i("found matching device %i ", i);
                // BLEDevice::getScan()->stop();
                myDevice = &advertisedDevice;
                doConnect = true;

            } // Found our server
        }
        //pBLEScan->clearResults();
        //pBLEScan->stop();
        if (doConnect)
        {
            log_i("connectToServer");
            delay(1000);
            connectToServer();
        }
        else
            log_i("failed to find device");
    }

    bool connectToServer()
    {
        log_i("Forming a connection to ");
        //log_i("%s", myDevice->getAddress().toString().c_str());
        
        BLEClient *pClient = BLEDevice::createClient();
        //pClient->setMTU(23);
        log_i(" - Created client");

        // pClient->setClientCallbacks(new MyClientCallback());
        //  Connect to the remove BLE Server.
        bool connected = pClient->connect(myDevice);

        log_i(" - Connected to server %s", boolToChar(connected));
        if (!connected)
            return false;

        // pClient->setMTU(517); // set client to request maximum MTU from server (default is 23 otherwise)
        log_i("try to find service UUID: ");
        // Obtain a reference to the service we are after in the remote BLE server.
        BLERemoteService *pRemoteService = pClient->getService(HID_SERVICE_UUID);
        if (pRemoteService == nullptr)
        {
            log_i("Failed to find our service UUID: ");
            log_i("%s", HID_SERVICE_UUID.toString().c_str());
            pClient->disconnect();
            return false;
        }
        log_i(" - Found our service");

        std::map<std::string, BLERemoteCharacteristic *> *pCharacteristicMap = (*pRemoteService).getCharacteristics();
        std::map<std::string, BLERemoteCharacteristic *>::iterator itr;
        for (itr = (*pCharacteristicMap).begin(); itr != (*pCharacteristicMap).end(); itr++)
        {
            log_i("%s", itr->first);
            log_i("%s", itr->second->toString().c_str());
        }

        // Obtain a reference to the characteristic in the service of the remote BLE server.
        pRemoteCharacteristic = pRemoteService->getCharacteristic(pRemoteService->getUUID());
        if (pRemoteCharacteristic == nullptr)
        {
            log_i("Failed to find our characteristic UUID: ");
            log_i("%s", pRemoteService->getUUID().toString().c_str());
            pClient->disconnect();
            return false;
        }
        log_i(" - Found our characteristic");

        // Read the value of the characteristic.
        if (pRemoteCharacteristic->canRead())
        {
            std::string value = pRemoteCharacteristic->readValue();
            log_i("The characteristic value was: ");
            log_i("%s", value.c_str());
        }

        if (pRemoteCharacteristic->canNotify())
            pRemoteCharacteristic->registerForNotify(notifyCallback);

        connected = true;
        return true;
    }
}
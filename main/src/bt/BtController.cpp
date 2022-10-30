#include "BtController.h"

namespace RestApi
{
    void Bt_startScan()
    {
        deserialize();
        BtController::scanForDevices(WifiController::getJDoc());
        serialize();
    }

    void Bt_connect()
    {
        deserialize();
        String mac = (*WifiController::getJDoc())["mac"];
        int ps = (*WifiController::getJDoc())["psx"];
       
        if (ps == 0)
        {
            BtController::setMacAndConnect(mac);
        }
        else 
        {
            ps_c.start(mac);
        }
        
        
        WifiController::getJDoc()->clear();
        serialize();
    }

    void Bt_getPairedDevices()
    {
        deserialize();
        BtController::getPairedDevices(WifiController::getJDoc());
        serialize();
    }

    void Bt_remove()
    {
        deserialize();
        String mac = (*WifiController::getJDoc())["mac"];
        BtController::removePairedDevice(mac);
        WifiController::getJDoc()->clear();
        serialize();
    }
}

namespace BtController
{

    BluetoothSerial btClassic;

    bool doConnect = false;
    bool connected = false;
    bool doScan = false;
    bool ENABLE = false;
    int BT_DISCOVER_TIME = 10000;
    BTAddress *mac;

    void setup()
    {
        //BLEDevice::setCustomGapHandler(my_gap_event_handler);
        //BLEDevice::setCustomGattsHandler(my_gatts_event_handler);
        //BLEDevice::setCustomGattcHandler(my_gattc_event_handler);
        //BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
        btClassic.begin("ESP32-BLE-1");
    }

    void scanForDevices(DynamicJsonDocument *jdoc)
    {
        log_i("Start scanning BT");
        BTScanResults *foundDevices = btClassic.discover(BT_DISCOVER_TIME);
        (*jdoc).clear();
        for (int i = 0; i < foundDevices->getCount(); i++)
        {
            log_i("Device %i %s", i, foundDevices->getDevice(i)->toString().c_str());
            JsonObject ob = (*jdoc).createNestedObject();
            ob["name"] = foundDevices->getDevice(i)->getName();
            ob["mac"] = foundDevices->getDevice(i)->getAddress().toString();
        }
        // pBLEScan->clearResults();
        // pBLEScan->stop();
    }

#define PAIR_MAX_DEVICES 20
    char bda_str[18];

    char *bda2str(const uint8_t *bda, char *str, size_t size)
    {
        if (bda == NULL || str == NULL || size < 18)
        {
            return NULL;
        }
        sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
                bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
        return str;
    }

    void getPairedDevices(DynamicJsonDocument *jdoc)
    {
        jdoc->clear();
        int count = esp_bt_gap_get_bond_device_num();
        if (!count)
        {
            log_i("No bonded device found.");
            JsonObject ob = (*jdoc).createNestedObject();
            ob["name"] = "No bonded device found";
        }
        else
        {
            log_i("Bonded device count: %d", count);
            if (PAIR_MAX_DEVICES < count)
            {
                count = PAIR_MAX_DEVICES;
                log_i("Reset bonded device count: %d", count);
            }
            esp_bd_addr_t pairedDeviceBtAddr[PAIR_MAX_DEVICES];
            esp_err_t tError = esp_bt_gap_get_bond_device_list(&count, pairedDeviceBtAddr);
            if (ESP_OK == tError)
            {
                for (int i = 0; i < count; i++)
                {
                    JsonObject ob = (*jdoc).createNestedObject();
                    ob["name"] = "";
                    ob["mac"] = bda2str(pairedDeviceBtAddr[i], bda_str, 18);
                }
            }
        }
    }

    void setMacAndConnect(String m)
    {

        mac = new BTAddress(m.c_str());
        log_i("input mac %s  BLEAdress: %s", m.c_str(), mac->toString().c_str());
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
        // log_i("%s", myDevice->getAddress().toString().c_str());
        /*btClassic.connect
        BLEClient *pClient = BLEDevice::createClient();
        // pClient->setMTU(23);
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
        return true;*/
    }

    void removePairedDevice(String pairedmac)
    {
        BTAddress * add = new BTAddress(pairedmac.c_str());
        esp_err_t tError = esp_bt_gap_remove_bond_device((uint8_t*)add->getNative());
        log_i("paired device removed:%s", boolToChar(tError == ESP_OK));
    }
}
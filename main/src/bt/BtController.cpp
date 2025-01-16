
#include "BtController.h"
#include <esp_log.h>
#include "../config/ConfigController.h"
#ifdef BTHID
#include "HidController.h"
#endif
#include "Arduino.h"
#include "JsonKeys.h"


namespace BtController
{
    GamePadData lastData;

    void (*cross_changed_event)(int pressed);
    void (*circle_changed_event)(int pressed);
    void (*triangle_changed_event)(int pressed);
    void (*square_changed_event)(int pressed);
    void (*dpad_changed_event)(Dpad::Direction pressed);
    void (*xyza_changed_event)(int x, int y,int z, int a);
    void (*analogcontroller_event)(int left, int right, bool r1, bool r2, bool l1, bool l2);


    void btControllerLoop(void *p)
    {
        for (;;)
        {
            loop();
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
    }

    void setCrossChangedEvent(void (*cross_changed_event1)(int pressed))
    {
        cross_changed_event = cross_changed_event1;
    }

    void setCircleChangedEvent(void (*circle_changed_event1)(int pressed))
    {
        circle_changed_event = circle_changed_event1;
    }

    void setTriangleChangedEvent(void (*triangle_changed_event1)(int pressed))
    {
        triangle_changed_event = triangle_changed_event1;
    }

    void setSquareChangedEvent(void (*square_changed_event1)(int pressed))
    {
        square_changed_event = square_changed_event1;
    }

    void setDpadChangedEvent(void (*dpad_changed_event1)(Dpad::Direction pressed))
    {
        dpad_changed_event = dpad_changed_event1;
    }

    void setXYZAChangedEvent(void (*xyza_changed_event1)(int x, int y, int z, int a))
    {
        xyza_changed_event = xyza_changed_event1;
    }

    void setAnalogControllerChangedEvent(void (*analogcontroller_event1)(int left, int right, bool r1, bool r2, bool l1, bool l2))
    {
        analogcontroller_event = analogcontroller_event1;
    }

    void setup()
    {

// get the bluetooth config
#ifdef PSXCONTROLLER
        char *m = Config::getPsxMac();
        int type = Config::getPsxControllerType();

        if (m != NULL && pinConfig.PSX_MAC != NULL)
        {
            // always remove all the devices?
            removeAllPairedDevices();
            m = (char *)pinConfig.PSX_MAC;
            type = pinConfig.PSX_CONTROLLER_TYPE;
        }

        // if the mac is not empty, try to connect to the psx controller
        if (m != NULL)
        {
            // initiate either PS3 or PS4 controller
            log_i("Connecting to PSX controller");
            setupPS(m, type);
        }
#endif
#ifdef BTHID
        setupHidController();
#endif
        xTaskCreate(&btControllerLoop, "btController_task", pinConfig.BT_CONTROLLER_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, NULL);
    }

#ifdef PSXCONTROLLER
    void setupPS(char *mac, int type)
    {
        // select the type of wifi controller - either PS3 or PS4
        if (type == 1)
        {
            log_d("Connecting to PS3");
            psx = new PSController(nullptr, PSController::kPS3);
        }
        else if (type == 2)
        {
            log_d("Connecting to PS4");
            psx = new PSController(nullptr, PSController::kPS4);
        }
        bool returnBluetooth = psx->startListening(mac);
        log_d("Return Bluetooth %i", returnBluetooth);
    }
#endif

    void checkdata(int first, int sec, void (*event)(int t))
    {
        if (first != sec)
        {
            first = sec;
            if (event != nullptr)
                event(first);
        }
    }

#ifdef BTHID

    void loop()
    {
        //log_i("hid connected:%i", hidIsConnected);
        if (hidIsConnected)
        {
            //log_i("cross_changed_event nullptr %i", cross_changed_event == nullptr);
            checkdata(lastData.cross, gamePadData.cross, cross_changed_event);
            checkdata(lastData.circle, gamePadData.circle, circle_changed_event);
            checkdata(lastData.triangle, gamePadData.triangle, triangle_changed_event);
            checkdata(lastData.square, gamePadData.square, square_changed_event);
            if(lastData.dpaddirection != gamePadData.dpaddirection)
            {
                lastData.dpaddirection = gamePadData.dpaddirection;
                if(dpad_changed_event != nullptr)
                    dpad_changed_event(lastData.dpaddirection);
            }

            /*
                zvalue = gamePadData.LeftY;
                xvalue = gamePadData.RightX;
                yvalue = gamePadData.RightY;
                avalue = gamePadData.LeftX;
            */
            //log_i("xyza_changed_event nullptr %i", xyza_changed_event == nullptr);
            if (xyza_changed_event != nullptr)
                xyza_changed_event(gamePadData.LeftY, gamePadData.RightX, gamePadData.RightY, gamePadData.LeftX);
            lastData.LeftX = gamePadData.LeftY;
            lastData.RightX = gamePadData.RightX;
            lastData.RightY = gamePadData.RightY;
            lastData.LeftY = gamePadData.LeftX;

            int left = gamePadData.dpaddirection == Dpad::Direction::left;
            int right = gamePadData.dpaddirection == Dpad::Direction::right;
            bool r1 = gamePadData.r1;
            bool r2 = gamePadData.r2;
            bool l1 = gamePadData.l1;
            bool l2 = gamePadData.l2;
            if (analogcontroller_event != nullptr)
                analogcontroller_event(left, right, r1, r2, l1, l2);
        }
    }
#endif

#ifdef PSXCONTROLLER

    void loop()
    {
        if (psx != nullptr && psx->isConnected())
        {
            checkdata(lastData.cross, psx->event.button_down.cross, cross_changed_event);
            checkdata(lastData.circle, psx->event.button_down.circle, circle_changed_event);
            checkdata(lastData.triangle, psx->event.button_down.triangle, triangle_changed_event);
            checkdata(lastData.square, psx->event.button_down.square, square_changed_event);
            Dpad::Direction dir;
            if (psx->event.button_down.up)
                dir = Dpad::Direction::up;
            if (psx->event.button_down.down)
                dir = Dpad::Direction::down;
            if (psx->event.button_down.left)
                dir = Dpad::Direction::left;
            if (psx->event.button_down.right)
                dir = Dpad::Direction::right;
            if (dir != lastData.dpaddirection)
            {
                lastData.dpaddirection = dir;
                if (dpad_changed_event != nullptr)
                    dpad_changed_event(dir);
            }

            if (xyza_changed_event != nullptr)
                xyza_changed_event(psx->state.analog.stick.ly, psx->state.analog.stick.rx, psx->state.analog.stick.ry, psx->state.analog.stick.lx);
            lastData.LeftX = psx->state.analog.stick.ly;
            lastData.RightX = psx->state.analog.stick.rx;
            lastData.RightY = psx->state.analog.stick.ry;
            lastData.LeftY = psx->state.analog.stick.lx;

            left = psx->event.button_down.left;
            right = psx->event.button_down.right;
            r1 = psx->event.button_down.r1;
            r2 = psx->event.button_down.r2;
            l1 = psx->event.button_down.l1;
            l2 = psx->event.button_down.l2;
            if (analogcontroller_event != nullptr)
                analogcontroller_event(left, right, r1, r2, l1, l2);
        }
    }
#endif

    cJSON *scanForDevices(cJSON *doc)
    {
        log_i("scanForDevices - this does not work on all ESP32 even if same fabricate and same version");
        log_i("scanForDevices - Also ensure that we have enough Heap memory. 39300 seems to be the minimum below which it doesn't work anymore");
// scan for bluetooth devices and return the list of devices
#ifdef BTHID
        hid_demo_task(nullptr);
#endif
        return NULL;
    }

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

    cJSON *getPairedDevices(cJSON *doc)
    {
        cJSON *root = cJSON_CreateObject();
#ifdef BTHID
        int count = esp_bt_gap_get_bond_device_num();
        if (!count)
        {
            log_i("No bonded device found.");
            cJSON *item = cJSON_CreateObject();
            cJSON_AddItemToArray(root, item);
            cJSON_AddStringToObject(item, "name", "No bonded device found");
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
                    cJSON *item = cJSON_CreateObject();
                    cJSON_AddItemToArray(root, item);
                    cJSON_AddStringToObject(item, "name", "");
                    cJSON_AddStringToObject(item, "mac", bda2str(pairedDeviceBtAddr[i], bda_str, 18));
                }
            }
        }
#endif
        return doc;
    }

    void setMacAndConnect(char *m)
    {
    }

#ifdef PSXCONTROLLER
    void connectPsxController(char *mac, int type)
    {
        log_i("start psx advertising with mac: %s type:%i", mac, type);
        Config::setPsxMac(mac);
        Config::setPsxControllerType(type);
        setupPS(mac, type);
    }
#endif

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
        return false;
    }

    void removePairedDevice(char *pairedmac)
    {
#ifdef BTHID
        esp_err_t tError = esp_bt_gap_remove_bond_device((uint8_t *)pairedmac);

        log_i("paired device removed:%s", tError == ESP_OK);
#endif
    }

    int8_t sgn(int val)
    {
        if (val < 0)
            return -1;
        if (val == 0)
            return 0;
        return 1;
    }

#include "esp_bt.h"
#include "esp_bt_device.h"

    void removeAllPairedDevices()
    {
#ifdef BTHID
        esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        esp_bt_controller_init(&bt_cfg);
        esp_bt_controller_enable(ESP_BT_MODE_BTDM);

        // Get the maximum number of paired devices
        int num_devices = esp_bt_gap_get_bond_device_num();

        // Get the list of paired devices
        esp_bd_addr_t *devices = (esp_bd_addr_t *)malloc(num_devices * sizeof(esp_bd_addr_t));
        esp_bt_gap_get_bond_device_list(&num_devices, devices);

        // Delete each paired device
        for (int i = 0; i < num_devices; i++)
        {
            // log_d("REmoving device...");
            esp_bt_gap_remove_bond_device(devices[i]);
        }

        free(devices);

        esp_bt_controller_disable();
        esp_bt_controller_deinit();
#endif
    }
} // namespace name

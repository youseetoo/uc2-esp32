#include "PinConfig.h"
#ifdef BLUETOOTH
#include "BtController.h"
#include <esp_log.h>
#include "../analogout/AnalogOutController.h"
#include "../laser/LaserController.h"
#include "../led/LedController.h"
#include "../motor/FocusMotor.h"
#include "../config/ConfigController.h"
#ifdef BTHID
#include "HidController.h"
#endif

#include "Arduino.h"
#include "JsonKeys.h"

namespace BtController
{

    void btControllerLoop(void *p)
    {
        for (;;)
        {
            loop();
            vTaskDelay(5 / portTICK_PERIOD_MS);
        }
    }

    void setup()
    {

        log_d("Setup bluetooth controller");
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
                log_d("Using MAC address");
                log_d(pinConfig.PSX_MAC);
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
        // xTaskCreate(&btControllerLoop, "btController_task", 4092, NULL, 5, NULL);
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

    void handelAxis(int value, int s)
    {
#ifdef FOCUS_MOTOR
        if (value >= offset_val || value <= -offset_val)
        {
            // move_x
            FocusMotor::data[s]->speed = value;
            FocusMotor::data[s]->isforever = true;
            FocusMotor::startStepper(s);
            if (s == Stepper::X)
                joystick_drive_X = true;
            if (s == Stepper::Y)
                joystick_drive_Y = true;
            if (s == Stepper::Z)
                joystick_drive_Z = true;
            if (s == Stepper::A)
                joystick_drive_A = true;
        }
        else if (joystick_drive_X)
        {
            FocusMotor::stopStepper(s);
            if (s == Stepper::X)
                joystick_drive_X = false;
            if (s == Stepper::Y)
                joystick_drive_Y = false;
            if (s == Stepper::Z)
                joystick_drive_Z = false;
            if (s == Stepper::A)
                joystick_drive_A = false;
        }
#endif
    }

    void loop()
    {
        // this maps the physical inputs (e.g. joystick) to the physical outputs (e.g. motor)
        #ifdef BTHID
        if (hidIsConnected)
        #endif
        #ifdef PSXCONTROLLER
        if ((psx != nullptr && psx->isConnected()))
        #endif
        {
#ifdef LED_CONTROLLER
            // switch LED on/off on cross/circle button press
            bool cross = false;
            bool circle = false;
            #ifdef PSXCONTROLLER
            if (psx != nullptr)
            {
                cross = psx->event.button_down.cross;
                circle = psx->event.button_down.circle;
            }
            #endif
            #ifdef BTHID 
            if (hidIsConnected)
            {
                cross = gamePadData.cross;
                circle = gamePadData.circle;
            }
            #endif
            if (cross && !led_on)
            {
                log_i("Turn on LED ");
                IS_PS_CONTROLER_LEDARRAY = !LedController::TurnedOn();
                LedController::set_all(pinConfig.JOYSTICK_MAX_ILLU, pinConfig.JOYSTICK_MAX_ILLU, pinConfig.JOYSTICK_MAX_ILLU);
                led_on = true;
            }
            if (circle && led_on)
            {
                log_i("Turn off LED ");
                IS_PS_CONTROLER_LEDARRAY = !LedController::TurnedOn();
                LedController::set_all(0, 0, 0);
                led_on = false;
            }
#endif
            /* code */

#ifdef LASER_CONTROLLER
            bool up = false;
            bool down = false;
            bool right = false;
            bool left = false;
            #ifdef PSXCONTROLLER
            if (psx != nullptr)
            {
                up = psx->event.button_down.up;
                down = psx->event.button_down.down;
                right = psx->event.button_down.right;
                left = psx->event.button_down.left;
            }
            #endif
            #ifdef BTHID 
            if (hidIsConnected)
            {
                up = gamePadData.dpaddirection == Dpad::Direction::up;
                down = gamePadData.dpaddirection == Dpad::Direction::down;
                left = gamePadData.dpaddirection == Dpad::Direction::left;
                right = gamePadData.dpaddirection == Dpad::Direction::right;
            }
            #endif
            // LASER 1
            // switch laser 1 on/off on triangle/square button press
            if (up && !laser_on)
            {
                // Switch laser 2 on/off on up/down button press
                log_d("Turning on LAser 10000");
                ledcWrite(LaserController::PWM_CHANNEL_LASER_2, 20000);
                laser_on = true;
            }
            if (down && laser_on)
            {
                log_d("Turning off LAser ");
                ledcWrite(LaserController::PWM_CHANNEL_LASER_2, 0);
                laser_on = false;
            }

            // LASER 2
            // switch laser 2 on/off on triangle/square button press
            if (right && !laser2_on)
            {
                log_d("Turning on LAser 10000");
                ledcWrite(LaserController::PWM_CHANNEL_LASER_1, 20000);
                laser2_on = true;
            }
            if (left && laser2_on)
            {
                log_d("Turning off LAser ");
                ledcWrite(LaserController::PWM_CHANNEL_LASER_1, 0);
                laser2_on = false;
            }
#endif

            // MOTORS
#ifdef FOCUS_MOTOR
            /* code */
            int zvalue = 0;
            int xvalue = 0;
            int yvalue = 0;
            int avalue = 0;
            #ifdef PSXCONTROLLER
            if (psx != nullptr)
            {
                zvalue = psx->state.analog.stick.ly;
                xvalue = psx->state.analog.stick.rx;
                yvalue = psx->state.analog.stick.ry;
                avalue = psx->state.analog.stick.lx;
            }
            #endif
            #ifdef BTHID 
            if (hidIsConnected)
            {
                zvalue = gamePadData.LeftY;
                xvalue = gamePadData.RightX;
                yvalue = gamePadData.RightY;
                avalue = gamePadData.LeftX;
            }
            #endif
            if (logCounter > 100)
            {
                log_i("X:%d y:%d, z:%d, a:%d", xvalue, yvalue, zvalue, avalue);
                logCounter = 0;
            }
            else
                logCounter++;

            // Z-Direction
            handelAxis(zvalue, Stepper::Z);

            // X-Direction
            handelAxis(xvalue, Stepper::X);

            // Y-direction
            handelAxis(yvalue, Stepper::Y);

            // A-direction
            handelAxis(avalue, Stepper::A);
#endif

#ifdef ANALOG_OUT_CONTROLLER
            left = false;
            right = false;
            bool r1 = false;
            bool r2 = false;
            bool l1 = false;
            bool l2 = false;
            #ifdef PSXCONTROLLER
            if (psx != nullptr)
            {
                left = psx->event.button_down.left;
                right = psx->event.button_down.right;
                r1 = psx->event.button_down.r1;
                r2 = psx->event.button_down.r2;
                l1 = psx->event.button_down.l1;
                l2 = psx->event.button_down.l2;
            }
            #endif
            #ifdef BTHID 
            if (hidIsConnected)
            {
                left = gamePadData.dpaddirection == Dpad::Direction::left;
                right = gamePadData.dpaddirection == Dpad::Direction::right;
                r1 = gamePadData.r1;
                r2 = gamePadData.r2;
                l1 = gamePadData.l1;
                l2 = gamePadData.l2;
            }
            #endif
            /*
               Keypad left
            */
            if (left)
            {
                // fine lens -
                analogout_val_1 -= 1;
                delay(50);
                ledcWrite(AnalogOutController::PWM_CHANNEL_analogout_1, analogout_val_1);
            }
            if (right)
            {
                // fine lens +
                analogout_val_1 += 1;
                delay(50);
                ledcWrite(AnalogOutController::PWM_CHANNEL_analogout_1, analogout_val_1);
            }
            // unknown button
            /*if (PS4.data.button.start) {
              // reset
              analogout_val_1 = 0;
              ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
            }*/
            if (r2)
            {
                // analogout_val_1++ coarse
                if ((analogout_val_1 + 1000 < pwm_max))
                {
                    analogout_val_1 += 1000;
                    ledcWrite(AnalogOutController::PWM_CHANNEL_analogout_1, analogout_val_1);
                }
            }
            if (l2)
            {
                // analogout_val_1-- coarse
                if ((analogout_val_1 - 1000 > 0))
                {
                    analogout_val_1 -= 1000;
                    ledcWrite(AnalogOutController::PWM_CHANNEL_analogout_1, analogout_val_1);
                }
            }

            if (l1)
            {
                // analogout_val_1 + semi coarse
                if ((analogout_val_1 + 100 < pwm_max))
                {
                    analogout_val_1 += 100;
                    ledcWrite(AnalogOutController::PWM_CHANNEL_analogout_1, analogout_val_1);
                    // delay(100);
                }
            }
            if (r1)
            {
                // analogout_val_1 - semi coarse
                if ((analogout_val_1 - 100 > 0))
                {
                    analogout_val_1 -= 100;
                    ledcWrite(AnalogOutController::PWM_CHANNEL_analogout_1, analogout_val_1);
                    // delay(50);
                }
            }
#endif
        }
    }

    cJSON *scanForDevices(cJSON *doc)
    {
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
            log_d("REmoving device...");
            esp_bt_gap_remove_bond_device(devices[i]);
        }

        free(devices);

        esp_bt_controller_disable();
        esp_bt_controller_deinit();
        #endif
    }
} // namespace name
#endif

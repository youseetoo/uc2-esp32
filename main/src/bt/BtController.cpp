#include "BtController.h"
#include "PinConfig.h"
#include <esp_log.h>
#include "../dac/DacController.h"
#include "../analogin/AnalogInController.h"
#include "../analogout/AnalogOutController.h"
#include "../digitalout/DigitalOutController.h"
#include "../digitalin/DigitalInController.h"
#include "../laser/LaserController.h"
#include "../wifi/RestApiCallbacks.h"
#include "../led/LedController.h"
#include "../motor/FocusMotor.h"

void btControllerLoop(void *p)
{
    for (;;)
    {
        moduleController.get(AvailableModules::btcontroller)->loop();
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}
BtController::BtController() { log_i("ctor"); }
BtController::~BtController() { log_i("~ctor"); }

int BtController::act(cJSON *doc)
{
    return 0;
}

cJSON *BtController::get(cJSON *doc)
{
    return doc;
}

void BtController::setup()
{

    log_d("Setup bluetooth controller");
    // get the bluetooth config
    if (!pinConfig.useBtHID)
    {
        char* m = Config::getPsxMac();
        int type = Config::getPsxControllerType();

        if (m != NULL && pinConfig.PSX_MAC != NULL)
        {

            // always remove all the devices?
            removeAllPairedDevices();
            m = (char*)pinConfig.PSX_MAC;
            type = pinConfig.PSX_CONTROLLER_TYPE;
            log_d("Using MAC address %c", pinConfig.PSX_MAC);
        }

        // if the mac is not empty, try to connect to the psx controller
        if (m != NULL)
        {
            // initiate either PS3 or PS4 controller
            log_i("Connecting to PSX controller");
            setupPS(m, type);
        }
    }
    else
        setupHidController();
    // xTaskCreate(&btControllerLoop, "btController_task", 4092, NULL, 5, NULL);
}

void BtController::setupPS(char *mac, int type)
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

void BtController::handelAxis(int value, int s)
{
    FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
    if (value >= offset_val || value <= -offset_val)
    {
        // move_x
        motor->data[s]->speed = value;
        motor->data[s]->isforever = true;
        motor->startStepper(s);
        if (s == Stepper::X)
            joystick_drive_X = true;
        if (s == Stepper::Y)
            joystick_drive_Y = true;
        if (s == Stepper::Z)
            joystick_drive_Z = true;
        if (s == Stepper::A)
            joystick_drive_A = true;
    }
    else if (joystick_drive_X || joystick_drive_Y || joystick_drive_Z || joystick_drive_A)
    {
        motor->stopStepper(s);
        if (s == Stepper::X)
            joystick_drive_X = false;
        if (s == Stepper::Y)
            joystick_drive_Y = false;
        if (s == Stepper::Z)
            joystick_drive_Z = false;
        if (s == Stepper::A)
            joystick_drive_A = false;
    }
}

void BtController::loop()
{
    // this maps the physical inputs (e.g. joystick) to the physical outputs (e.g. motor)
    if ((psx != nullptr && psx->isConnected()) || hidIsConnected)
    {
        if (moduleController.get(AvailableModules::led) != nullptr)
        {
            // switch LED on/off on cross/circle button press
            LedController *led = (LedController *)moduleController.get(AvailableModules::led);
            bool cross = false;
            bool circle = false;
            if (psx != nullptr)
            {
                cross = psx->event.button_down.cross;
                circle = psx->event.button_down.circle;
            }
            else if (hidIsConnected)
            {
                cross = gamePadData.cross;
                circle = gamePadData.circle;
            }
            if (cross && !led_on)
            {
                log_i("Turn on LED ");
                IS_PS_CONTROLER_LEDARRAY = !led->TurnedOn();
                led->set_all(pinConfig.JOYSTICK_MAX_ILLU, pinConfig.JOYSTICK_MAX_ILLU, pinConfig.JOYSTICK_MAX_ILLU);
                led_on = true;
            }
            if (circle && led_on)
            {
                log_i("Turn off LED ");
                IS_PS_CONTROLER_LEDARRAY = !led->TurnedOn();
                led->set_all(0, 0, 0);
                led_on = false;
            }
        }
        /* code */

        if (moduleController.get(AvailableModules::laser) != nullptr)
        {
            bool up = false;
            bool down = false;
            bool right = false;
            bool left = false;
            LaserController *laser = (LaserController *)moduleController.get(AvailableModules::laser);
            if (psx != nullptr)
            {
                up = psx->event.button_down.up;
                down = psx->event.button_down.down;
                right = psx->event.button_down.right;
                left = psx->event.button_down.left;
            }
            else if (hidIsConnected)
            {
                up = gamePadData.dpaddirection == Dpad::Direction::up;
                down = gamePadData.dpaddirection == Dpad::Direction::down;
                left = gamePadData.dpaddirection == Dpad::Direction::left;
                right = gamePadData.dpaddirection == Dpad::Direction::right;
            }
            // LASER 1
            // switch laser 1 on/off on triangle/square button press
            if (up && !laser_on)
            {
                // Switch laser 2 on/off on up/down button press
                log_d("Turning on LAser 10000");
                ledcWrite(laser->PWM_CHANNEL_LASER_2, 20000);
                laser_on = true;
            }
            if (down && laser_on)
            {
                log_d("Turning off LAser ");
                ledcWrite(laser->PWM_CHANNEL_LASER_2, 0);
                laser_on = false;
            }

            // LASER 2
            // switch laser 2 on/off on triangle/square button press
            if (right && !laser2_on)
            {
                log_d("Turning on LAser 10000");
                ledcWrite(laser->PWM_CHANNEL_LASER_1, 20000);
                laser2_on = true;
            }
            if (left && laser2_on)
            {
                log_d("Turning off LAser ");
                ledcWrite(laser->PWM_CHANNEL_LASER_1, 0);
                laser2_on = false;
            }
        }

        // MOTORS
        if (moduleController.get(AvailableModules::motor) != nullptr)
        {
            /* code */
            FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
            int zvalue = 0;
            int xvalue = 0;
            int yvalue = 0;
            int avalue = 0;
            if (psx != nullptr)
            {
                zvalue = psx->state.analog.stick.ly;
                xvalue = psx->state.analog.stick.rx;
                yvalue = psx->state.analog.stick.ry;
                avalue = psx->state.analog.stick.lx;
            }
            else if (hidIsConnected)
            {
                zvalue = gamePadData.LeftY;
                xvalue = gamePadData.RightX;
                yvalue = gamePadData.RightY;
                avalue = gamePadData.LeftX;
            }
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
        }

        if (moduleController.get(AvailableModules::analogout) != nullptr)
        {
            AnalogOutController *analogout = (AnalogOutController *)moduleController.get(AvailableModules::analogout);
            bool left = false;
            bool right = false;
            bool r1 = false;
            bool r2 = false;
            bool l1 = false;
            bool l2 = false;
            if (psx != nullptr)
            {
                left = psx->event.button_down.left;
                right = psx->event.button_down.right;
                r1 = psx->event.button_down.r1;
                r2 = psx->event.button_down.r2;
                l1 = psx->event.button_down.l1;
                l2 = psx->event.button_down.l2;
            }
            else if (hidIsConnected)
            {
                left = gamePadData.dpaddirection == Dpad::Direction::left;
                right = gamePadData.dpaddirection == Dpad::Direction::right;
                r1 = gamePadData.r1;
                r2 = gamePadData.r2;
                l1 = gamePadData.l1;
                l2 = gamePadData.l2;
            }
            /*
               Keypad left
            */
            if (left)
            {
                // fine lens -
                analogout_val_1 -= 1;
                delay(50);
                ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
            }
            if (right)
            {
                // fine lens +
                analogout_val_1 += 1;
                delay(50);
                ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
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
                    ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
                }
            }
            if (l2)
            {
                // analogout_val_1-- coarse
                if ((analogout_val_1 - 1000 > 0))
                {
                    analogout_val_1 -= 1000;
                    ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
                }
            }

            if (l1)
            {
                // analogout_val_1 + semi coarse
                if ((analogout_val_1 + 100 < pwm_max))
                {
                    analogout_val_1 += 100;
                    ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
                    // delay(100);
                }
            }
            if (r1)
            {
                // analogout_val_1 - semi coarse
                if ((analogout_val_1 - 100 > 0))
                {
                    analogout_val_1 -= 100;
                    ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
                    // delay(50);
                }
            }
        }
    }
}

cJSON *BtController::scanForDevices(cJSON *doc)
{
    log_i("scanForDevices - this does not work on all ESP32 even if same fabricate and same version");
    // scan for bluetooth devices and return the list of devices
    hid_demo_task(nullptr);
    return NULL;
}

char *BtController::bda2str(const uint8_t *bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18)
    {
        return NULL;
    }
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    return str;
}

cJSON *BtController::getPairedDevices(cJSON *doc)
{
    cJSON *root = cJSON_CreateObject();
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
    return doc;
}

void BtController::setMacAndConnect(char* m)
{
}

void BtController::connectPsxController(char* mac, int type)
{
    log_i("start psx advertising with mac: %s type:%i", mac, type);
    Config::setPsxMac(mac);
    Config::setPsxControllerType(type);
    setupPS(mac, type);
}

bool BtController::connectToServer()
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

void BtController::removePairedDevice(char* pairedmac)
{
    esp_err_t tError = esp_bt_gap_remove_bond_device((uint8_t *)pairedmac);
    log_i("paired device removed:%s", tError == ESP_OK);
}

int8_t BtController::sgn(int val)
{
    if (val < 0)
        return -1;
    if (val == 0)
        return 0;
    return 1;
}

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

#include "esp_bt.h"
#include "esp_bt_device.h"

void BtController::removeAllPairedDevices()
{

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
}

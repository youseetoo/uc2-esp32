#include "BtController.h"

namespace RestApi
{
    void Bt_startScan()
    {
        BtController *bt = (BtController *)moduleController.get(AvailableModules::btcontroller);
        serialize(bt->scanForDevices(deserialize()));
    }

    void Bt_connect()
    {
        BtController *bt = (BtController *)moduleController.get(AvailableModules::btcontroller);
        DynamicJsonDocument doc = deserialize();
        String mac = doc["mac"];
        int ps = doc["psx"];

        if (ps == 0)
        {
            bt->setMacAndConnect(mac);
        }
        else
        {
            bt->connectPsxController(mac, ps);
        }

        doc.clear();
        serialize(doc);
    }

    void Bt_getPairedDevices()
    {
        BtController *bt = (BtController *)moduleController.get(AvailableModules::btcontroller);
        serialize(bt->getPairedDevices(deserialize()));
    }

    void Bt_remove()
    {
        BtController *bt = (BtController *)moduleController.get(AvailableModules::btcontroller);
        DynamicJsonDocument doc = deserialize();
        String mac = doc["mac"];
        bt->removePairedDevice(mac);
        doc.clear();
        serialize(doc);
    }
}

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

int BtController::act(DynamicJsonDocument doc)
{
    return 0;
}

DynamicJsonDocument BtController::get(DynamicJsonDocument doc)
{
    return doc;
}

void BtController::setup()
{

    log_d("Setup bluetooth controller");
    // setting up the bluetooth controller

    // BLEDevice::setCustomGapHandler(my_gap_event_handler);
    // BLEDevice::setCustomGattsHandler(my_gatts_event_handler);
    // BLEDevice::setCustomGattcHandler(my_gattc_event_handler);
    // BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);

    // get the bluetooth config
    String m = Config::getPsxMac();
    int type = Config::getPsxControllerType();

    if (m.isEmpty() && !pinConfig.PSX_MAC.isEmpty())
    {

        // always remove all the devices?
        removeAllPairedDevices();
        m = pinConfig.PSX_MAC;
        type = pinConfig.PSX_CONTROLLER_TYPE;
        log_d("Using MAC address");
        Serial.println(pinConfig.PSX_MAC);
    }

    // if the mac is not empty, try to connect to the psx controller
    if (!m.isEmpty())
    {
        // initiate either PS3 or PS4 controller
        log_i("Connecting to PSX controller");
        setupPS(m, type);
    }
    //xTaskCreate(&btControllerLoop, "btController_task", 4092, NULL, 5, NULL);
}

void BtController::setupPS(String mac, int type)
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
            if(psx != nullptr)
            {
                cross = psx->event.button_down.cross;
                circle = psx->event.button_down.circle;
            }
            else if(hidIsConnected){
                cross = gamePadData.cross;
                circle = gamePadData.circle;
            }
            if (cross)
            {
                log_i("Turn on LED ");
                IS_PS_CONTROLER_LEDARRAY = !led->TurnedOn();
                led->set_all(pinConfig.JOYSTICK_MAX_ILLU, pinConfig.JOYSTICK_MAX_ILLU, pinConfig.JOYSTICK_MAX_ILLU);
            }
            if (circle)
            {
                log_i("Turn off LED ");
                IS_PS_CONTROLER_LEDARRAY = !led->TurnedOn();
                led->set_all(0, 0, 0);
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
            if(psx != nullptr)
            {
                up = psx->event.button_down.up;
                down = psx->event.button_down.down;
                right = psx->event.button_down.right;
                left = psx->event.button_down.left;
            }
            else if(hidIsConnected)
            {
                up = gamePadData.dpaddirection == Dpad::Direction::up;
                down = gamePadData.dpaddirection == Dpad::Direction::down;
                left = gamePadData.dpaddirection == Dpad::Direction::left;
                right = gamePadData.dpaddirection == Dpad::Direction::right;
            }
            // LASER 1
            // switch laser 1 on/off on triangle/square button press
            if (up){
                // Switch laser 2 on/off on up/down button press
                Serial.print("Turning on LAser 10000");
                ledcWrite(laser->PWM_CHANNEL_LASER_2, 20000);
            }
            if (down)
            {
                Serial.print("Turning off LAser ");
                ledcWrite(laser->PWM_CHANNEL_LASER_2, 0);
            }

            // LASER 2
            // switch laser 2 on/off on triangle/square button press
            if (right)
            {
                Serial.print("Turning on LAser 10000");
                ledcWrite(laser->PWM_CHANNEL_LASER_1, 20000);
            }
            if (left)
            {
                Serial.print("Turning off LAser ");
                ledcWrite(laser->PWM_CHANNEL_LASER_1, 0);
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
            if(psx != nullptr)
            {
                zvalue = psx->state.analog.stick.ly;
                xvalue = psx->state.analog.stick.rx;
                yvalue = psx->state.analog.stick.ry;
                avalue = psx->state.analog.stick.lx;
            }
            else if(hidIsConnected)
            {
                zvalue = gamePadData.LeftY;
                xvalue = gamePadData.RightX;
                yvalue = gamePadData.RightY;
                avalue = gamePadData.LeftX;
            }
            if (logCounter > 300)
            {
                log_i("X:%d", xvalue);
                logCounter =0;
            }
            else
                logCounter++;
            
            // Z-Direction
            if (abs(zvalue) > offset_val)
            {
                stick_ly = zvalue;
                stick_ly = stick_ly - sgn(stick_ly) * offset_val;
                if (abs(stick_ly) > 50)
                    stick_ly = 2 * stick_ly; // add more speed above threshold
                motor->data[Stepper::Z]->speed = 0.1 * pinConfig.JOYSTICK_SPEED_MULTIPLIER_Z * stick_ly;
                motor->data[Stepper::Z]->isforever = true;
                joystick_drive_Z = true;
                //motor->faststeppers[Stepper::Z]->enableOutputs();
                //motor->faststeppers[Stepper::Z]->setAutoEnable(false);
                if (motor->data[Stepper::Z]->stopped)
                {
                    motor->startStepper(Stepper::Z);
                }
            }
            else if (motor->data[Stepper::Z]->speed != 0 && joystick_drive_Z)
            {
                motor->stopStepper(Stepper::Z);
                joystick_drive_Z = false;
            }
            
            
            // X-Direction
            if (xvalue >= offset_val || xvalue <= -offset_val)
            {
                // move_x
                motor->data[Stepper::X]->speed = xvalue;
                motor->data[Stepper::X]->isforever = true;
                
                if (motor->data[Stepper::X]->stopped && !joystick_drive_X)
                {
                    motor->startStepper(Stepper::X);
                    joystick_drive_X = true;
                }
            }
            else if (motor->data[Stepper::X]->speed != 0 && joystick_drive_X)
            {
                motor->stopStepper(Stepper::X);
                joystick_drive_X = false;
            }

            // Y-direction
            if (abs(yvalue) > offset_val)
            {
                stick_ry = yvalue;
                stick_ry = stick_ry - sgn(stick_ry) * offset_val;
                motor->data[Stepper::Y]->speed = 0.1 * pinConfig.JOYSTICK_SPEED_MULTIPLIER * stick_ry;
                motor->data[Stepper::Y]->isforever = true;
                //motor->faststeppers[Stepper::Y]->enableOutputs();
                //motor->faststeppers[Stepper::Y]->setAutoEnable(false);

                if (abs(stick_ry) > 50)
                    stick_ry = 2 * stick_ry; // add more speed above threshold
                joystick_drive_Y = true;
                if (motor->data[Stepper::Y]->stopped)
                {
                    motor->startStepper(Stepper::Y);
                }
            }
            else if (motor->data[Stepper::Y]->speed != 0 && joystick_drive_Y)
            {
                motor->stopStepper(Stepper::Y);
                joystick_drive_Y = false;
            }

            // A-direction
            if (abs(avalue) > offset_val)
            {
                stick_lx = avalue;
                stick_lx = stick_lx - sgn(stick_lx) * offset_val;
                motor->data[Stepper::A]->speed = 0.1 * pinConfig.JOYSTICK_SPEED_MULTIPLIER * stick_lx;
                motor->data[Stepper::A]->isforever = true;
                //motor->faststeppers[Stepper::A]->enableOutputs();
                //motor->faststeppers[Stepper::A]->setAutoEnable(false);

                if (abs(stick_lx) > 50)
                    stick_lx = 2 * stick_lx; // add more speed above threshold
                joystick_drive_A = true;
                if (motor->data[Stepper::A]->stopped)
                {
                    motor->startStepper(Stepper::A);
                }
            }
            else if (motor->data[Stepper::A]->speed != 0 && joystick_drive_A)
            {
                motor->stopStepper(Stepper::A);
                joystick_drive_A = false;
            }
        }

        if (moduleController.get(AvailableModules::analogout) != nullptr)
        {
            AnalogOutController *analogout = (AnalogOutController *)moduleController.get(AvailableModules::analogout);
            /*
               Keypad left
            */
            if (psx->event.button_down.left || gamePadData.dpaddirection == Dpad::Direction::left)
            {
                // fine lens -
                analogout_val_1 -= 1;
                delay(50);
                ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
            }
            if (psx->event.button_down.right  || gamePadData.dpaddirection == Dpad::Direction::right)
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

            int offset_val_shoulder = 5;
            if (abs(psx->event.button_down.r2) > offset_val_shoulder)
            {
                // analogout_val_1++ coarse
                if ((analogout_val_1 + 1000 < pwm_max))
                {
                    analogout_val_1 += 1000;
                    ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
                }
                // Serial.println(analogout_val_1);
                // delay(100);
            }

            if (abs(psx->event.button_down.l2) > offset_val_shoulder)
            {
                // analogout_val_1-- coarse
                if ((analogout_val_1 - 1000 > 0))
                {
                    analogout_val_1 -= 1000;
                    ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
                }
                // Serial.println(analogout_val_1);
                // delay(100);
            }

            if (abs(psx->event.button_down.l1) > offset_val_shoulder)
            {
                // analogout_val_1 + semi coarse
                if ((analogout_val_1 + 100 < pwm_max))
                {
                    analogout_val_1 += 100;
                    ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
                    // delay(100);
                }
            }
            if (abs(psx->event.button_down.r1) > offset_val_shoulder)
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

DynamicJsonDocument BtController::scanForDevices(DynamicJsonDocument doc)
{
    // scan for bluetooth devices and return the list of devices
    setupHidController();
    return doc;
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

DynamicJsonDocument BtController::getPairedDevices(DynamicJsonDocument doc)
{
    doc.clear();
    int count = esp_bt_gap_get_bond_device_num();
    if (!count)
    {
        log_i("No bonded device found.");
        JsonObject ob = doc.createNestedObject();
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
                JsonObject ob = doc.createNestedObject();
                ob["name"] = "";
                ob["mac"] = bda2str(pairedDeviceBtAddr[i], bda_str, 18);
            }
        }
    }
    return doc;
}

void BtController::setMacAndConnect(String m)
{
}

void BtController::connectPsxController(String mac, int type)
{
    log_i("start psx advertising with mac: %s type:%i", mac.c_str(), type);
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

void BtController::removePairedDevice(String pairedmac)
{
    esp_err_t tError = esp_bt_gap_remove_bond_device((uint8_t *)pairedmac.c_str());
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
        Serial.println("REmoving device...");
        esp_bt_gap_remove_bond_device(devices[i]);
    }

    free(devices);

    esp_bt_controller_disable();
    esp_bt_controller_deinit();

}

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
    }

    // if the mac is not empty, try to connect to the psx controller
    if (!m.isEmpty())
    {
        // initiate either PS3 or PS4 controller
        log_i("Connecting to PSX controller");
        setupPS(m, type);
    }
    xTaskCreate(&btControllerLoop, "btController_task", 2048, NULL, 5, NULL);
}


void BtController::setupPS(String mac, int type)
{
	// select the type of wifi controller - either PS3 or PS4
	if(type == 1)
		psx = new PSController(nullptr, PSController::kPS3);
	else if(type == 2)
		psx = new PSController(nullptr, PSController::kPS4);
    psx->startListening(mac);
}

void BtController::loop()
{
    // this maps the physical inputs (e.g. joystick) to the physical outputs (e.g. motor)
    if (psx != nullptr && psx->isConnected())
    {
        if (moduleController.get(AvailableModules::led) != nullptr)
        {
			// switch LED on/off on cross/circle button press
            LedController *led = (LedController *)moduleController.get(AvailableModules::led);
            if (psx->event.button_down.cross)
            {
				log_i("Turn on LED ");
                IS_PS_CONTROLER_LEDARRAY = !led->TurnedOn();
                led->set_all(255, 255, 255);
            }
            if (psx->event.button_down.circle)
            {
				log_i("Turn off LED ");
                IS_PS_CONTROLER_LEDARRAY = !led->TurnedOn();
                led->set_all(0,0,0);
            }
        }
        /* code */

        if (moduleController.get(AvailableModules::laser) != nullptr)
        {

            LaserController *laser = (LaserController *)moduleController.get(AvailableModules::laser);

            // LASER 1
			// switch laser 1 on/off on triangle/square button press
            if (psx->event.button_down.up)
            {
				// Switch laser 2 on/off on up/down button press
                Serial.print("Turning on LAser 10000");
                ledcWrite(laser->PWM_CHANNEL_LASER_2, 20000);
            }
            if (psx->event.button_down.down)
            {
                Serial.print("Turning off LAser ");
                ledcWrite(laser->PWM_CHANNEL_LASER_2, 0);
            }

            // LASER 2
			// switch laser 2 on/off on triangle/square button press
            if (psx->event.button_down.right)
            {
                Serial.print("Turning on LAser 10000");
                ledcWrite(laser->PWM_CHANNEL_LASER_1, 20000);
            }
            if (psx->event.button_down.left)
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
			// Z-Direction
			if (abs(psx->state.analog.stick.ly) > offset_val)
			{
				stick_ly = psx->state.analog.stick.ly;
				stick_ly = stick_ly - sgn(stick_ly) * offset_val;
				if(abs(stick_ly)>50)stick_ly=2*stick_ly; // add more speed above threshold
				motor->data[Stepper::Z]->speed = stick_ly * global_speed;
				//Serial.println(motor->data[Stepper::Z]->speed);
				motor->data[Stepper::Z]->isforever = true;
				joystick_drive_Z = true;
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
			if ((abs(psx->state.analog.stick.rx) > offset_val))
			{
				// move_x
				stick_rx = psx->state.analog.stick.rx;
				stick_rx = stick_rx - sgn(stick_rx) * offset_val;
				motor->data[Stepper::X]->speed = stick_rx * global_speed;
				if(abs(stick_rx)>50)stick_rx=2*stick_rx; // add more speed above threshold
				motor->data[Stepper::X]->isforever = true;
				joystick_drive_X = true;
				if (motor->data[Stepper::X]->stopped)
                {
                    motor->startStepper(Stepper::X);
                }
			}
			else if (motor->data[Stepper::X]->speed != 0 && joystick_drive_X)
			{
				motor->stopStepper(Stepper::X);
				joystick_drive_X = false;
			}

			// Y-direction
			if ((abs(psx->state.analog.stick.ry) > offset_val))
			{
				stick_ry = psx->state.analog.stick.ry;
				stick_ry = stick_ry - sgn(stick_ry) * offset_val;
				motor->data[Stepper::Y]->speed = stick_ry * global_speed;
				if(abs(stick_ry)>50)stick_ry=2*stick_ry; // add more speed above threshold
				motor->data[Stepper::Y]->isforever = true;
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
		}

        if (moduleController.get(AvailableModules::analogout) != nullptr)
		{
			AnalogOutController *analogout = (AnalogOutController *)moduleController.get(AvailableModules::analogout);
			/*
			   Keypad left
			*/
			if (psx->event.button_down.left)
			{
				// fine lens -
				analogout_val_1 -= 1;
				delay(50);
				ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
			}
			if (psx->event.button_down.right)
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
                //Serial.println(analogout_val_1);
				//delay(100);
			}

			if (abs(psx->event.button_down.l2) > offset_val_shoulder)
			{
				// analogout_val_1-- coarse
				if ((analogout_val_1 - 1000 > 0))
				{
					analogout_val_1 -= 1000;
					ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
				}
                //Serial.println(analogout_val_1);
				//delay(100);
			}

			if (abs(psx->event.button_down.l1) > offset_val_shoulder)
			{
				// analogout_val_1 + semi coarse
				if ((analogout_val_1 + 100 < pwm_max))
				{
					analogout_val_1 += 100;
					ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
					//delay(100);
				}
			}
			if (abs(psx->event.button_down.r1) > offset_val_shoulder)
			{
				// analogout_val_1 - semi coarse
				if ((analogout_val_1 - 100 > 0))
				{
					analogout_val_1 -= 100;
					ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
					//delay(50);
				}
			}
		}
    }
}

DynamicJsonDocument BtController::scanForDevices(DynamicJsonDocument doc)
{
    // scan for bluetooth devices and return the list of devices
    btClassic.begin("ESP32-BLE-1");
    log_i("Start scanning BT");
    BTScanResults *foundDevices = btClassic.discover(BT_DISCOVER_TIME);
    doc.clear();
    for (int i = 0; i < foundDevices->getCount(); i++)
    {
        log_i("Device %i %s", i, foundDevices->getDevice(i)->toString().c_str());
        JsonObject ob = doc.createNestedObject();
        ob["name"] = foundDevices->getDevice(i)->getName();
        ob["mac"] = foundDevices->getDevice(i)->getAddress().toString();
    }
    // pBLEScan->clearResults();
    // pBLEScan->stop();
    btClassic.end();
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
    BTAddress *add = new BTAddress(pairedmac.c_str());
    esp_err_t tError = esp_bt_gap_remove_bond_device((uint8_t *)add->getNative());
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

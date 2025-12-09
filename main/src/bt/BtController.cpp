
#include "BtController.h"
#include <esp_log.h>
#include "../config/ConfigController.h"
#ifdef BTHID
#include "HidController.h"
#endif
#include "Arduino.h"
#include "JsonKeys.h"
#include <cmath>


namespace BtController
{
    GamePadData lastData;

    void (*options_changed_event)(int pressed);
    void (*share_changed_event)(int pressed);
    void (*cross_changed_event)(int pressed);
    void (*circle_changed_event)(int pressed);
    void (*triangle_changed_event)(int pressed);
    void (*square_changed_event)(int pressed);
    void (*dpad_changed_event)(Dpad::Direction pressed);
    void (*xyza_changed_event)(int x, int y,int z, int a);
    void (*analogcontroller_event)(int left, int right, bool r1, bool r2, bool l1, bool l2);
    
    // PS4 Trackpad event handlers
    void (*trackpad_swipe_event)(SwipeDirection direction);
    void (*trackpad_touch_event)(TouchData touch1, TouchData touch2);
    
    // Trackpad swipe detection variables
    static TrackpadData lastTrackpadData;
    static TouchData swipeStartTouch;
    static bool swipeInProgress = false;
    static const uint16_t SWIPE_THRESHOLD = 50; // Minimum distance for swipe detection
    static const uint32_t SWIPE_TIMEOUT = 500; // Maximum time for swipe (ms)
    static uint32_t swipeStartTime = 0;


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

    void setShareChangedEvent(void (*share_changed_event1)(int pressed))
    {
        log_i("setShareChangedEvent");
        share_changed_event = share_changed_event1;
    }
    
    void setOptionsChangedEvent(void (*options_changed_event1)(int pressed))
    {
        log_i("setOptionsChangedEvent");
        options_changed_event = options_changed_event1;
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

    void setTrackpadSwipeEvent(void (*trackpad_swipe_event1)(SwipeDirection direction))
    {
        log_i("setTrackpadSwipeEvent");
        trackpad_swipe_event = trackpad_swipe_event1;
    }

    void setTrackpadTouchEvent(void (*trackpad_touch_event1)(TouchData touch1, TouchData touch2))
    {
        log_i("setTrackpadTouchEvent");
        trackpad_touch_event = trackpad_touch_event1;
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

        // initialize update limit 
        lastUpdate = millis();
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

    // Process trackpad data and detect swipes
    void processTrackpadData(TrackpadData& currentData)
    {
        // Call touch event handler if registered
        if (trackpad_touch_event != nullptr)
        {
            trackpad_touch_event(currentData.touch1, currentData.touch2);
        }

        // Swipe detection logic
        if (trackpad_swipe_event != nullptr)
        {
            // Check for new touch (start of potential swipe)
            if (!swipeInProgress && currentData.touch1.isActive && !lastTrackpadData.touch1.isActive)
            {
                swipeStartTouch = currentData.touch1;
                swipeInProgress = true;
                swipeStartTime = millis();
                log_d("Swipe started at (%d, %d)", swipeStartTouch.x, swipeStartTouch.y);
            }
            // Check for end of touch (end of potential swipe)
            else if (swipeInProgress && !currentData.touch1.isActive && lastTrackpadData.touch1.isActive)
            {
                uint32_t swipeDuration = millis() - swipeStartTime;
                
                if (swipeDuration < SWIPE_TIMEOUT)
                {
                    // Calculate swipe distance and direction
                    int16_t deltaX = lastTrackpadData.touch1.x - swipeStartTouch.x;
                    int16_t deltaY = lastTrackpadData.touch1.y - swipeStartTouch.y;
                    uint16_t distance = sqrt(deltaX * deltaX + deltaY * deltaY);
                    
                    if (distance > SWIPE_THRESHOLD)
                    {
                        SwipeDirection direction = SWIPE_NONE;
                        
                        // Determine primary swipe direction
                        if (abs(deltaX) > abs(deltaY))
                        {
                            // Horizontal swipe
                            direction = (deltaX > 0) ? SWIPE_RIGHT : SWIPE_LEFT;
                        }
                        else
                        {
                            // Vertical swipe
                            direction = (deltaY > 0) ? SWIPE_DOWN : SWIPE_UP;
                        }
                        
                        log_i("Swipe detected: direction=%d, distance=%d, duration=%d ms", 
                              direction, distance, swipeDuration);
                        trackpad_swipe_event(direction);
                    }
                }
                
                swipeInProgress = false;
            }
            // Check for swipe timeout
            else if (swipeInProgress && (millis() - swipeStartTime) > SWIPE_TIMEOUT)
            {
                log_d("Swipe timeout");
                swipeInProgress = false;
            }
        }

        // Update last trackpad data
        lastTrackpadData = currentData;
    }

#ifdef BTHID

    void loop()
    {
        //log_i("hid connected:%i", hidIsConnected);
        if (hidIsConnected and (millis()-updateRateMS)>lastUpdate)
        {
            // Check each button for state change and call event handler
            // Only trigger event when state actually changes
            if (lastData.cross != gamePadData.cross)
            {
                lastData.cross = gamePadData.cross;
                if (cross_changed_event != nullptr)
                    cross_changed_event(gamePadData.cross);
            }
            if (lastData.circle != gamePadData.circle)
            {
                lastData.circle = gamePadData.circle;
                if (circle_changed_event != nullptr)
                    circle_changed_event(gamePadData.circle);
            }
            if (lastData.triangle != gamePadData.triangle)
            {
                lastData.triangle = gamePadData.triangle;
                if (triangle_changed_event != nullptr)
                    triangle_changed_event(gamePadData.triangle);
            }
            if (lastData.square != gamePadData.square)
            {
                lastData.square = gamePadData.square;
                if (square_changed_event != nullptr)
                    square_changed_event(gamePadData.square);
            }
            if (lastData.options != gamePadData.options)
            {
                lastData.options = gamePadData.options;
                if (options_changed_event != nullptr)
                    options_changed_event(gamePadData.options);
            }
            if (lastData.share != gamePadData.share)
            {
                lastData.share = gamePadData.share;
                if (share_changed_event != nullptr)
                    share_changed_event(gamePadData.share);
            }
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
            if (xyza_changed_event != nullptr) // x,y,z,a
                xyza_changed_event(gamePadData.RightX, gamePadData.RightY, gamePadData.LeftY, gamePadData.LeftX);
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

            lastUpdate = millis();

        }
        if (millis() - lastUpdate > updateRateMS and !hidIsConnected){
            // ensure all values are zero and we stop motors
            xyza_changed_event(0, 0, 0, 0);
        }

    }
#endif

#ifdef PSXCONTROLLER

    void loop()
    {
        if (psx != nullptr && psx->isConnected())
        {
            // Check each button for state change and call event handler
            // Only trigger event when state actually changes
            if (lastData.cross != psx->event.button_down.cross)
            {
                lastData.cross = psx->event.button_down.cross;
                if (cross_changed_event != nullptr)
                    cross_changed_event(psx->event.button_down.cross);
            }
            if (lastData.circle != psx->event.button_down.circle)
            {
                lastData.circle = psx->event.button_down.circle;
                if (circle_changed_event != nullptr)
                    circle_changed_event(psx->event.button_down.circle);
            }
            if (lastData.triangle != psx->event.button_down.triangle)
            {
                lastData.triangle = psx->event.button_down.triangle;
                if (triangle_changed_event != nullptr)
                    triangle_changed_event(psx->event.button_down.triangle);
            }
            if (lastData.square != psx->event.button_down.square)
            {
                lastData.square = psx->event.button_down.square;
                if (square_changed_event != nullptr)
                    square_changed_event(psx->event.button_down.square);
            }
            if (lastData.options != psx->event.button_down.options)
            {
                lastData.options = psx->event.button_down.options;
                if (options_changed_event != nullptr)
                    options_changed_event(psx->event.button_down.options);
            }
            if (lastData.share != psx->event.button_down.share)
            {
                lastData.share = psx->event.button_down.share;
                if (share_changed_event != nullptr)
                    share_changed_event(psx->event.button_down.share);
            }
            
            Dpad::Direction dir = Dpad::Direction::none;
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

            if (xyza_changed_event != nullptr) // x,y,z,a
                xyza_changed_event(psx->state.analog.stick.rx, psx->state.analog.stick.ry, psx->state.analog.stick.ly, psx->state.analog.stick.lx);
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
        // Create a task for BT scanning to prevent blocking the serial task
        // IMPORTANT: Pass non-null parameter (1) to indicate it's running as a task
        // This prevents vTaskDelete from being skipped and ensures proper task cleanup
        // Stack size 8192 is needed for BT scanning operations
        xTaskCreate(hid_demo_task, "hid_demo_task", 8192, (void*)1, 6, NULL);

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "status", "scan_started");
        cJSON_AddStringToObject(root, "message", "Bluetooth device scan started in background");
        return root;
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

    // disconnect from current controller
    void disconnect()
    {
#ifdef BTHID
        // HID controllers don't have a simple disconnect function
        // Set the connection flag to false to indicate disconnection intent
        hidIsConnected = false;
        log_i("HID controller disconnect requested");
#endif
#ifdef PSXCONTROLLER
        if (psx != nullptr && psx->isConnected())
        {
            delete psx;
            psx = nullptr;
            log_i("PSX controller disconnected");
        }
#endif
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

# Explanation of the Firmware

The UC2-ESP firmware operates a number of different physical devices such as:

```cpp
    bool analogout = false;
    bool dac = false;
    bool digitalout = false;
    bool digitalin = false;
    bool laser = true;
    bool led = true;
    bool config = true;
    bool motor = true;
    bool pid = false;
    bool scanner = false;
    bool analogin = false;
    bool slm = false;
    bool home = false;
    bool state = true;
    bool analogJoystick = false;
````

The general way to interact with the devices is following a REST-API-inspired command structure that makes use of `GET, SET, ACT`, where properties (e.g. pins), can be set, or read out/get. Actions are triggered using an act command. 
The ESP32 communicates through a USB Serial interface or via a Wifi-based HTTP REST API. 

## Registering a device

### Serial

Steps to add a new controller / module:


1. Add the controller's state to the main/ModuleConfig.h e.g.:
```cpp
bool btcontroller = true;
```

2. Add the controller to the main/ModuleController.h e.g.:
```cpp
enum class AvailableModules
{
    btcontroller,
```

3. Register device in the main/ModuleController.cpp, e.g.:
```cpp
    // eventually load the BTController module
    if (moduleConfig->btcontroller)
    {
        modules.insert(std::make_pair(AvailableModules::btcontroller, dynamic_cast<Module *>(new BTController())));
        log_i("add btcontroller");
    }
```

4. Wrap the module interface into the REST API style. E.g. in `BtController.cpp`

```cpp
namespace RestApi
{
    void Bt_startScan()
    {
        serialize(BtController::scanForDevices(deserialize()));
    }

    void Bt_connect()
    {
        DynamicJsonDocument doc = deserialize();
        String mac = doc["mac"];
        int ps = doc["psx"];
       
        if (ps == 0)
        {
            BtController::setMacAndConnect(mac);
        }
        else 
        {
            BtController::connectPsxController(mac,ps);
        }
        
        
        doc.clear();
        serialize(doc);
    }

    void Bt_getPairedDevices()
    {
        serialize(BtController::getPairedDevices(deserialize()));
    }

    void Bt_remove()
    {
        DynamicJsonDocument doc = deserialize();
        String mac = doc["mac"];
        BtController::removePairedDevice(mac);
        doc.clear();
        serialize(doc);
    }
}
```


5. Add the handle to the SerialProcess.cpp

e.g.

```cpp
	/*
	Handle BTController
	*/
		if (task == bt_scan_endpoint) // start for Bluetooth Devices
			serialize(moduleController.get(AvailableModules::btcontroller)->Bt_startScan(jsonDocument));
		if (task == bt_paireddevices_endpoint) // get paired devices
			serialize(moduleController.get(AvailableModules::btcontroller)->Bt_getPairedDevices(jsonDocument));
		if (task == bt_connect_endpoint) // connect to device
			serialize(moduleController.get(AvailableModules::btcontroller)->Bt_connect(jsonDocument));
		if (task == bt_remove_endpoint) // remove paired device
			serialize(moduleController.get(AvailableModules::btcontroller)->Bt_remove(jsonDocument));
}
```


6. Add edpoints to `Endpoints.h` e.g. 

```cpp
static const PROGMEM String bt_scan_endpoint = "/bt_scan";
static const PROGMEM String bt_connect_endpoint = "/bt_connect";
static const PROGMEM String bt_remove_endpoint = "/bt_remove";
static const PROGMEM String bt_paireddevices_endpoint = "/bt_paireddevices";
```
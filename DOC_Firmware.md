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
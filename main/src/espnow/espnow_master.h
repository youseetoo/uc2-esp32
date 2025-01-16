#pragma once
#include "../../cJsonTool.h"
#include "../motor/MotorTypes.h"
#include "../laser/LaserController.h"
#include "../home/HomeMotor.h"
#include "cJsonTool.h"
#include "cJSON.h"
#include <esp_now.h>
#include <WiFi.h>
#include <map>

// Message types
enum MessageType
{
    PAIRING,
    POSITION_REQUEST,
    POSITION_RESPONSE
};

// Pairing data structure
struct PairingMessage
{
    uint8_t msgType;
    uint8_t axis;       // Assigned axis (unique for each slave)
    uint8_t macAddr[6]; // Slave MAC address
    uint8_t channel;    // Channel
};

// Position request and response data structures
struct PositionRequest
{
    uint8_t msgType;
    uint8_t axis; // Axis to query
};

struct PositionResponse
{
    uint8_t msgType;
    uint8_t axis;    // Responding axis
    long position;   // Motor position
    char status[10]; // Status (e.g., "STOPPED")
};



namespace espnow_master
{

    // common functions
    int act(cJSON *doc);
    cJSON *get(cJSON *ob);
    void setup();
    void loop();
    void updateMotorData(int i);

    uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    static unsigned long lastRequestTime = 0;
    // Map axis to MAC address
    static std::map<uint8_t, uint8_t[6]> axisToMac;

    // Function prototypes
    void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len);
    void startStepper(int axis, bool reduced = false, bool external = false); // Standardwert für den dritten Parameter
    void requestPosition(uint8_t axis);
    
    /*
    void startStepper(int i, bool reduced, bool external);
    void stopStepper(int i);
    int axis2address(int axis);
    */

};
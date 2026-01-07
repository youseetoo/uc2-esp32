# CANopen Usage Examples

This document provides practical examples of using the CANopen features in the UC2-ESP32 CAN system.

## Table of Contents
- [Basic Network Management](#basic-network-management)
- [Device Discovery](#device-discovery)
- [Motor Control](#motor-control)
- [Laser Control](#laser-control)
- [LED Control](#led-control)
- [Configuration via SDO](#configuration-via-sdo)
- [Error Handling](#error-handling)
- [Serial Commands](#serial-commands)

## Basic Network Management

### Start All Nodes (Enter Operational State)

```cpp
// From master node
#ifdef CAN_MASTER
CANopen_Master::startNode(0);  // 0 = broadcast to all nodes
#endif
```

This puts all satellite nodes into OPERATIONAL state, enabling PDO communication.

### Stop a Specific Node

```cpp
#ifdef CAN_MASTER
CANopen_Master::stopNode(11);  // Stop Motor X (ID 11)
#endif
```

### Reset a Node

```cpp
#ifdef CAN_MASTER
CANopen_Master::resetNode(11);  // Soft reset Motor X
// Node will reboot and send boot-up message
#endif
```

### Check Node Online Status

```cpp
#ifdef CAN_MASTER
if (CANopen_Master::isNodeOnline(11)) {
    log_i("Motor X is online");
    CANopen_NMT_State state = CANopen_Master::getNodeState(11);
    // state will be OPERATIONAL, PREOPERATIONAL, STOPPED, or BOOTUP
}
#endif
```

## Device Discovery

### Scan Network for Devices

```cpp
#ifdef CAN_MASTER
// Scan using CANopen NMT
cJSON* devices = CANopen_Master::scanNetwork(2000);  // 2 second timeout

if (devices != NULL) {
    int count = cJSON_GetArraySize(devices);
    log_i("Found %d devices", count);
    
    for (int i = 0; i < count; i++) {
        cJSON* device = cJSON_GetArrayItem(devices, i);
        int nodeId = cJSON_GetObjectItem(device, "nodeId")->valueint;
        const char* state = cJSON_GetObjectItem(device, "stateStr")->valuestring;
        
        log_i("Device %d: state=%s", nodeId, state);
    }
    
    cJSON_Delete(devices);
}
#endif
```

### Get Device Information

```cpp
#ifdef CAN_MASTER
cJSON* info = CANopen_Master::getDeviceInfo(11);
if (info != NULL) {
    bool online = cJSON_GetObjectItem(info, "online")->valueint;
    log_i("Motor X online: %s", online ? "yes" : "no");
    
    cJSON_Delete(info);
}
#endif
```

## Motor Control

### Send Motor Command via PDO

```cpp
#ifdef CAN_MASTER
// Move Motor X to position 10000 at 1000 steps/sec
int32_t targetPosition = 10000;
uint32_t speed = 1000;

CANopen_Master::sendMotorCommand(11, targetPosition, speed);
#endif
```

**Direct PDO method:**

```cpp
#ifdef CAN_MASTER
CANopen_Motor_RPDO1 cmd;
cmd.targetPosition = 10000;
cmd.speed = 1000;

CANopen_Master::sendRpdo(11, 1, (uint8_t*)&cmd, sizeof(cmd));
#endif
```

### Receive Motor Status (Slave Side)

```cpp
#ifndef CAN_MASTER
// In motor controller firmware
void sendCurrentPosition(int32_t position, bool isRunning) {
    uint8_t statusWord = 0;
    
    if (FocusMotor::getData()[0]->isEnable) 
        statusWord |= MOTOR_STATUS_ENABLED;
    if (isRunning) 
        statusWord |= MOTOR_STATUS_RUNNING;
    if (!isRunning && position == FocusMotor::getData()[0]->targetPosition)
        statusWord |= MOTOR_STATUS_TARGET_REACHED;
    
    CANopen_Slave::sendMotorStatus(position, statusWord);
}
#endif
```

### Set Motor Parameters via SDO

```cpp
#ifdef CAN_MASTER
// Set motor speed to 2000 steps/sec
uint32_t speed = 2000;
CANopen_Master::sdoWrite(11, OD_MOTOR_SPEED, 0, (uint8_t*)&speed, sizeof(speed), 1000);

// Read current motor position
uint8_t data[4];
uint8_t size = 4;
if (CANopen_Master::sdoRead(11, OD_MOTOR_CURRENT_POSITION, 0, data, &size, 1000) == 0) {
    int32_t position = *(int32_t*)data;
    log_i("Motor position: %ld", position);
}
#endif
```

## Laser Control

### Set Laser Intensity via PDO

```cpp
#ifdef CAN_MASTER
// Set Laser 0 to 50% intensity (32768 out of 65535)
uint16_t intensity = 32768;
uint8_t laserId = 0;
uint8_t enable = 1;

CANopen_Master::sendLaserCommand(20, intensity, laserId, enable);
#endif
```

**Direct PDO method:**

```cpp
#ifdef CAN_MASTER
CANopen_Laser_RPDO1 cmd;
cmd.intensity = 32768;  // 50% of 65535
cmd.laserId = 0;
cmd.enable = 1;
memset(cmd.reserved, 0, sizeof(cmd.reserved));

CANopen_Master::sendRpdo(20, 1, (uint8_t*)&cmd, sizeof(cmd));
#endif
```

### Turn Off Laser

```cpp
#ifdef CAN_MASTER
CANopen_Master::sendLaserCommand(20, 0, 0, 0);
#endif
```

## LED Control

### Set LED Color via PDO

```cpp
#ifdef CAN_MASTER
// Set LED to red at full brightness
uint8_t mode = 1;         // LED_MODE_ON
uint8_t r = 255;
uint8_t g = 0;
uint8_t b = 0;
uint8_t brightness = 255;

CANopen_Master::sendLedCommand(30, mode, r, g, b, brightness);
#endif
```

**Direct PDO method:**

```cpp
#ifdef CAN_MASTER
CANopen_LED_RPDO1 cmd;
cmd.mode = 1;          // On
cmd.red = 255;
cmd.green = 0;
cmd.blue = 0;
cmd.brightness = 255;
memset(cmd.reserved, 0, sizeof(cmd.reserved));

CANopen_Master::sendRpdo(30, 1, (uint8_t*)&cmd, sizeof(cmd));
#endif
```

### Set LED Pattern

```cpp
#ifdef CAN_MASTER
// Pulse pattern with blue color
uint8_t mode = 3;  // Assume 3 = pulse mode
uint8_t r = 0;
uint8_t g = 0;
uint8_t b = 255;
uint8_t brightness = 200;

CANopen_Master::sendLedCommand(30, mode, r, g, b, brightness);
#endif
```

## Configuration via SDO

### Read Device Identity

```cpp
#ifdef CAN_MASTER
uint8_t data[4];
uint8_t size = 4;

// Read vendor ID
if (CANopen_Master::sdoRead(11, OD_IDENTITY_VENDOR_ID, 1, data, &size, 1000) == 0) {
    uint32_t vendorId = *(uint32_t*)data;
    log_i("Vendor ID: 0x%08X", vendorId);
}

// Read product code
if (CANopen_Master::sdoRead(11, OD_IDENTITY_PRODUCT_CODE, 2, data, &size, 1000) == 0) {
    uint32_t productCode = *(uint32_t*)data;
    log_i("Product Code: 0x%08X", productCode);
}

// Read serial number
if (CANopen_Master::sdoRead(11, OD_IDENTITY_SERIAL, 4, data, &size, 1000) == 0) {
    uint32_t serial = *(uint32_t*)data;
    log_i("Serial Number: %lu", serial);
}
#endif
```

### Set Heartbeat Interval

```cpp
#ifdef CAN_MASTER
// Set heartbeat to 500ms
uint16_t interval = 500;
CANopen_Master::sdoWrite(11, OD_PRODUCER_HEARTBEAT_TIME, 0, 
                         (uint8_t*)&interval, sizeof(interval), 1000);
#endif
```

### Configure Motor Soft Limits

```cpp
#ifdef CAN_MASTER
// Enable soft limits
uint8_t enabled = 1;
CANopen_Master::sdoWrite(11, OD_SOFT_LIMIT_ENABLE, 0, &enabled, 1, 1000);

// Set minimum position
int32_t minPos = -10000;
CANopen_Master::sdoWrite(11, OD_SOFT_LIMIT_MIN, 0, 
                         (uint8_t*)&minPos, sizeof(minPos), 1000);

// Set maximum position
int32_t maxPos = 10000;
CANopen_Master::sdoWrite(11, OD_SOFT_LIMIT_MAX, 0, 
                         (uint8_t*)&maxPos, sizeof(maxPos), 1000);
#endif
```

## Error Handling

### Send Emergency Message (Slave Side)

```cpp
#ifndef CAN_MASTER
// Motor position limit exceeded
uint16_t errorCode = EMCY_MOTOR_POSITION_LIMIT;
uint8_t errorRegister = EMCY_ERR_REG_DEVICE_PROFILE;

// Manufacturer-specific data (5 bytes)
uint8_t mfrData[5];
int32_t currentPos = FocusMotor::getData()[0]->currentPosition;
memcpy(mfrData, &currentPos, 4);
mfrData[4] = 0;  // Reserved

CANopen_Slave::sendEmergency(errorCode, errorRegister, mfrData);
#endif
```

### Clear Emergency

```cpp
#ifndef CAN_MASTER
// Error resolved
CANopen_Slave::clearEmergency();
#endif
```

### Monitor Errors (Master Side)

```cpp
#ifdef CAN_MASTER
// Check error status of a node
uint8_t errorStatus = CANopen_Master::getNodeErrorStatus(11);

if (errorStatus != 0) {
    log_w("Motor X has error: 0x%02X", errorStatus);
    
    if (errorStatus & EMCY_ERR_REG_DEVICE_PROFILE) {
        log_e("Device profile specific error");
    }
    if (errorStatus & EMCY_ERR_REG_TEMPERATURE) {
        log_e("Over-temperature error");
    }
}
#endif
```

## Serial Commands

The master node can receive JSON commands via serial to control the CAN network.

### Network Scan Command

```json
{
  "task": "/can_act",
  "scan": true,
  "qid": 1
}
```

**Response:**
```json
{
  "scan": [
    {"nodeId": 11, "state": 5, "stateStr": "operational", "errorRegister": 0},
    {"nodeId": 12, "state": 5, "stateStr": "operational", "errorRegister": 0},
    {"nodeId": 20, "state": 5, "stateStr": "operational", "errorRegister": 0}
  ],
  "qid": 1,
  "count": 3
}
```

### NMT Commands via Serial

**Start all nodes:**
```json
{
  "task": "/can_act",
  "nmt": {
    "command": "start",
    "nodeId": 0
  }
}
```

**Stop specific node:**
```json
{
  "task": "/can_act",
  "nmt": {
    "command": "stop",
    "nodeId": 11
  }
}
```

**Reset node:**
```json
{
  "task": "/can_act",
  "nmt": {
    "command": "reset",
    "nodeId": 11
  }
}
```

### Motor Control via Serial

```json
{
  "task": "/motor_act",
  "motor": {
    "steppers": [
      {
        "stepperid": 1,
        "position": 10000,
        "speed": 1000,
        "isabs": true,
        "isaccel": false
      }
    ]
  },
  "qid": 123
}
```

This will be automatically converted to CANopen PDO or legacy UC2 message depending on configuration.

## Best Practices

### 1. Always Check Node Online Before Commands

```cpp
#ifdef CAN_MASTER
if (CANopen_Master::isNodeOnline(11)) {
    CANopen_Master::sendMotorCommand(11, 1000, 500);
} else {
    log_w("Motor X is offline, skipping command");
}
#endif
```

### 2. Use PDOs for Real-Time Data, SDOs for Configuration

```cpp
#ifdef CAN_MASTER
// Configuration (once at startup) - use SDO
uint32_t maxSpeed = 5000;
CANopen_Master::sdoWrite(11, OD_MOTOR_SPEED, 0, 
                         (uint8_t*)&maxSpeed, sizeof(maxSpeed), 1000);

// Real-time control (in loop) - use PDO
CANopen_Master::sendMotorCommand(11, targetPosition, speed);
#endif
```

### 3. Handle Heartbeat Timeouts

```cpp
#ifdef CAN_MASTER
void loop() {
    can_controller::loop();  // This calls CANopen_Master::loop()
    
    // Check specific node
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 10000) {  // Check every 10 seconds
        if (!CANopen_Master::isNodeOnline(11)) {
            log_e("Motor X heartbeat timeout!");
            // Try to reset it
            CANopen_Master::resetNode(11);
        }
        lastCheck = millis();
    }
}
#endif
```

### 4. Start Nodes After Scan

```cpp
#ifdef CAN_MASTER
// Scan network
cJSON* devices = CANopen_Master::scanNetwork(2000);

// Put all found nodes into operational state
CANopen_Master::startNode(0);  // Broadcast

vTaskDelay(pdMS_TO_TICKS(100));  // Wait for state change

// Now ready for PDO communication
CANopen_Master::sendMotorCommand(11, 0, 1000);
#endif
```

### 5. Monitor Network Statistics

```cpp
#ifdef CAN_MASTER
cJSON* stats = CANopen_Master::getNetworkStats();
if (stats != NULL) {
    int activeNodes = cJSON_GetObjectItem(stats, "activeNodes")->valueint;
    int heartbeatsRx = cJSON_GetObjectItem(stats, "heartbeatsReceived")->valueint;
    
    log_i("Active nodes: %d, Heartbeats: %d", activeNodes, heartbeatsRx);
    
    cJSON_Delete(stats);
}
#endif
```

## Troubleshooting

### No Response from Node

1. Check node is powered on
2. Verify CAN bus connections (CAN_H, CAN_L, GND)
3. Check termination resistors (120Ω at both ends)
4. Scan network to see if node appears
5. Check heartbeat timeout setting

### SDO Timeout

1. Ensure node is in PRE-OPERATIONAL or OPERATIONAL state
2. Use NMT to start node: `CANopen_Master::startNode(nodeId)`
3. Check Object Dictionary index exists
4. Increase timeout if network is busy

### PDO Not Working

1. Node must be in OPERATIONAL state
2. Send NMT start command first
3. Verify PDO COB-ID is correct
4. Check message size matches PDO definition

### Emergency Messages

1. Check error register via SDO: `OD_ERROR_REGISTER`
2. Monitor serial output for EMCY messages
3. Clear error on slave: `CANopen_Slave::clearEmergency()`
4. Reset node if error persists

## Further Reading

- [CANopen Integration Guide](../../DOCS/CANopen_Integration.md)
- [CAN README](README.md)
- [CiA 301 Specification](https://www.can-cia.org/can-knowledge/canopen/canopen/)

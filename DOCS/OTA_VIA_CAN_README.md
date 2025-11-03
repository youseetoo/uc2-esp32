# OTA Firmware Update via CAN Bus

This feature allows you to update the firmware of ESP32 slave devices in a CAN network wirelessly via WiFi, without needing physical access to the device.

## Overview

The OTA (Over-The-Air) update mechanism works as follows:

1. **Master sends OTA command** via CAN bus to a specific slave device, including WiFi credentials
2. **Slave receives command** and connects to the specified WiFi network
3. **Slave starts ArduinoOTA server** and waits for firmware upload
4. **You upload firmware** using PlatformIO, Arduino IDE, or espota.py
5. **Slave reboots** with new firmware

## Hardware Requirements

- ESP32 Master device with CAN transceiver
- ESP32 Slave device(s) with CAN transceiver
- WiFi network accessible to the slave device
- USB connection to master (for sending initial command)

## JSON Command Format

From the master device, send the following JSON command:

```json
{
  "task": "/can_act",
  "ota": {
    "canid": 11,
    "ssid": "YourWiFiSSID",
    "password": "YourWiFiPassword",
    "timeout": 300000
  }
}
```

### Parameters:

- `canid`: CAN ID of the target slave device (decimal)
  - Motor X: 11, Motor Y: 12, Motor Z: 13, Motor A: 10
  - Laser 0: 20, Laser 1: 21, etc.
  - LED: 30
  - Galvo: 50
- `ssid`: WiFi network SSID (max 32 characters)
- `password`: WiFi password (max 64 characters)
- `timeout`: OTA server timeout in milliseconds (optional, default: 300000 = 5 minutes)

## Usage Methods

### Method 1: Python Script (Recommended)

Use the provided Python script to send the OTA command:

```bash
cd PYTHON
python ota_update_via_can.py \
  --port /dev/ttyUSB0 \
  --canid 11 \
  --ssid "MyWiFi" \
  --password "MyPassword" \
  --timeout 300000
```

**Arguments:**
- `--port, -p`: Serial port connected to master (e.g., `/dev/ttyUSB0`, `COM3`)
- `--canid, -c`: CAN ID of target slave (decimal)
- `--ssid, -s`: WiFi SSID
- `--password, -w`: WiFi password
- `--timeout, -t`: Timeout in milliseconds (optional, default 300000)
- `--find-ip`: Try to find device IP after sending command (optional)

### Method 2: Direct Serial Command

Connect to the master device via serial terminal and send:

```json
{"task": "/can_act", "ota": {"canid": 11, "ssid": "MyWiFi", "password": "MyPassword"}}
```

### Method 3: REST API (if WiFi is enabled on master)

Send HTTP POST request to master:

```bash
curl -X POST http://UC2-MASTER.local/can_act \
  -H "Content-Type: application/json" \
  -d '{"ota": {"canid": 11, "ssid": "MyWiFi", "password": "MyPassword"}}'
```

## Firmware Upload

After the slave connects to WiFi, it will be discoverable via mDNS as:

```
UC2-CAN-<CANID>.local
```

For example, CAN ID 11 (0xB in hex) will be: `UC2-CAN-B.local`

### Using PlatformIO

```bash
# From project root
platformio run -t upload --upload-port UC2-CAN-B.local

# Or specify environment
platformio run -e seeed_xiao_esp32s3_can_slave_motor -t upload --upload-port UC2-CAN-B.local
```

### Using Arduino IDE

1. Go to **Tools** → **Port**
2. Select **UC2-CAN-B at UC2-CAN-B.local**
3. Click **Upload**

### Using espota.py directly

```bash
python ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py \
  -i UC2-CAN-B.local \
  -f .pio/build/seeed_xiao_esp32s3_can_slave_motor/firmware.bin
```

## Process Flow

```
┌──────────┐                ┌──────────┐                ┌──────────┐
│  Master  │                │ CAN Bus  │                │  Slave   │
└─────┬────┘                └────┬─────┘                └────┬─────┘
      │                          │                           │
      │  OTA Command (JSON)      │                           │
      │─────────────────────────>│                           │
      │                          │  OTA_START Message        │
      │                          │──────────────────────────>│
      │                          │                           │
      │                          │                           │ Connect to WiFi
      │                          │                           │──────────┐
      │                          │                           │          │
      │                          │                           │<─────────┘
      │                          │                           │
      │                          │  OTA_ACK (Success)        │
      │                          │<──────────────────────────│
      │<─────────────────────────│                           │
      │                          │                           │
      │                          │                           │ Start OTA Server
      │                          │                           │──────────┐
      │                          │                           │          │
      │                          │                           │<─────────┘
┌─────┴────┐                     │                     ┌─────┴─────┐
│   PC     │                     │                     │ WiFi Net  │
└─────┬────┘                     │                     └─────┬─────┘
      │                          │                           │
      │          Upload Firmware via WiFi/mDNS               │
      │─────────────────────────────────────────────────────>│
      │                          │                           │
      │                          │                           │ Flash Firmware
      │                          │                           │──────────┐
      │                          │                           │          │
      │                          │                           │<─────────┘
      │                          │                           │
      │                          │                           │ Reboot
      │                          │                           │──────────┐
      │                          │                           │          │
      │                          │                           │<─────────┘
```

## OTA Acknowledgment

The slave sends back an acknowledgment to the master:

- **Status 0**: Success - Connected to WiFi, OTA server started
- **Status 1**: WiFi connection failed
- **Status 2**: OTA start failed

You can monitor the master's serial output to see these acknowledgments.

## CAN Message Protocol

### OTA_START Message (Master → Slave)

- **Message Type ID**: `0x60` (OTA_START)
- **Payload**:
  - SSID (32 bytes)
  - Password (64 bytes)
  - Timeout (4 bytes, uint32_t)

### OTA_ACK Message (Slave → Master)

- **Message Type ID**: `0x61` (OTA_ACK)
- **Payload**:
  - Status (1 byte): 0=success, 1=WiFi failed, 2=OTA failed
  - CAN ID (1 byte): Slave's CAN ID

## Troubleshooting

### Slave doesn't connect to WiFi

1. Check SSID and password are correct
2. Ensure WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
3. Check WiFi signal strength near the slave device
4. Monitor slave's serial output for error messages

### Cannot find device on network

1. Wait 30-60 seconds after sending OTA command
2. Check your computer is on the same WiFi network
3. Try using IP address instead of mDNS hostname
4. Check firewall settings
5. Use `avahi-browse -a` (Linux) or `dns-sd -B` (macOS) to discover mDNS devices

### Upload fails

1. Ensure device is still in OTA mode (check timeout hasn't expired)
2. Try using IP address instead of hostname
3. Check firewall/antivirus isn't blocking port 3232
4. Verify firmware binary is for correct target platform

### Timeout occurs before upload

Increase timeout in the command:

```json
{"task": "/can_act", "ota": {"canid": 11, "ssid": "WiFi", "password": "pass", "timeout": 600000}}
```

## Security Considerations

⚠️ **Important**: WiFi credentials are transmitted over CAN bus in **plain text**. 

For production systems, consider:
- Using a temporary WiFi network for OTA updates
- Implementing encryption on CAN messages
- Using WPA3 security on WiFi network
- Changing WiFi password after OTA update
- Using a separate VLAN for IoT devices

## Implementation Details

### Files Modified/Created

1. **`main/src/can/can_messagetype.h`** - Added `OTA_START` and `OTA_ACK` message types
2. **`main/src/can/OtaTypes.h`** - New header with OTA data structures
3. **`main/src/can/can_controller.h`** - Added OTA function declarations
4. **`main/src/can/can_controller.cpp`** - Implemented OTA functions:
   - `sendOtaStartCommandToSlave()` - Master sends OTA command
   - `handleOtaCommand()` - Slave handles OTA command
   - `sendOtaAck()` - Slave sends acknowledgment
5. **`PYTHON/ota_update_via_can.py`** - Python script for easy OTA initiation

### Required Build Flags

The OTA functionality requires ArduinoOTA to be enabled. This is already included in most configurations via `sdkconfig.defaults`:

```
CONFIG_ARDUINO_SELECTIVE_ArduinoOTA=y
```

## Example Scenarios

### Scenario 1: Update Motor Controller

```bash
# Send OTA command to motor X (CAN ID 11)
python ota_update_via_can.py --port /dev/ttyUSB0 --canid 11 \
  --ssid "LabWiFi" --password "lab123456"

# Wait 30 seconds, then upload
platformio run -e seeed_xiao_esp32s3_can_slave_motor -t upload \
  --upload-port UC2-CAN-B.local
```

### Scenario 2: Update Multiple Devices

```bash
# Update motor X
python ota_update_via_can.py --port /dev/ttyUSB0 --canid 11 \
  --ssid "WiFi" --password "pass"
sleep 30
platformio run -e seeed_xiao_esp32s3_can_slave_motor -t upload \
  --upload-port UC2-CAN-B.local

# Update motor Y  
python ota_update_via_can.py --port /dev/ttyUSB0 --canid 12 \
  --ssid "WiFi" --password "pass"
sleep 30
platformio run -e seeed_xiao_esp32s3_can_slave_motor -t upload \
  --upload-port UC2-CAN-C.local
```

### Scenario 3: Update LED Controller

```bash
python ota_update_via_can.py --port /dev/ttyUSB0 --canid 30 \
  --ssid "WiFi" --password "pass" --timeout 600000

platformio run -e seeed_xiao_esp32s3_can_slave_led_debug -t upload \
  --upload-port UC2-CAN-1E.local
```

## Known Limitations

1. Only one device can be updated at a time
2. Device must restart after OTA update
3. CAN communication is paused during WiFi connection and OTA upload
4. Requires physical access to master device to send initial command (unless master has WiFi/BT)
5. Maximum SSID length: 32 characters
6. Maximum password length: 64 characters

## Future Enhancements

Potential improvements for future versions:

- [ ] Encrypted credential transmission
- [ ] Batch update multiple devices
- [ ] Progress reporting via CAN
- [ ] Resume interrupted uploads
- [ ] Verify firmware before flashing
- [ ] Remote configuration via OTA
- [ ] Support for SPIFFS/LittleFS updates

## Support

For issues or questions:
- Check serial monitor output on both master and slave
- Enable debug logging: `-DCORE_DEBUG_LEVEL=5`
- Report issues on GitHub with full debug output

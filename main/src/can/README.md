# UC2 CAN Bus Communication with CANopen

This directory contains the CAN bus communication implementation for the UC2-ESP32 project, featuring CANopen-inspired standardization.

## Overview

The UC2 system uses a **master-satellite architecture** over CAN bus:

- **Master Node (Gateway)**: Central ESP32 that receives commands via Serial/USB and manages satellite devices
- **Satellite Nodes**: Specialized ESP32 devices (motors, LEDs, lasers, sensors) connected to the CAN bus

## Architecture

```
┌─────────────────┐
│   Host Computer │
│   (Python, etc) │
└────────┬────────┘
         │ USB/Serial
         │ JSON commands
         ▼
┌─────────────────┐      CAN Bus (500 kbps)
│  Master ESP32   │◄──────────────────────────────┐
│  (Gateway)      │                               │
└────────┬────────┘                               │
         │                                        │
    ┌────┴────┬────────┬────────┬────────┐       │
    │         │        │        │        │       │
    ▼         ▼        ▼        ▼        ▼       │
┌────────┐┌────────┐┌────────┐┌────────┐┌────────┐
│ Motor  ││ Motor  ││ Laser  ││  LED   ││ Galvo  │
│   X    ││   Y    ││  Ctrl  ││  Ctrl  ││  Ctrl  │
└────────┘└────────┘└────────┘└────────┘└────────┘
  ID: 11    ID: 12    ID: 20    ID: 30    ID: 40
```

## File Structure

### Core CAN Implementation

- **`can_controller.h/cpp`**: Main CAN communication controller
  - ISO-TP transport layer management
  - Message dispatching and routing
  - Device scanning and management
  - OTA update coordination
  - Legacy UC2 protocol support

- **`can_messagetype.h`**: Message type definitions for UC2 protocol
  - Motor, Laser, LED, Galvo, Sensor messages
  - Scan requests/responses
  - OTA commands

- **`iso-tp-twai/`**: ISO 15765-2 transport protocol implementation
  - Multi-frame message fragmentation/reassembly
  - Flow control
  - Built on ESP32 TWAI (Two-Wire Automotive Interface)

### CANopen Integration (New)

- **`canopen_types.h`**: CANopen protocol data structures
  - NMT (Network Management) messages
  - SDO (Service Data Objects) for parameter access
  - PDO (Process Data Objects) for real-time data
  - EMCY (Emergency) messages for error reporting
  - Object Dictionary indexes

- **`canopen_master.h/cpp`**: CANopen master/gateway implementation
  - NMT command transmission
  - SDO client for configuration
  - PDO transmission to satellites
  - Network scanning and monitoring
  - Heartbeat timeout detection

- **`canopen_slave.h/cpp`**: CANopen slave/satellite implementation
  - NMT state machine
  - SDO server for Object Dictionary access
  - PDO reception and transmission
  - Heartbeat generation
  - Emergency message transmission

### Supporting Files

- **`OtaTypes.h`**: Over-The-Air update structures
  - WiFi credentials for OTA
  - OTA acknowledgments
  - Scan response structures

## CAN ID Assignment

### Device Type Ranges

| Device Type | CAN ID Range | Description |
|-------------|--------------|-------------|
| Master      | 256 (0x100)  | Gateway/central node |
| Motors      | 10-19        | Stepper motor controllers |
| Lasers      | 20-29        | Laser intensity control |
| LEDs        | 30-39        | LED array control |
| Galvos      | 40-49        | Scanning galvo mirrors |
| Sensors     | 50-59        | Environmental sensors |

### Standard Motor IDs

- **ID 10**: Motor A (general purpose)
- **ID 11**: Motor X (X-axis)
- **ID 12**: Motor Y (Y-axis)
- **ID 13**: Motor Z (Z-axis / focus)

## Communication Protocols

The system supports **two protocols** simultaneously:

### 1. Legacy UC2 Protocol

Custom message format using ISO-TP transport:
- **MotorData**: Full motor control (position, speed, acceleration)
- **MotorDataReduced**: Optimized motor commands
- **LaserData**: Laser intensity control
- **LedCommand**: LED patterns and colors
- **HomeData**: Homing procedure parameters
- **ScanRequest/Response**: Network device discovery

### 2. CANopen Protocol (New)

Standard CANopen messages for better interoperability:

#### NMT (Network Management)
- **Start/Stop nodes**: Control operational state
- **Reset nodes**: Software reset or communication reset
- **States**: Bootup → Pre-operational → Operational → Stopped

#### Heartbeat
- **Periodic alive messages**: Every 1000ms (configurable)
- **Automatic timeout detection**: 5 seconds
- **Online/offline tracking**: Master monitors all satellites

#### SDO (Service Data Objects)
- **Configuration access**: Read/write Object Dictionary
- **Examples**:
  - Read device type: `Index 0x1000`
  - Set heartbeat interval: `Index 0x1017`
  - Motor soft limits: `Index 0x2020-0x2022`

#### PDO (Process Data Objects)
- **Real-time data transfer**: Low latency, no protocol overhead
- **Motor RPDO**: Position and speed commands
- **Motor TPDO**: Current position and status
- **Laser RPDO**: Intensity control
- **LED RPDO**: Color and pattern commands

#### Emergency Messages
- **Standardized error reporting**: Error codes and registers
- **Motor errors**: Position limits, over-temperature, drive faults
- **Laser errors**: Safety interlocks, over-power
- **LED errors**: Over-current, controller faults

## Usage Examples

### Master Node Setup

```cpp
#define CAN_MASTER
#include "can/can_controller.h"

void setup() {
    can_controller::setup();  // Initializes CANopen Master
}

void loop() {
    can_controller::loop();   // Handles heartbeat monitoring
}
```

### Satellite Node Setup

```cpp
#define MOTOR_CONTROLLER
#include "can/can_controller.h"

void setup() {
    can_controller::setup();  // Initializes CANopen Slave
    // Sends boot-up message automatically
}

void loop() {
    can_controller::loop();   // Sends periodic heartbeat
}
```

### Sending Motor Command (CANopen PDO)

```cpp
#ifdef CAN_MASTER
CANopen_Motor_RPDO1 cmd;
cmd.targetPosition = 10000;  // steps
cmd.speed = 1000;            // steps/sec

CANopen_Master::sendRpdo(11, 1, (uint8_t*)&cmd, sizeof(cmd));
// Sends to Motor X (ID 11)
#endif
```

### Reading Device Info (CANopen SDO)

```cpp
#ifdef CAN_MASTER
uint8_t data[4];
uint8_t size = 4;

// Read device type from node 11
CANopen_Master::sdoRead(11, 0x1000, 0, data, &size, 1000);
uint32_t deviceType = *(uint32_t*)data;
#endif
```

### Network Scan

```cpp
#ifdef CAN_MASTER
// Using CANopen NMT
cJSON* devices = CANopen_Master::scanNetwork(2000);
// Returns JSON array of discovered devices

// Using legacy scan
cJSON* devices = can_controller::scanCanDevices();
#endif
```

## CANopen Object Dictionary

See [CANopen_Integration.md](../../DOCS/CANopen_Integration.md) for complete Object Dictionary reference.

### Quick Reference

| Index  | Name | Access | Description |
|--------|------|--------|-------------|
| 0x1000 | Device Type | RO | Device type identifier |
| 0x1001 | Error Register | RO | Current error status |
| 0x1017 | Heartbeat Time | RW | Heartbeat interval (ms) |
| 0x2001 | Motor Target Pos | RW | Target position (steps) |
| 0x2002 | Motor Current Pos | RO | Current position (steps) |
| 0x2003 | Motor Speed | RW | Speed (steps/sec) |
| 0x3001 | Laser Intensity | RW | PWM intensity (0-65535) |
| 0x4001-3 | LED RGB | RW | Red/Green/Blue channels |

## Backward Compatibility

The CANopen layer is **fully backward compatible** with existing UC2 devices:

1. **Dual Protocol Support**: Both UC2 and CANopen messages are processed
2. **Automatic Detection**: Message type determined by COB-ID or message format
3. **Gradual Migration**: Update firmware progressively without breaking network
4. **Legacy Devices**: Continue to work with new master firmware

## Debugging

### Enable Debug Logging

In `PinConfig.h`:
```cpp
bool DEBUG_CAN_ISO_TP = true;
```

### Common Issues

**Heartbeat Timeouts**
- Check CAN bus termination (120Ω resistors at both ends)
- Verify baud rate (500 kbps)
- Check RX/TX pin connections

**SDO Timeouts**
- Node may be in wrong NMT state (use NMT start command)
- Object Dictionary index doesn't exist
- Node is offline (check heartbeat)

**PDO Not Working**
- Node must be in OPERATIONAL state
- Check PDO mapping configuration
- Verify COB-ID calculation

## Performance

- **CAN Bus Speed**: 500 kbps
- **Heartbeat Period**: 1000ms (configurable)
- **Heartbeat Timeout**: 5000ms
- **PDO Latency**: < 10ms (no fragmentation)
- **SDO Latency**: 50-100ms (expedited transfer)
- **Network Scan**: ~2 seconds (for all nodes)

## Future Enhancements

- [ ] CANopen Manager for advanced network configuration
- [ ] SDO Block Transfer for large data uploads
- [ ] SYNC messages for coordinated multi-axis motion
- [ ] Time Stamps for precision timing
- [ ] Electronic Data Sheets (EDS) for each device type
- [ ] CANopen configuration tools (CanFestival integration)

## References

- **CiA 301**: CANopen Application Layer and Communication Profile
- **CiA 402**: CANopen Device Profile for Drives and Motion Control
- **ISO 11898**: CAN bus specification
- **ISO 15765-2**: ISO-TP transport protocol
- [CANopen Integration Guide](../../DOCS/CANopen_Integration.md)
- [UC2 Documentation](https://openuc2.github.io/docs/)

## Contributing

When adding new CAN features:

1. Update both legacy and CANopen protocols
2. Maintain backward compatibility
3. Add tests for new message types
4. Update documentation
5. Follow existing code style
6. Test with real hardware if possible

## License

This code is part of the UC2-ESP32 project and inherits its license.

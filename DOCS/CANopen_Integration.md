# CANopen Integration for UC2-ESP32

## Overview

This document describes the CANopen-inspired integration for the UC2-ESP32 CAN communication system. The implementation follows CiA (CAN in Automation) specifications while maintaining backward compatibility with the existing custom protocol.

## Architecture

### Master-Satellite Model

The UC2 system uses a **master-satellite architecture**:

- **Master Node (Gateway)**: ESP32 device that:
  - Receives commands via Serial/USB from a host computer
  - Translates commands to CAN bus messages
  - Manages satellite devices (motors, LEDs, lasers, sensors)
  - Reports device states back to the host via Serial
  - Implements CANopen Network Management (NMT)

- **Satellite Nodes**: Specialized ESP32 devices for:
  - **Motor Controllers** (CAN IDs 10-19): Stepper motor control
  - **Laser Controllers** (CAN IDs 20-29): Laser power control
  - **LED Controllers** (CAN IDs 30-39): LED array control
  - **Galvo Controllers** (CAN IDs 40-49): Scanning galvo mirrors
  - **Sensor Nodes** (CAN IDs 50-59): Environmental sensors

### CANopen Integration Approach

Instead of a full CANopen stack (which would be heavy for ESP32), we implement CANopen-**inspired** features:

1. **COB-ID Mapping**: Map UC2 message types to CANopen Communication Object IDs
2. **NMT States**: Implement basic Network Management (Pre-operational, Operational, Stopped)
3. **Heartbeat**: Periodic status messages for device monitoring
4. **Emergency Messages**: Standardized error reporting (EMCY)
5. **PDOs**: Process Data Objects for real-time motor/sensor data
6. **SDOs**: Service Data Objects for configuration (parameter access)

## CANopen Object Dictionary (OD)

### Standard Objects (CiA 301)

| Index | Sub | Name | Type | Access | Description |
|-------|-----|------|------|--------|-------------|
| 0x1000 | 0 | Device Type | UINT32 | RO | Device type identifier |
| 0x1001 | 0 | Error Register | UINT8 | RO | Error status bits |
| 0x1017 | 0 | Producer Heartbeat Time | UINT16 | RW | Heartbeat interval (ms) |
| 0x1018 | - | Identity Object | - | RO | Device identity |
| 0x1018 | 1 | Vendor ID | UINT32 | RO | 0x00002E9C (UC2 vendor ID) |
| 0x1018 | 2 | Product Code | UINT32 | RO | Device-specific code |
| 0x1018 | 3 | Revision Number | UINT32 | RO | Firmware version |
| 0x1018 | 4 | Serial Number | UINT32 | RO | Unique device serial |

### Communication Objects (CiA 301)

| Index | Sub | Name | Type | Access | Description |
|-------|-----|------|------|--------|-------------|
| 0x1400-0x15FF | - | RPDO Communication | - | RW | Receive PDO parameters |
| 0x1600-0x17FF | - | RPDO Mapping | - | RW | Receive PDO mapping |
| 0x1800-0x19FF | - | TPDO Communication | - | RW | Transmit PDO parameters |
| 0x1A00-0x1BFF | - | TPDO Mapping | - | RW | Transmit PDO mapping |

### Manufacturer-Specific Objects (UC2)

#### Motor Controller Objects (0x2000-0x2FFF)

| Index | Sub | Name | Type | Access | Description |
|-------|-----|------|------|--------|-------------|
| 0x2000 | 0 | Motor Control | UINT8 | RW | Motor enable/disable |
| 0x2001 | 0 | Target Position | INT32 | RW | Target position (steps) |
| 0x2002 | 0 | Current Position | INT32 | RO | Current position (steps) |
| 0x2003 | 0 | Speed | UINT32 | RW | Motor speed (steps/sec) |
| 0x2004 | 0 | Acceleration | UINT32 | RW | Acceleration (steps/sec²) |
| 0x2005 | 0 | Motor Status | UINT8 | RO | Running/stopped status |
| 0x2010 | 0 | Homing Control | UINT8 | WO | Start homing sequence |
| 0x2011 | 0 | Homing Speed | UINT32 | RW | Homing speed |
| 0x2012 | 0 | Homing Direction | INT8 | RW | Homing direction |
| 0x2013 | 0 | Homing Status | UINT8 | RO | Homing state |
| 0x2020 | 0 | Soft Limit Enable | UINT8 | RW | Enable/disable soft limits |
| 0x2021 | 0 | Soft Limit Min | INT32 | RW | Minimum position limit |
| 0x2022 | 0 | Soft Limit Max | INT32 | RW | Maximum position limit |

#### Laser Controller Objects (0x3000-0x3FFF)

| Index | Sub | Name | Type | Access | Description |
|-------|-----|------|------|--------|-------------|
| 0x3000 | 0 | Laser Enable | UINT8 | RW | Laser on/off |
| 0x3001 | 0 | Laser Intensity | UINT16 | RW | PWM intensity (0-65535) |
| 0x3002 | 0 | Laser ID | UINT8 | RO | Laser channel ID |

#### LED Controller Objects (0x4000-0x4FFF)

| Index | Sub | Name | Type | Access | Description |
|-------|-----|------|------|--------|-------------|
| 0x4000 | 0 | LED Mode | UINT8 | RW | LED mode (off, on, pattern) |
| 0x4001 | 0 | LED Color R | UINT8 | RW | Red channel (0-255) |
| 0x4002 | 0 | LED Color G | UINT8 | RW | Green channel (0-255) |
| 0x4003 | 0 | LED Color B | UINT8 | RW | Blue channel (0-255) |
| 0x4004 | 0 | LED Brightness | UINT8 | RW | Overall brightness (0-255) |

## COB-ID Assignment

### Standard CANopen COB-IDs

CANopen uses an 11-bit CAN identifier split into:
- **Function Code** (bits 7-10): Message type
- **Node-ID** (bits 0-6): Device address (1-127)

| Function | COB-ID Range | Description |
|----------|--------------|-------------|
| NMT | 0x000 | Network management (from master) |
| SYNC | 0x080 | Synchronization object |
| EMCY | 0x080 + Node-ID | Emergency messages |
| TPDO1 | 0x180 + Node-ID | Transmit PDO 1 (real-time data) |
| RPDO1 | 0x200 + Node-ID | Receive PDO 1 (real-time data) |
| TPDO2 | 0x280 + Node-ID | Transmit PDO 2 |
| RPDO2 | 0x300 + Node-ID | Receive PDO 2 |
| TPDO3 | 0x380 + Node-ID | Transmit PDO 3 |
| RPDO3 | 0x400 + Node-ID | Receive PDO 3 |
| TPDO4 | 0x480 + Node-ID | Transmit PDO 4 |
| RPDO4 | 0x500 + Node-ID | Receive PDO 4 |
| SDO TX | 0x580 + Node-ID | SDO response (to master) |
| SDO RX | 0x600 + Node-ID | SDO request (from master) |
| Heartbeat | 0x700 + Node-ID | Node heartbeat/node guarding |

### UC2 CAN ID Mapping

Current UC2 CAN IDs are preserved for backward compatibility:

| Device Type | UC2 CAN ID | CANopen Node-ID | Notes |
|-------------|-----------|-----------------|-------|
| Master | 0x100 (256) | - | Gateway node (broadcasts NMT) |
| Motor A | 10 | 10 | First motor |
| Motor X | 11 | 11 | X-axis motor |
| Motor Y | 12 | 12 | Y-axis motor |
| Motor Z | 13 | 13 | Z-axis motor |
| Laser 0 | 20 | 20 | Laser channel 0 |
| Laser 1 | 21 | 21 | Laser channel 1 |
| LED Array | 30 | 30 | LED controller |
| Galvo | 40 | 40 | Galvo scanner |

## Network Management (NMT)

### NMT States

Each satellite node implements these CANopen NMT states:

1. **Initialization** (Boot-up): Device powers on, performs self-test
2. **Pre-operational**: Configuration mode, SDOs allowed, PDOs disabled
3. **Operational**: Normal operation, PDOs enabled
4. **Stopped**: All communication disabled except heartbeat and NMT

### NMT Commands (from Master)

| Command | COB-ID | Data Byte 0 | Data Byte 1 | Description |
|---------|--------|-------------|-------------|-------------|
| Start Remote Node | 0x000 | 0x01 | Node-ID | Enter Operational |
| Stop Remote Node | 0x000 | 0x02 | Node-ID | Enter Stopped |
| Enter Pre-operational | 0x000 | 0x80 | Node-ID | Config mode |
| Reset Node | 0x000 | 0x81 | Node-ID | Software reset |
| Reset Communication | 0x000 | 0x82 | Node-ID | Reset CAN params |

Node-ID = 0 means broadcast to all nodes.

### Boot-up Message

On startup, each node sends a boot-up message:
- COB-ID: 0x700 + Node-ID
- Data: [0x00] (1 byte)
- This informs the master that the node is online

## Heartbeat Protocol

Satellites send periodic heartbeat messages to indicate they're alive:

- **COB-ID**: 0x700 + Node-ID
- **DLC**: 1 byte
- **Data[0]**: Current NMT state
  - 0x00: Boot-up
  - 0x04: Stopped
  - 0x05: Operational
  - 0x7F: Pre-operational

Heartbeat interval is configurable via OD index 0x1017 (default: 1000ms).

## Emergency Messages (EMCY)

Satellites send emergency messages on critical errors:

- **COB-ID**: 0x080 + Node-ID
- **DLC**: 8 bytes
- **Data**:
  - Byte 0-1: Error code (UINT16)
  - Byte 2: Error register
  - Byte 3-7: Manufacturer-specific error info

### UC2 Error Codes

| Error Code | Description |
|------------|-------------|
| 0x0000 | No error |
| 0x1000 | Generic error |
| 0x2300 | Motor position limit exceeded |
| 0x2310 | Motor over-temperature |
| 0x2320 | Motor drive error |
| 0x3000 | Laser safety interlock |
| 0x4000 | LED controller error |
| 0x5000 | Communication error |
| 0x6000 | Sensor failure |

## Process Data Objects (PDOs)

PDOs provide fast, real-time data transfer without protocol overhead.

### Motor Controller PDO Mapping

**RPDO1** (Master → Motor): Motor command
- COB-ID: 0x200 + Node-ID
- Byte 0-3: Target position (INT32)
- Byte 4-7: Speed (UINT32)

**TPDO1** (Motor → Master): Motor status
- COB-ID: 0x180 + Node-ID
- Byte 0-3: Current position (INT32)
- Byte 4: Status (UINT8)
  - Bit 0: Motor enabled
  - Bit 1: Motor running
  - Bit 2: Target reached
  - Bit 3: Homing active
  - Bit 4: Error

### Laser Controller PDO Mapping

**RPDO1** (Master → Laser): Laser control
- COB-ID: 0x200 + Node-ID
- Byte 0-1: Intensity (UINT16)
- Byte 2: Laser ID (UINT8)

## Service Data Objects (SDOs)

SDOs provide request/response access to the Object Dictionary for configuration.

### SDO Protocol

**SDO Request** (Master → Satellite):
- COB-ID: 0x600 + Node-ID
- Byte 0: Command specifier
  - 0x40: Read (upload)
  - 0x22: Write 4 bytes (download)
  - 0x2B: Write 2 bytes
  - 0x2F: Write 1 byte
- Byte 1-2: Object index (little-endian)
- Byte 3: Sub-index
- Byte 4-7: Data (for write commands)

**SDO Response** (Satellite → Master):
- COB-ID: 0x580 + Node-ID
- Byte 0: Command specifier
  - 0x43: Read response (4 bytes)
  - 0x4B: Read response (2 bytes)
  - 0x4F: Read response (1 byte)
  - 0x60: Write confirmation
  - 0x80: Abort (error)
- Byte 1-2: Object index (little-endian)
- Byte 3: Sub-index
- Byte 4-7: Data (for read responses) or abort code

## Implementation Strategy

### Phase 1: CANopen Layer (Minimal Changes)

Create new files without modifying existing code:

1. **canopen_types.h**: CANopen data structures (NMT, SDO, PDO, EMCY)
2. **canopen_od.h**: Object Dictionary definitions
3. **canopen_master.cpp/h**: Master-side CANopen functions
4. **canopen_slave.cpp/h**: Satellite-side CANopen functions

### Phase 2: Integration

Integrate CANopen layer with existing `can_controller`:

1. Add CANopen message handlers to `dispatchIsoTpData()`
2. Implement NMT state machine in satellite nodes
3. Add heartbeat generation in satellite `loop()`
4. Map existing commands to PDO/SDO equivalents

### Phase 3: Gateway Enhancement

Enhance master node with gateway features:

1. Serial-to-CAN command translation
2. CAN-to-Serial status reporting
3. Network management (device discovery, state control)
4. Error handling and recovery

## Backward Compatibility

The implementation maintains full backward compatibility:

1. **Existing messages**: Continue to work via ISO-TP
2. **CAN IDs**: Preserved (10-19 for motors, 20-29 for lasers, etc.)
3. **Message structures**: MotorData, LaserData, etc. remain unchanged
4. **API**: Serial JSON commands unchanged

CANopen features are **additive**:
- Satellites respond to both custom and CANopen messages
- Master can use either protocol
- Gradual migration path for firmware updates

## Benefits of CANopen Integration

1. **Standardization**: Industry-standard protocol widely used in automation
2. **Interoperability**: Easier integration with third-party CAN tools
3. **Robustness**: Built-in error handling (EMCY), network management (NMT)
4. **Monitoring**: Heartbeat for device health tracking
5. **Configuration**: Standardized parameter access via SDO
6. **Real-time**: Efficient PDO for time-critical data
7. **Tooling**: Compatible with CANopen configuration tools (CanFestival, etc.)

## Future Extensions

1. **CANopen Manager**: Full CANopen stack for advanced features
2. **SDO Block Transfer**: Faster configuration uploads
3. **Sync Messages**: Coordinated multi-axis motion
4. **Time Stamps**: Precision timing for synchronized operations
5. **Electronic Data Sheets (EDS)**: Machine-readable device descriptions

## References

- CiA 301: CANopen Application Layer and Communication Profile
- CiA 402: CANopen Device Profile for Drives and Motion Control
- ISO 11898: CAN bus specification
- ISO 15765-2: ISO-TP (current transport layer)

## Testing Plan

1. **Unit Tests**: Individual CANopen message parsers
2. **Integration Tests**: Master-satellite communication
3. **Interoperability**: Test with CANopen analysis tools
4. **Performance**: Latency and throughput benchmarks
5. **Robustness**: Error injection and recovery testing

# UC2 Binary Protocol Specification

This document describes the binary protocol implementation added alongside the existing JSON-based serial interface to reduce communication overhead.

## Overview

The UC2 binary protocol provides a more efficient alternative to JSON for high-frequency or large-data communications with the ESP32 firmware. Both protocols can coexist and the system can auto-detect which protocol is being used.

## Protocol Features

- **Backward Compatible**: Existing JSON interface remains fully functional
- **Auto-Detection**: System automatically detects JSON vs Binary messages
- **Switchable Modes**: Runtime switching between protocol modes
- **Efficient**: Reduced overhead for large data transmissions
- **Validated**: Built-in checksum validation

## Protocol Modes

1. **JSON_ONLY (0)**: Only accept JSON messages
2. **BINARY_ONLY (1)**: Only accept binary messages  
3. **AUTO_DETECT (2)**: Auto-detect message format (default)

## Binary Message Format

### Command Message Structure
```
[Header: 8 bytes] [Payload: variable length]
```

### Header Format
| Byte | Field | Description |
|------|-------|-------------|
| 0 | magic_start | Always 0xAB |
| 1 | version | Protocol version (0x01) |
| 2 | command_id | Command identifier |
| 3 | flags | Reserved (0x00) |
| 4-5 | payload_length | Length of payload (little-endian) |
| 6-7 | checksum | Simple sum of payload bytes |

### Response Format
```
[Response Header: 9 bytes] [Payload: variable length]
```

| Byte | Field | Description |
|------|-------|-------------|
| 0 | magic_start | Always 0xAB |
| 1 | version | Protocol version (0x01) |
| 2 | command_id | Original command identifier |
| 3 | status | 0=success, 1=error, 2=busy |
| 4-5 | payload_length | Length of response payload |
| 6-7 | checksum | Simple sum of payload bytes |
| 8 | magic_end | Always 0xCD |

## Command IDs

| ID | Command | Equivalent JSON Endpoint |
|----|---------|-------------------------|
| 0x01 | CMD_STATE_GET | /state_get |
| 0x02 | CMD_STATE_ACT | /state_act |
| 0x03 | CMD_STATE_BUSY | /b |
| 0x10 | CMD_MOTOR_GET | /motor_get |
| 0x11 | CMD_MOTOR_ACT | /motor_act |
| 0x20 | CMD_LED_GET | /ledarr_get |
| 0x21 | CMD_LED_ACT | /ledarr_act |
| 0x30 | CMD_LASER_GET | /laser_get |
| 0x31 | CMD_LASER_ACT | /laser_act |
| 0x40 | CMD_DIGITAL_OUT_GET | /digitalout_get |
| 0x41 | CMD_DIGITAL_OUT_ACT | /digitalout_act |
| 0x42 | CMD_DIGITAL_IN_GET | /digitalin_get |
| 0x43 | CMD_DIGITAL_IN_ACT | /digitalin_act |
| 0xF0 | CMD_PROTOCOL_SWITCH | Protocol control |
| 0xF1 | CMD_PROTOCOL_INFO | Protocol info |

## Protocol Control

### Switch Protocol Mode (JSON)
```json
{"task": "/protocol_set", "mode": 2}
```
Modes: 0=JSON only, 1=Binary only, 2=Auto-detect

### Get Protocol Info (JSON)
```json
{"task": "/protocol_get"}
```

### Switch Protocol Mode (Binary)
Send CMD_PROTOCOL_SWITCH (0xF0) with 1-byte payload containing the desired mode.

## Implementation Details

### Compilation
Binary protocol support is enabled by default but can be disabled:
```cpp
#define ENABLE_BINARY_PROTOCOL 0
```

### Auto-Detection
- Messages starting with '{' are treated as JSON
- Messages starting with 0xAB are treated as binary
- Invalid messages are rejected with appropriate error responses

### Integration
The binary protocol integrates with existing controller infrastructure by:
1. Parsing binary messages into equivalent cJSON structures
2. Using the existing `jsonProcessor()` and controller methods
3. Converting responses back to binary format when needed

## Example Usage

### Python Client Example
```python
import struct

def send_state_busy():
    header = struct.pack('<BBBBHH', 
                        0xAB,  # magic
                        0x01,  # version
                        0x03,  # CMD_STATE_BUSY
                        0x00,  # flags
                        0x00,  # payload_length
                        0x00)  # checksum
    serial.write(header)
```

### Response Handling
```python
def read_response():
    header = serial.read(9)
    magic, version, cmd_id, status, payload_len, checksum, magic_end = \
        struct.unpack('<BBBBHHB', header)
    
    if payload_len > 0:
        payload = serial.read(payload_len)
    return cmd_id, status, payload
```

## Performance Benefits

The binary protocol provides several advantages over JSON:

1. **Size Reduction**: Headers are fixed 8 bytes vs variable JSON overhead
2. **Parsing Speed**: Simple binary unpacking vs JSON parsing
3. **Memory Usage**: No string allocation for simple commands
4. **Bandwidth**: Especially beneficial for frequent status checks

## Testing

Use the provided `test_binary_client.py` to test binary protocol functionality:

```bash
python3 test_binary_client.py
```

The test script demonstrates:
- Binary vs JSON performance comparison
- Protocol switching
- Command examples
- Response handling
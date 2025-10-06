# UC2 Binary Protocol - Build and Test Guide

This guide explains how to compile and test the new binary protocol feature.

## Compilation

### Prerequisites
- PlatformIO Core installed
- ESP32 toolchain (automatically downloaded by PlatformIO)

### Build Commands

```bash
# Enable binary protocol (default is enabled)
# No additional flags needed - protocol is enabled by default

# Build for UC2_3 board (most common)
pio run --environment UC2_3

# Build for other board variants
pio run --environment seeed_xiao_esp32s3
pio run --environment UC2_WEMOS

# To disable binary protocol (reduce firmware size)
# Add to platformio.ini build_flags:
# -DENABLE_BINARY_PROTOCOL=0
```

### Firmware Upload

```bash
# Upload to connected ESP32
pio run --target upload --environment UC2_3

# Monitor serial output
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

## Testing

### 1. Unit Tests (Native)
The unit tests can be compiled for native execution:

```bash
# Run tests (requires test environment setup)
# Note: Some tests may require ESP32 specific features
python3 validate_implementation.py
```

### 2. Hardware Testing

#### Using the Test Client
```bash
# Install dependencies
pip3 install pyserial

# Run the test client (adjust port as needed)
python3 test_binary_client.py

# Run the demo
python3 demo_binary_protocol.py
```

#### Using the UC2-REST Extension
```python
from uc2_serial_binary import UC2SerialBinary
import serial

# Connect to ESP32
ser = serial.Serial('/dev/ttyUSB0', 115200)
uc2 = UC2SerialBinary(ser)

# Test protocol switching
uc2.switch_protocol_mode(2)  # Auto-detect mode

# Performance comparison
json_time = time_command(lambda: uc2.get_state(use_binary=False))
binary_time = time_command(lambda: uc2.get_state(use_binary=True))

print(f"JSON: {json_time:.3f}s, Binary: {binary_time:.3f}s")
```

### 3. Manual Testing via Serial Monitor

#### JSON Commands (existing)
```json
{"task":"/state_get"}
{"task":"/b"}
{"task":"/protocol_set", "mode": 2}
{"task":"/protocol_get"}
```

#### Binary Commands (hex bytes)
```
State Busy: AB 01 03 00 00 00 00 00
State Get:  AB 01 01 00 00 00 00 00
Protocol Info: AB 01 F1 00 00 00 00 00
```

## Verification

### Check Protocol is Active
Send JSON command:
```json
{"task":"/protocol_get"}
```

Expected response:
```json
{"protocol":"hybrid","binary_enabled":1,"mode":2,"version":1}
```

### Performance Test
1. Send repeated `/b` commands using JSON
2. Send repeated CMD_STATE_BUSY (0x03) using binary
3. Compare response times

Expected: Binary protocol should be 30-70% faster for simple commands.

## Troubleshooting

### Common Issues

1. **Binary commands not recognized**
   - Check that ENABLE_BINARY_PROTOCOL=1 (default)
   - Verify magic byte 0xAB at start of message
   - Check message format matches specification

2. **Compilation errors**
   - Ensure all new files are in correct directories
   - Check that #include paths are correct
   - Verify ESP32 platform packages are installed

3. **Serial communication issues**
   - Check baud rate (default 115200)
   - Verify correct serial port
   - Test with JSON commands first

### Debug Messages
Enable debug output in platformio.ini:
```ini
build_flags = -DCORE_DEBUG_LEVEL=5
```

Binary protocol debug messages will show:
- Protocol initialization
- Message detection (JSON vs Binary)
- Command processing
- Response generation

## Integration with UC2-REST

To integrate with the existing UC2-REST Python library:

1. Copy `uc2_serial_binary.py` to your UC2-REST project
2. Modify existing serial classes to inherit from `UC2SerialBinary`
3. Use `send_command()` method which auto-selects protocol
4. Enable binary protocol for high-frequency operations

Example integration:
```python
# In existing UC2-REST code
from uc2_serial_binary import UC2SerialBinary

class UC2Serial(UC2SerialBinary):  # Extend existing class
    def __init__(self, port, baudrate=115200):
        ser = serial.Serial(port, baudrate)
        super().__init__(ser)
        self.set_protocol_mode("auto")  # Enable auto-detection
    
    def move_motor_fast(self, positions):
        # Use binary protocol for high-speed motor control
        for pos in positions:
            binary_payload = struct.pack('<if', motor_id, pos)
            self.send_binary_command("/motor_act", binary_payload)
```

This integration maintains backward compatibility while providing binary protocol benefits where needed.
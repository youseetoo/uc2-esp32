# AS5311 PCNT Encoder Interface for ESP32S3 CAN Motor Controller

This implementation adds PCNT (Pulse Counter) hardware-based encoder interface as an alternative to the existing interrupt-based approach for AS5311 linear encoders in ESP32S3 CAN motor controllers.

## Features

### Dual Interface Support
- **Interrupt-based**: Original implementation using GPIO interrupts and manual quadrature decoding
- **PCNT-based**: New hardware implementation using ESP-IDF PCNT peripheral for quadrature decoding

### Runtime Interface Selection
```cpp
// Switch to PCNT interface for encoder 1 (X-axis)
LinearEncoderController::setEncoderInterface(1, ENCODER_PCNT_BASED);

// Check current interface
EncoderInterface current = LinearEncoderController::getEncoderInterface(1);

// Check if PCNT is supported
bool pcntSupported = LinearEncoderController::isPCNTEncoderSupported();
```

### Encoder-Based Motion Control
```cpp
// Enable encoder-based motion control for a motor
FocusMotor::setEncoderBasedMotion(axis, true);

// Check if encoder-based motion is enabled
bool enabled = FocusMotor::isEncoderBasedMotionEnabled(axis);

// Direct MotorData access
motorData.encoderBasedMotion = true;

// Send via CAN to slave motor controller
can_controller::sendEncoderBasedMotionToCanDriver(axis, true);
```

### CAN Integration
The encoder-based motion flag is transmitted over CAN using the existing single-value update mechanism:
```cpp
can_controller::sendMotorSingleValue(axis, offsetof(MotorData, encoderBasedMotion), value);
```

## Configuration

### Enable Encoder Pins
In your pin configuration (e.g., `seeed_xiao_esp32s3_can_slave_motor/PinConfig.h`):
```cpp
// Enable encoder pins for X-axis
int8_t ENC_X_A = GPIO_NUM_21;  // Phase A
int8_t ENC_X_B = GPIO_NUM_20;  // Phase B
bool ENC_X_encoderDirection = true;  // Set direction
```

### Build Configuration
PCNT is automatically detected if ESP-IDF version 4.0+ is available. No additional build flags needed.

## Usage Example

### Basic Setup
```cpp
void setup() {
    // Initialize encoder controller (sets up both interfaces)
    LinearEncoderController::setup();
    
    // Switch X-axis to PCNT if available
    if (LinearEncoderController::isPCNTEncoderSupported()) {
        LinearEncoderController::setEncoderInterface(1, ENCODER_PCNT_BASED);
        log_i("X-axis using PCNT interface");
    }
}

void enablePreciseMotion(int axis) {
    // Enable encoder-based motion control
    FocusMotor::setEncoderBasedMotion(axis, true);
    
    // This automatically notifies CAN slaves if in CAN master mode
}
```

### Performance Comparison
```cpp
void compareEncoderInterfaces() {
    // Test interrupt-based interface
    LinearEncoderController::setEncoderInterface(1, ENCODER_INTERRUPT_BASED);
    // ... measure performance ...
    
    // Test PCNT-based interface  
    LinearEncoderController::setEncoderInterface(1, ENCODER_PCNT_BASED);
    // ... measure performance ...
}
```

## Benefits

### PCNT Interface Advantages
- **Lower CPU overhead**: Hardware quadrature decoding vs software interrupts
- **Higher precision**: Hardware counters with less jitter
- **Better real-time performance**: No interrupt latency issues

### Backward Compatibility
- Existing interrupt-based code continues to work unchanged
- Automatic fallback to interrupt-based if PCNT unavailable
- No breaking changes to existing motor control API

## Technical Details

### PCNT Configuration
- Uses PCNT units 0-2 for X, Y, Z axes respectively
- Configures dual-channel quadrature decoding
- 16-bit hardware counters with overflow handling
- Configurable encoder direction support

### CAN Protocol Extension
- Extends existing MotorData structure with `encoderBasedMotion` flag
- Uses existing single-value update mechanism for CAN transmission
- Maintains compatibility with existing CAN protocol

### Code Structure
- **LinearEncoderController**: Main interface with automatic fallback logic
- **PCNTEncoderController**: PCNT-specific implementation
- **FocusMotor**: Helper functions for encoder-based motion control
- **can_controller**: CAN transmission of encoder flags

## Testing

Run the encoder interface tests:
```bash
pio test -f test_encoder_pcnt
```

### Manual Testing
1. Enable encoder pins in configuration
2. Compare performance between interfaces:
   ```cpp
   // Measure timing for position reads
   uint32_t start = micros();
   float pos = LinearEncoderController::getCurrentPosition(1);
   uint32_t time_interrupt = micros() - start;
   
   LinearEncoderController::setEncoderInterface(1, ENCODER_PCNT_BASED);
   start = micros();
   pos = LinearEncoderController::getCurrentPosition(1);
   uint32_t time_pcnt = micros() - start;
   
   log_i("Interrupt: %u us, PCNT: %u us", time_interrupt, time_pcnt);
   ```

## Bug Fixes Included

- Fixed Y-axis encoder setup in LinearEncoderController (was incorrectly checking ENC_X_A instead of ENC_Y_A)
- Added proper bounds checking for encoder indices
- Resolved circular header dependencies

## Future Enhancements

1. **Closed-loop motion control**: Use encoder feedback for precise positioning
2. **JSON API integration**: Add encoder interface selection to motor control JSON API
3. **Home position detection**: Hardware-based homing using encoder index pulse
4. **Motion profiling**: Advanced motion control algorithms using encoder feedback
5. **Multi-axis coordination**: Coordinated motion using multiple encoder inputs

## Installation

This implementation is fully backward compatible. To use:

1. Update to the latest code
2. Optionally enable encoder pins in your configuration  
3. Use the new API functions to switch between interfaces
4. Test performance improvements with your specific hardware

The default behavior remains unchanged - existing interrupt-based encoder functionality continues to work exactly as before.
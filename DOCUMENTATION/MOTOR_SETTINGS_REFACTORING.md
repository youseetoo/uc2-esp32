# Motor Settings Refactoring

## Overview

This document describes the refactoring of motor data structures to reduce CAN/I2C bus traffic and improve code clarity.

## Problem

Previously, `MotorData` was sent over CAN/I2C bus for every motor start command. This structure contained ~40+ bytes including:
- Runtime action data (targetPosition, speed, isStop, etc.)
- Configuration settings (maxPos, minPos, trigger settings, pins, etc.)

The configuration settings rarely change and don't need to be sent with every motor command, wasting bandwidth and causing unnecessary overhead.

## Solution

Split motor data into two distinct structures:

### 1. **MotorAction** (Runtime Commands)
- Size: ~13 bytes
- Sent frequently (every motor move command)
- Contains only runtime action data:
  - `targetPosition`
  - `speed`
  - `isforever`
  - `absolutePosition`
  - `isStop`

**Legacy Compatibility**: `MotorDataReduced` is now a typedef alias for `MotorAction`

### 2. **MotorSettings** (Configuration)
- Size: ~52 bytes
- Sent once during initialization or when settings change
- Contains configuration data:
  - Direction inversion flags
  - Acceleration settings
  - Trigger configuration
  - Pin assignments
  - Soft limits (maxPos, minPos)
  - Advanced features (encoderBasedMotion)

### 3. **MotorData** (Legacy Full Structure)
- Kept for backward compatibility
- Still contains all fields but organized with comments
- Used internally on master/slave for full state management

## Usage

### Sending Motor Settings (One-time or rare)

#### Via CAN:
```cpp
MotorSettings settings;
settings.maxspeed = 20000;
settings.acceleration = 100000;
settings.maxPos = 100000;
settings.minPos = 0;
settings.softLimitEnabled = true;
settings.encoderBasedMotion = false;

can_controller::sendMotorSettingsToCANDriver(settings, axis);
```

#### Via I2C:
```cpp
MotorSettings settings;
settings.maxspeed = 20000;
settings.acceleration = 100000;

i2c_master::sendMotorSettingsToI2CDriver(settings, axis);
```

### Sending Motor Actions (Frequent)

#### Via CAN (using reduced mode):
```cpp
// Using existing API with reduced=1 parameter
MotorData motorData;
motorData.targetPosition = 1000;
motorData.speed = 5000;
motorData.isforever = false;
motorData.absolutePosition = true;
motorData.isStop = false;

// reduced=1 automatically sends only MotorAction/MotorDataReduced
can_controller::sendMotorDataToCANDriver(motorData, axis, 1);
```

#### Via I2C (using reduced mode):
```cpp
MotorData motorData;
motorData.targetPosition = 1000;
motorData.speed = 5000;
motorData.isforever = false;

// reduced=true sends only MotorAction/MotorDataReduced
i2c_master::sendMotorDataToI2CDriver(motorData, axis, true);
```

## Benefits

1. **Reduced Bus Traffic**: ~13 bytes instead of ~40+ bytes for frequent motor commands (67% reduction)
2. **Clearer Separation**: Configuration vs. runtime actions are now distinct
3. **Better Performance**: Less data transmission = faster communication
4. **Backward Compatible**: 
   - `MotorDataReduced` still works (typedef to `MotorAction`)
   - Full `MotorData` still supported
   - Existing `reduced` parameter in functions still works

## Implementation Details

### CAN Communication
- **Sender**: `can_controller::sendMotorSettingsToCANDriver()`
- **Receiver**: `can_controller::dispatchIsoTpData()` - handles `sizeof(MotorSettings)`

### I2C Communication
- **Sender**: `i2c_master::sendMotorSettingsToI2CDriver()`
- **Receiver**: `i2c_slave_motor::receiveEvent()` - handles `sizeof(MotorSettings)`

### Message Size Detection
Both CAN and I2C receivers use `sizeof()` to detect which struct was sent:
- `sizeof(MotorAction)` = 13 bytes → Runtime command
- `sizeof(MotorSettings)` = 52 bytes → Configuration update
- `sizeof(MotorData)` = Full struct → Legacy full update

## Migration Path

### Phase 1: Current Implementation ✅
- New structures defined
- Send/receive functions implemented for both CAN and I2C
- Backward compatible with existing code

### Phase 2: Future Optimization (TODO)
- Refactor `MotorData` to remove duplicate settings fields
- Create helper functions to convert between structures
- Add settings cache to avoid re-sending unchanged settings

### Phase 3: Complete Migration (TODO)
- Replace all full `MotorData` transmissions with appropriate split
- Remove legacy full-structure sends where not needed
- Optimize memory layout further

## Notes

- Settings fields still exist in `MotorData` for backward compatibility
- TODO comment marks these fields for future removal from `MotorData`
- Pin configuration (dirPin/stpPin) could be removed if `PinConfig` handles it globally

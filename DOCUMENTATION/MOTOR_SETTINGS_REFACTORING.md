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

## Smart Sending Mechanism

### Automatic Settings Transmission

The system now automatically handles when to send settings vs. actions:

1. **First Motor Start**: Settings are automatically sent on the first motor command
2. **Settings Changes**: When settings are modified (soft limits, acceleration, etc.), they are sent via `MotorSettings`
3. **Subsequent Movements**: Only `MotorAction` (reduced=1) is sent for movement commands
4. **After Restart**: Settings automatically reset when sending restart command to slave
5. **After Scan**: All settings flags reset when performing CAN network scan

### Implementation Details

#### Tracking Mechanism
- `motorSettingsSent[axis]` array tracks which axes have received settings
- Settings are sent automatically on first `startStepper()` call
- Explicit settings updates (e.g., `setSoftLimits()`) always send new settings

#### Helper Function
```cpp
MotorSettings extractMotorSettings(const MotorData& motorData)
```
Extracts only settings-related fields from `MotorData` struct.

## Usage

### Automatic Usage (Recommended)

Simply use the motor as normal - settings are sent automatically:

```cpp
// First motor start - automatically sends MotorSettings + MotorAction
FocusMotor::getData()[axis]->targetPosition = 1000;
FocusMotor::getData()[axis]->speed = 5000;
FocusMotor::startStepper(axis, 1); // reduced=1 for MotorAction

// Subsequent starts - only sends MotorAction
FocusMotor::getData()[axis]->targetPosition = 2000;
FocusMotor::startStepper(axis, 1); // Only ~13 bytes sent!
```

### Manual Settings Update

When changing settings explicitly:

#### Via CAN:
```cpp
// Update soft limits - automatically uses MotorSettings
can_controller::sendSoftLimitsToCANDriver(0, 100000, true, axis);
```

#### Via I2C:
```cpp
MotorSettings settings;
settings.maxspeed = 20000;
settings.acceleration = 100000;
settings.maxPos = 100000;
settings.minPos = 0;
settings.softLimitEnabled = true;

i2c_master::sendMotorSettingsToI2CDriver(settings, axis);
```

### Forcing Settings Resend

The system automatically resets settings flags when needed. Manual reset is rarely required:

```cpp
// Automatic scenarios (no manual action needed):
// - Sending restart command: {"task": "/can_act", "restart": 11}
// - Performing CAN scan: {"task": "/can_act", "scan": true}

// Manual reset (rarely needed):
can_controller::resetMotorSettingsFlag(axis);     // Single axis
can_controller::resetAllMotorSettingsFlags();     // All axes

// For I2C:
i2c_master::resetMotorSettingsFlag(axis);
i2c_master::resetAllMotorSettingsFlags();
```

## Benefits

1. **67% Reduced Bus Traffic**: ~13 bytes instead of ~40+ bytes for frequent motor commands
2. **Automatic Management**: No need to manually decide when to send settings
3. **Smart Caching**: Settings sent only once per axis (or when changed)
4. **Better Performance**: Less data transmission = faster communication
5. **100% Backward Compatible**: 
   - `MotorDataReduced` still works (typedef to `MotorAction`)
   - Full `MotorData` still supported for legacy code
   - Existing `reduced` parameter in functions still works

## Implementation Details

### CAN Communication
- **Auto-send on first start**: `can_controller::startStepper()`
- **Explicit settings**: `can_controller::sendMotorSettingsToCANDriver()`
- **Soft limits update**: `can_controller::sendSoftLimitsToCANDriver()` (now uses MotorSettings)
- **Receiver**: `can_controller::dispatchIsoTpData()` - handles `sizeof(MotorSettings)`

### I2C Communication
- **Auto-send on first start**: `i2c_master::startStepper()`
- **Explicit settings**: `i2c_master::sendMotorSettingsToI2CDriver()`
- **Receiver**: `i2c_slave_motor::receiveEvent()` - handles `sizeof(MotorSettings)`

### Message Size Detection
Both CAN and I2C receivers use `sizeof()` to detect which struct was sent:
- `sizeof(MotorAction)` = 13 bytes → Runtime command
- `sizeof(MotorSettings)` = 52 bytes → Configuration update
- `sizeof(MotorData)` = Full struct → Legacy full update

## Example Scenarios

### Scenario 1: First-time Motor Use
```cpp
// User sends first motor command
FocusMotor::getData()[X]->targetPosition = 1000;
FocusMotor::getData()[X]->speed = 5000;
FocusMotor::startStepper(X, 1);

// System automatically:
// 1. Checks motorSettingsSent[X] = false
// 2. Extracts settings from MotorData
// 3. Sends MotorSettings (52 bytes) via CAN/I2C
// 4. Marks motorSettingsSent[X] = true
// 5. Sends MotorAction (13 bytes) for the move
```

### Scenario 2: Subsequent Moves
```cpp
// User sends another motor command
FocusMotor::getData()[X]->targetPosition = 2000;
FocusMotor::startStepper(X, 1);

// System:
// 1. Checks motorSettingsSent[X] = true
// 2. Skips settings transmission
// 3. Only sends MotorAction (13 bytes) → 67% bandwidth saved!
```

### Scenario 3: Settings Update
```cpp
// User updates soft limits
can_controller::sendSoftLimitsToCANDriver(0, 100000, true, X);

// System:
// 1. Extracts current MotorSettings from MotorData[X]
// 2. Updates minPos, maxPos, softLimitEnabled
// 3. Sends complete MotorSettings (52 bytes)
// 4. motorSettingsSent[X] remains true
```

## Migration Path

### Phase 1: Current Implementation ✅
- New structures defined
- Send/receive functions implemented for both CAN and I2C
- Automatic settings transmission on first start
- Backward compatible with existing code
- Smart caching prevents redundant settings transmission

### Phase 2: Future Optimization (TODO)
- Refactor `MotorData` to remove duplicate settings fields
- Separate MotorData into MotorState (runtime) and MotorConfig (settings)
- Add settings version tracking for change detection
- Implement settings diff to send only changed values

### Phase 3: Complete Migration (TODO)
- Replace all full `MotorData` transmissions with appropriate split
- Remove legacy full-structure sends where not needed
- Optimize memory layout further
- Add telemetry to track bandwidth savings

## Testing & Validation

### Verify Settings Transmission
Enable debug logging to see when settings are sent:
```cpp
pinConfig.DEBUG_CAN_ISO_TP = true; // For CAN
```

Look for log messages:
- `"Sending MotorSettings to axis: X..."` - Settings sent
- `"Starting motor on axis X..."` - Motor action sent
- `"MotorSettings sent to CAN slave at address X"` - Successful transmission

### Monitor Bandwidth Savings
- **Before**: Every motor command = ~40+ bytes
- **After**: First command = 52 + 13 = 65 bytes, subsequent = 13 bytes
- **Savings**: After 5 moves: 67% reduction in total traffic

## Notes

- Settings fields still exist in `MotorData` for backward compatibility
- TODO comment marks these fields for future removal from `MotorData`
- Pin configuration (dirPin/stpPin) could be removed if `PinConfig` handles it globally
- Settings are cached per-axis to avoid redundant transmissions
- Slave restart requires resetting `motorSettingsSent[axis]` flag

# Motor Settings Usage Examples

## Quick Start

The motor settings system now **automatically** handles when to send configuration vs. movement commands. You don't need to manually manage this!

## Typical Usage (Automatic)

```cpp
// Example: Moving a motor via CAN bus

// First move - Settings are automatically sent once
FocusMotor::getData()[X]->targetPosition = 1000;
FocusMotor::getData()[X]->speed = 5000;
FocusMotor::getData()[X]->absolutePosition = true;
FocusMotor::startStepper(X, 1); // reduced=1 uses MotorAction (13 bytes)
// System sends: MotorSettings (52 bytes) + MotorAction (13 bytes) = 65 bytes

// Second move - Only movement data sent!
FocusMotor::getData()[X]->targetPosition = 2000;
FocusMotor::startStepper(X, 1);
// System sends: MotorAction (13 bytes) only
// 🎉 67% bandwidth saved!

// Third move - Still only movement data
FocusMotor::getData()[X]->targetPosition = -500;
FocusMotor::getData()[X]->speed = 3000;
FocusMotor::startStepper(X, 1);
// System sends: MotorAction (13 bytes) only
```

## Advanced Usage

### Updating Soft Limits

Soft limits are now automatically sent via `MotorSettings`:

```cpp
// Via CAN
can_controller::sendSoftLimitsToCANDriver(
    0,        // minPos
    100000,   // maxPos
    true,     // enabled
    X         // axis
);
// Internally uses MotorSettings struct

// Via I2C
i2c_master::sendSoftLimitsToI2CDriver(0, 100000, true, X);
```

### Manual Settings Update

If you need to manually update specific settings:

```cpp
// CAN Example
MotorSettings settings = can_controller::extractMotorSettings(*FocusMotor::getData()[X]);
settings.acceleration = 200000;
settings.maxspeed = 30000;
settings.encoderBasedMotion = true;
can_controller::sendMotorSettingsToCANDriver(settings, X);

// I2C Example
MotorSettings settings = i2c_master::extractMotorSettings(*FocusMotor::getData()[Y]);
settings.directionPinInverted = true;
settings.acceleration = 150000;
i2c_master::sendMotorSettingsToI2CDriver(settings, Y);
```

### Forcing Settings Resend

The system **automatically** resets settings flags in these scenarios:
- After sending a restart command to a CAN slave
- After performing a CAN network scan (new/restarted devices need settings)

Manual reset is rarely needed, but available if necessary:

```cpp
// Option 1: Reset specific axis (rarely needed)
can_controller::resetMotorSettingsFlag(X);

// Option 2: Reset all axes (rarely needed)  
can_controller::resetAllMotorSettingsFlags();

// Next motor command will automatically resend settings
FocusMotor::startStepper(X, 1);
```

For I2C:
```cpp
i2c_master::resetMotorSettingsFlag(axis);
i2c_master::resetAllMotorSettingsFlags();
```

## JSON API Examples

### Restarting a CAN Slave

Settings are **automatically** reset when restarting a slave:

```json
{
    "task": "/can_act",
    "restart": 11
}
```

System automatically:
1. Sends restart command to CAN ID 11
2. Resets `motorSettingsSent` flag for that axis
3. Next motor command will resend settings

### CAN Network Scan

Settings are **automatically** reset when scanning:

```json
{
    "task": "/can_act",
    "scan": true
}
```

System automatically:
1. Resets all `motorSettingsSent` flags
2. Scans for devices
3. Next motor commands will resend settings to all axes

### Setting Soft Limits via REST API

```json
{
    "task": "/motor_act", 
    "softlimits": {
        "steppers": [
            {
                "stepperid": 1, 
                "min": -100000, 
                "max": 100000, 
                "isen": 1
            }
        ]
    }
}
```

This automatically:
1. Saves to preferences
2. Sends via `MotorSettings` to CAN/I2C slave
3. Updates local motor data

### Motor Movement via REST API

```json
{
    "task": "/motor_act",
    "motor": {
        "steppers": [
            {
                "stepperid": 1,
                "position": 10000,
                "speed": 20000,
                "isabs": 1
            }
        ]
    }
}
```

System automatically:
1. Sends `MotorSettings` on first use (if not sent yet)
2. Sends `MotorAction` for the movement (always)

## Monitoring

### Enable Debug Logging

```cpp
// In your PinConfig or setup code
pinConfig.DEBUG_CAN_ISO_TP = true;
```

### Expected Log Output

**First Motor Start:**
```
Sending MotorSettings to axis: 1, maxspeed: 20000, acceleration: 100000, softLimitEnabled: 1
MotorSettings sent to CAN slave at address 11
Starting motor on axis 1 with speed 5000, targetPosition 1000, reduced: 1
Reducing MotorData to axis: 1 at address: 11, isStop: 0
```

**Subsequent Starts:**
```
Starting motor on axis 1 with speed 5000, targetPosition 2000, reduced: 1
Reducing MotorData to axis: 1 at address: 11, isStop: 0
```

Notice: No "Sending MotorSettings" message on subsequent starts!

## Bandwidth Comparison

### Before Optimization
Every motor command sends full `MotorData` (~85 bytes):
```
Move 1: 85 bytes
Move 2: 85 bytes
Move 3: 85 bytes
Move 4: 85 bytes
Move 5: 85 bytes
Total: 425 bytes
```

### After Optimization
First command sends settings + action, rest send only actions:
```
Move 1: 52 (settings) + 13 (action) = 65 bytes
Move 2: 13 bytes
Move 3: 13 bytes
Move 4: 13 bytes
Move 5: 13 bytes
Total: 117 bytes
```

**Savings: 72% reduction after 5 moves!**

## Common Scenarios

### Scenario: Multi-Axis Setup

```cpp
// Configure all axes on first use
for (int axis = 0; axis < 4; axis++) {
    FocusMotor::getData()[axis]->speed = 5000;
    FocusMotor::getData()[axis]->targetPosition = 1000 * axis;
    FocusMotor::startStepper(axis, 1);
}
// First time: Each axis sends Settings + Action
// Subsequent moves: Only Action

// Later movements - only actions sent
FocusMotor::getData()[X]->targetPosition = 5000;
FocusMotor::startStepper(X, 1); // Only 13 bytes!

FocusMotor::getData()[Y]->targetPosition = 3000;
FocusMotor::startStepper(Y, 1); // Only 13 bytes!
```

### Scenario: Changing Settings Mid-Operation

```cpp
// Normal operation
FocusMotor::getData()[Z]->targetPosition = 1000;
FocusMotor::startStepper(Z, 1); // 13 bytes

// User changes acceleration via API
// This updates settings and sends them
can_controller::sendSoftLimitsToCANDriver(0, 50000, true, Z); // 52 bytes

// Continue normal operation
FocusMotor::getData()[Z]->targetPosition = 2000;
FocusMotor::startStepper(Z, 1); // Back to 13 bytes
```

### Scenario: Slave Device Restart

Settings are **automatically** managed when restarting a slave:

```cpp
// Method 1: Using JSON API (recommended)
// {"task": "/can_act", "restart": 11}
// System automatically resets motorSettingsSent flag

// Method 2: Manual restart via code
can_controller::sendCANRestartByID(CAN_MOTOR_IDs[X]);
// motorSettingsSent[X] is automatically reset

// Next motor command automatically resends settings
FocusMotor::startStepper(X, 1);
// First sends: MotorSettings (52 bytes)
// Then sends: MotorAction (13 bytes)
```

Manual flag reset (only if needed for special cases):
```cpp
// Rarely needed - system handles this automatically
can_controller::resetMotorSettingsFlag(X);
```

## Best Practices

1. **Use `reduced=1` for all movement commands** over CAN/I2C
   - Maximizes bandwidth savings
   - Settings are sent automatically when needed

2. **Don't manually send settings unless necessary**
   - The system handles it automatically
   - Only send manually when changing configuration mid-operation

3. **Monitor debug logs during development**
   - Verify settings are sent only once per axis
   - Confirm subsequent moves use only MotorAction

4. **Reset settings flags after slave restart**
   - ✅ **Automatically handled** when using restart command or scan
   - Manual reset rarely needed

5. **Use soft limits API for limit updates**
   - Automatically uses MotorSettings
   - Updates both local and remote slaves
   - Persists to preferences

## Troubleshooting

### Problem: Motor not responding after slave restart

**Solution:** This should be automatic! The system resets the settings flag when you send a restart command. If you manually power-cycled the device:

```cpp
// Use the helper function (preferred)
can_controller::resetMotorSettingsFlag(axis);

// Or send a restart command via JSON which handles it automatically
// {"task": "/can_act", "restart": 11}
```

### Problem: After CAN scan, motors need reconfiguration

**Solution:** Already handled automatically! The scan command resets all settings flags.

### Problem: Unexpected behavior after settings change

**Solution:** Explicitly send new settings:
```cpp
MotorSettings settings = can_controller::extractMotorSettings(*FocusMotor::getData()[axis]);
// Modify settings as needed
can_controller::sendMotorSettingsToCANDriver(settings, axis);
```

### Problem: Want to verify settings were sent

**Solution:** Enable debug logging and look for:
```
"Sending MotorSettings to axis: X..."
"MotorSettings sent to CAN slave at address X"
```

## Summary

The new motor settings system provides:
- ✅ **Fully automatic** settings management - handles restart/scan automatically
- ✅ **67-72%** bandwidth reduction for typical operations
- ✅ **Smart caching** - settings sent only when needed
- ✅ **Auto-reset** - restart & scan commands automatically reset flags
- ✅ **100% backward compatible** - existing code works unchanged
- ✅ **Easy debugging** - clear log messages show what's happening

Simply use `startStepper()` with `reduced=1` and let the system handle everything!

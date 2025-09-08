# UC2 ESP32 Serial API Documentation

This document provides comprehensive documentation for the UC2 ESP32 serial communication API, focusing on encoder-based precision motion control.

## Motor Control API

### Standard Motor Movement
Move motors using standard step counting:

```json
{
  "task": "/motor_act",
  "motor": {
    "steppers": [{
      "stepperid": 0,
      "position": 10000,
      "speed": 20000,
      "isabs": 1,
      "isaccel": 0,
      "accel": 10000
    }]
  }
}
```

### Encoder-Based Precision Movement
Move motors using encoder feedback for high precision positioning:

```json
{
  "task": "/motor_act",
  "motor": {
    "steppers": [{
      "stepperid": 0,
      "position": 10000,
      "speed": 20000,
      "isabs": 1,
      "isaccel": 0,
      "accel": 10000,
      "enc": 1,
      "cp": 100.0,
      "ci": 0.5,
      "cd": 10.0
    }]
  }
}
```

**Parameters:**
- `stepperid`: Motor axis ID (0-3)
- `position`: Target position in unified step units
- `speed`: Maximum speed in unified step units per second
- `isabs`: 1 for absolute positioning, 0 for relative
- `isaccel`: 1 to enable acceleration, 0 for constant speed
- `accel`: Acceleration value in steps/s²
- `enc`: 1 to enable encoder-based precision mode (optional)
- `cp`: PID proportional gain (optional, default: 100.0)
- `ci`: PID integral gain (optional, default: 0.5)
- `cd`: PID derivative gain (optional, default: 10.0)

**Unified Step Units:**
Both standard and precision modes use the same step units. When `enc=1`, the firmware automatically converts step values to encoder units using the configured conversion factor (default: 1 step = 0.3125µm).

## Homing API

### Standard Homing
Home motors using endstop switches:

```json
{
  "task": "/home_act",
  "home": {
    "steppers": [{
      "stepperid": 1,
      "speed": -40000,
      "direction": -1,
      "timeout": 60000,
      "endstoppolarity": 0
    }]
  }
}
```

### Encoder-Based Homing
Home motors using encoder feedback (no endstop switches required):

```json
{
  "task": "/home_act",
  "home": {
    "steppers": [{
      "stepperid": 1,
      "speed": -40000,
      "direction": -1,
      "enc": 1
    }]
  }
}
```

**Parameters:**
- `stepperid`: Motor axis ID (0-3)
- `speed`: Homing speed in unified step units per second
- `direction`: Homing direction (-1 or 1)
- `timeout`: Maximum homing time in milliseconds (standard homing only)
- `endstoppolarity`: Endstop switch polarity (standard homing only)
- `enc`: 1 to enable encoder-based homing (optional)

**Encoder Homing Process:**
When `enc=1`, the motor moves continuously until the encoder detects no position change, indicating a mechanical constraint. The system then automatically sets the position to zero and saves it to persistent storage.

## Linear Encoder Configuration API

### Configure Global Settings
Set global conversion factors and encoder parameters:

```json
{
  "task": "/linearencoder_act",
  "config": {
    "stepsToEncoderUnits": 0.3125,
    "steppers": [{
      "stepperid": 1,
      "encdir": 1,
      "motdir": 0,
      "stp2phys": 2.0,
      "mumPerStep": 1.95
    }]
  }
}
```

**Parameters:**
- `stepsToEncoderUnits`: Global step-to-encoder conversion factor (µm per step)
- `stepperid`: Motor axis ID (0-3)
- `encdir`: Encoder direction (0 or 1)
- `motdir`: Motor direction inversion (0 or 1)
- `stp2phys`: Steps to physical units conversion factor
- `mumPerStep`: Micrometers per encoder step

### Direct Precision Movement (Legacy)
Direct control via LinearEncoderController:

```json
{
  "task": "/linearencoder_act",
  "moveP": {
    "steppers": [{
      "stepperid": 1,
      "position": 10000,
      "speed": 1000,
      "isabs": 1,
      "cp": 100.0,
      "ci": 0.5,
      "cd": 10.0
    }]
  }
}
```

**Note:** This legacy interface is maintained for compatibility. The recommended approach is to use `motor_act` with `enc=1`.

## Motor Status API

### Get Motor Status
Retrieve current motor positions and states:

```json
{
  "task": "/motor_get",
  "qid": 1
}
```

**Response:**
```json
{
  "motor": {
    "steppers": [{
      "stepperid": 0,
      "position": 12500,
      "isDone": 1,
      "encoderPosition": 3906.25,
      "encoderCount": 1250
    }]
  },
  "qid": 1
}
```

### Get Linear Encoder Status
Retrieve encoder-specific information:

```json
{
  "task": "/linearencoder_get",
  "linencoder": {
    "posval": 1,
    "id": 1
  }
}
```

## Configuration Persistence

All encoder configurations are automatically saved to ESP32 preferences and restored on startup:

- **Global conversion factor**: Persistent across restarts
- **Encoder directions and parameters**: Saved per motor axis
- **Encoder positions**: Automatically saved when motions complete and restored on startup

## Error Handling

The system provides comprehensive error handling:

- **Soft limits**: Prevent motion beyond configured boundaries
- **Timeout protection**: Prevents infinite loops during homing
- **Watchdog safety**: Non-blocking motion loops prevent system crashes
- **PID stability**: Built-in windup protection and output limiting

## Performance Characteristics

### Precision Mode (enc=1)
- **Position accuracy**: ±1-2 encoder counts typical
- **Repeatability**: Excellent for identical moves
- **Speed independence**: Accuracy maintained across all speeds
- **Real-time feedback**: Continuous PID correction during motion

### Standard Mode (enc=0)
- **Speed**: High-speed motion without encoder overhead
- **Simplicity**: Direct stepper control
- **Compatibility**: Full backward compatibility with existing systems

## CAN Bus Support

All motor and encoder commands work seamlessly over CAN bus networks, enabling distributed motion control across multiple ESP32 motor controllers while maintaining unified step units and precision capabilities.
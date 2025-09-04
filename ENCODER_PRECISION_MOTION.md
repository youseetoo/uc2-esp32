# Encoder-Based Precision Motion Control

This document describes the encoder-based precision motion control functionality that provides high-accuracy positioning using encoder feedback.

## Overview

The system supports two motion modes:
- **Standard mode** (`enc=0` or omitted): Uses stepper motor open-loop control
- **Precision mode** (`enc=1`): Uses encoder feedback with PID control for precise positioning

## Unified Step Units

Both modes now use the same **step units** for position and speed values, eliminating the need for frontend conversions. The firmware handles internal conversion between step units and encoder units using a configurable conversion factor.

### Default Conversion Factor

The default configuration assumes:
- **Stepper motor**: 1mm = 3200 steps (200 steps/rotation × 16 microsteps)
- **Encoder**: 1mm = 500 counts (with 2µm step width)
- **Conversion factor**: 1 step = 0.3125µm in encoder units

## Motor Commands

### Precision Motion with enc=1

```json
{
  "task": "/motor_act", 
  "motor": { 
    "steppers": [{ 
      "stepperid": 0, 
      "position": 10000, 
      "speed": 20000, 
      "isabs": 0, 
      "isaccel": 0, 
      "accel": 10000, 
      "enc": 1 
    }] 
  } 
}
```

When `enc=1` is specified:
- Position values are in **step units** (same as enc=0)
- System converts to encoder units internally using the conversion factor
- Motion control is delegated to LinearEncoderController with PID feedback
- High precision positioning regardless of motor step errors

### Encoder-Based Homing

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

Encoder-based homing:
- Moves continuously until encoder position stops changing
- Detects mechanical constraints without endstop switches
- Sets both motor and encoder positions to zero
- Speed value in step units, converted internally

## Configuration

### Global Conversion Factor

Configure the step-to-encoder conversion factor:

```json
{
  "task": "/linearencoder_act",
  "config": {
    "stepsToEncoderUnits": 0.3125
  }
}
```

This sets the global conversion factor (micrometers per step) and stores it permanently.

### Per-Axis Configuration

Configure encoder and motor relationships per axis:

```json
{
  "task": "/linearencoder_act",
  "config": {
    "steppers": [{
      "stepperid": 1,
      "encdir": 1,     // Encoder direction (0/1)
      "motdir": 0,     // Motor direction (0/1) 
      "stp2phys": 2.0, // Per-axis conversion factor
      "mumPerStep": 1.95 // Encoder resolution
    }]
  }
}
```

## Benefits

- **Unified units**: Frontend works with step values for both precision and standard modes
- **High precision**: Encoder feedback eliminates cumulative step errors
- **Flexible configuration**: Conversion factors stored permanently
- **CAN compatibility**: Works seamlessly with distributed motor controllers
- **Backward compatibility**: All existing functionality unchanged

## Technical Implementation

The conversion flow:
1. Frontend sends position in step units
2. If `enc=1`: firmware converts steps → encoder units using global factor
3. LinearEncoderController receives encoder units for PID control
4. Precise positioning achieved through encoder feedback

This approach provides the precision of encoder-based control while maintaining the simplicity of unified step units across the entire system.
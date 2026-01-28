# Galvo Scanner Migration Documentation

## Overview

This document describes the migration from the old `SPIRenderer` to the new `HighSpeedScannerCore` for galvo scanning in the UC2-ESP firmware.

## Migration Summary

The galvo scanner code has been upgraded from a simple blocking renderer to a high-performance FreeRTOS task-based architecture with:

- **Precise timing control** using `esp_timer` with overrun detection
- **Advanced scan patterns** with pre-blanking, cosine flyback, and settling regions
- **Optional X-axis LUT** for linearity correction
- **3-trigger system** (pixel, line, frame) for camera synchronization
- **JSON and CAN bus** configuration interfaces

## New Files

| File | Description |
|------|-------------|
| `HighSpeedScannerCore.h` | Scanner core class with `ScanConfig` structure |
| `HighSpeedScannerCore.cpp` | FreeRTOS task-based scanner implementation |
| `DAC_MCP4822.h` | MCP4822 dual 12-bit DAC driver header |
| `DAC_MCP4822.cpp` | Optimized SPI-based DAC driver |

## Updated Files

| File | Changes |
|------|---------|
| `GalvoController.h` | New scanner interface, backward-compatible `GalvoData` |
| `GalvoController.cpp` | Complete rewrite using `HighSpeedScannerCore` |

## Backup Files

| File | Description |
|------|-------------|
| `GalvoController_old.cpp` | Original implementation (kept for reference) |
| `SPIRenderer.h` | Legacy renderer header (still available) |
| `SPIRenderer.cpp` | Legacy renderer implementation (still available) |

---

## ScanConfig Structure

The new `ScanConfig` structure provides complete control over scanning parameters:

```cpp
struct ScanConfig {
    uint16_t nx;                    // Number of X samples per line
    uint16_t ny;                    // Number of lines (Y steps)
    uint16_t x_min, x_max;          // X range (0-4095 for 12-bit DAC)
    uint16_t y_min, y_max;          // Y range (0-4095)
    uint16_t pre_samples;           // Blanking samples before imaging
    uint16_t fly_samples;           // Flyback samples (cosine ease)
    uint16_t sample_period_us;      // Microseconds per sample
    uint16_t trig_delay_us;         // Trigger delay after position update
    uint16_t trig_width_us;         // Trigger pulse width
    uint16_t line_settle_samples;   // Settling samples after flyback
    uint8_t  enable_trigger;        // Enable pixel trigger output
    uint8_t  apply_x_lut;           // Apply X lookup table
    uint16_t frame_count;           // Number of frames (0 = continuous)
};
```

### Line Profile Structure

Each scan line consists of four regions:

```
[Pre-blanking] → [Imaging ramp] → [Flyback cosine] → [Settle]
```

1. **Pre-blanking**: Mirror held at `x_min` (no trigger)
2. **Imaging**: Linear ramp from `x_min` to `x_max` (pixel triggers active)
3. **Flyback**: Cosine-eased return to `x_min` (smooth deceleration/acceleration)
4. **Settle**: Mirror held at `x_min` (mechanical settling time)

---

## JSON API

### New Format (Recommended)

Full control over all scan parameters:

```json
{
    "task": "/galvo_act",
    "qid": 1,
    "config": {
        "nx": 512,
        "ny": 512,
        "x_min": 1500,
        "x_max": 4000,
        "y_min": 1500,
        "y_max": 4000,
        "pre_samples": 4,
        "fly_samples": 16,
        "sample_period_us": 1,
        "trig_delay_us": 0,
        "trig_width_us": 1,
        "line_settle_samples": 0,
        "enable_trigger": 1,
        "apply_x_lut": 0,
        "frame_count": 1
    }
}
```

### Legacy Format (Backward Compatible)

The old API format is still supported and automatically converted:

```json
{
    "task": "/galvo_act",
    "qid": 1,
    "X_MIN": 0,
    "X_MAX": 30000,
    "Y_MIN": 0,
    "Y_MAX": 30000,
    "STEP": 1000,
    "tPixelDwelltime": 1,
    "nFrames": 1,
    "fastMode": true
}
```

Legacy parameter mapping:
- `X_MIN/X_MAX` (0-30000) → `x_min/x_max` (0-4095)
- `Y_MIN/Y_MAX` (0-30000) → `y_min/y_max` (0-4095)
- `STEP` → Computed to `nx` and `ny`
- `tPixelDwelltime` → `sample_period_us`
- `nFrames` → `frame_count`
- `fastMode` → Affects `pre_samples`, `fly_samples`, `line_settle_samples`

### Stop Command

```json
{
    "task": "/galvo_act",
    "qid": 1,
    "stop": true
}
```

### Get Status

```json
{
    "task": "/galvo_get"
}
```

Response:
```json
{
    "isRunning": true,
    "current_line": 256,
    "current_frame": 1,
    "timing_overruns": 0,
    "config": { ... },
    "X_MIN": 10922,
    "X_MAX": 29260,
    ...
}
```

---

## CAN Bus Protocol

The `GalvoData` structure is used for CAN communication (backward compatible):

```cpp
struct GalvoData {
    int32_t X_MIN;           // Legacy X range (0-30000)
    int32_t X_MAX;
    int32_t Y_MIN;           // Legacy Y range (0-30000)
    int32_t Y_MAX;
    int32_t STEP;            // Step size
    int32_t tPixelDwelltime; // Dwell time in µs
    int32_t nFrames;         // Number of frames
    bool fastMode;           // Fast mode flag
    bool isRunning;          // Running state
    int qid;                 // Queue ID
    
    // Conversion methods
    ScanConfig toScanConfig() const;
    void fromScanConfig(const ScanConfig& cfg);
};
```

### Master Mode (`CAN_SEND_COMMANDS` defined)

Commands are forwarded over CAN bus:
```cpp
can_controller::sendGalvoDataToCANDriver(galvoData);
```

### Slave Mode (`CAN_RECEIVE_GALVO` defined)

Commands received from CAN bus:
```cpp
GalvoController::setFromGalvoData(receivedData);
GalvoController::startScan();
```

State reported back to master:
```cpp
GalvoController::sendCurrentStateToMaster();
```

---

## PlatformIO Configuration

The galvo scanner uses these build flags (already defined):

```ini
[env:seeed_xiao_esp32s3_can_slave_galvo_default]
build_flags = 
    -DESP32S3_MODEL_XIAO=1
    -DMESSAGE_CONTROLLER=1
    -DCAN_BUS_ENABLED=1
    -DGALVO_CONTROLLER=1
    -DCAN_RECEIVE_GALVO=1
```

**No changes to platformio.ini are required.**

---

## Pin Configuration

Pins are read from `pinConfig` (defined in your PinConfig files):

| Function | Pin Config Field | Typical GPIO |
|----------|-----------------|--------------|
| DAC SDI (MOSI) | `galvo_sdi` | GPIO7/9 |
| DAC SCK | `galvo_sck` | GPIO8/7 |
| DAC CS | `galvo_cs` | GPIO9/8 |
| DAC LDAC | `galvo_ldac` | GPIO6 |
| Pixel Trigger | `galvo_trig_pixel` | GPIO2 |
| Line Trigger | `galvo_trig_line` | GPIO3 |
| Frame Trigger | `galvo_trig_frame` | GPIO4 |

---

## Performance Considerations

### Maximum Speed Operation

For maximum scan speed:

1. **Set `sample_period_us = 0`** - No inter-sample delay (SPI limited)
2. **Minimize `pre_samples` and `line_settle_samples`** - Reduces overhead
3. **Use small `fly_samples`** - Faster flyback
4. **Disable triggers if not needed** - `enable_trigger = 0`

Example fast scan config:
```json
{
    "config": {
        "nx": 256,
        "ny": 256,
        "x_min": 500,
        "x_max": 3500,
        "y_min": 500,
        "y_max": 3500,
        "pre_samples": 0,
        "fly_samples": 4,
        "sample_period_us": 0,
        "trig_width_us": 1,
        "line_settle_samples": 0,
        "enable_trigger": 1,
        "frame_count": 1
    }
}
```

### Timing Precision

The scanner task:
- Runs on **Core 1** with **maximum priority**
- Is **excluded from task watchdog** for uninterrupted timing
- Uses **polling SPI** (no DMA queuing jitter)
- Monitors **timing overruns** for diagnostics

### SPI Clock Speed

The DAC runs at **20 MHz SPI**, which is the maximum for MCP4822.
At 20 MHz, a 16-bit transaction takes ~1 µs per channel.

---

## Code Architecture

```
GalvoController (JSON/CAN interface)
        │
        ▼
HighSpeedScannerCore (scan engine)
        │
        ▼
    DAC_MCP4822 (hardware driver)
        │
        ▼
   MCP4822 DAC → Galvo Mirrors
```

### Thread Safety

- Configuration changes use a **mutex** (`config_mutex_`)
- Scanner task takes a **snapshot** of config at frame start
- Safe to change config while scanning (applied on next frame)

---

## Migration from SPIRenderer

If you were using `SPIRenderer` directly:

**Old code:**
```cpp
renderer->setParameters(X_MIN, X_MAX, Y_MIN, Y_MAX, STEP, dwell, frames);
renderer->setFastMode(true);
renderer->start();  // Blocking call!
```

**New code (via GalvoController):**
```cpp
ScanConfig cfg;
cfg.nx = (X_MAX - X_MIN) / STEP;
cfg.ny = (Y_MAX - Y_MIN) / STEP;
cfg.x_min = X_MIN * 4095 / 30000;
cfg.x_max = X_MAX * 4095 / 30000;
// ... set other params
GalvoController::setConfig(cfg);
GalvoController::startScan();  // Non-blocking (FreeRTOS task)
```

Or use the legacy JSON API which handles conversion automatically.

---

## Troubleshooting

### Scanner Won't Start

1. Check pins in `PinConfig.h` match hardware
2. Verify SPI bus not conflicting with other peripherals
3. Check log output for initialization errors

### Timing Overruns

If `timing_overruns` increases:
1. Reduce scan speed (increase `sample_period_us`)
2. Reduce complexity (fewer samples per line)
3. Ensure no other high-priority tasks on Core 1

### Triggers Not Working

1. Verify trigger pins are not used elsewhere
2. Check `enable_trigger` is set to 1
3. Use oscilloscope to verify pulse timing

---

## Files Reference

```
main/src/scanner/
├── GalvoController.h          # Main interface (updated)
├── GalvoController.cpp        # Implementation (rewritten)
├── GalvoController_old.cpp    # Backup of old implementation
├── HighSpeedScannerCore.h     # New scanner engine (added)
├── HighSpeedScannerCore.cpp   # Scanner implementation (added)
├── DAC_MCP4822.h              # DAC driver header (added)
├── DAC_MCP4822.cpp            # DAC driver implementation (added)
├── SPIRenderer.h              # Legacy renderer (kept)
├── SPIRenderer.cpp            # Legacy renderer (kept)
├── ScannerController.h        # Other scanner type
└── ScannerController.cpp      # Other scanner type
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 2.0 | 2025-01-27 | Migration to HighSpeedScannerCore, JSON+CAN support |
| 1.x | Previous | SPIRenderer-based implementation |

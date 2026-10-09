# UC2-ESP32 Firmware — Comprehensive Test Implementation Plan

## 1. Executive Summary

This document defines the implementation plan for a comprehensive test suite covering the UC2-ESP32 firmware. The plan covers infrastructure setup, test organization, implementation phases, mocking strategy, CI/CD integration, and documentation. The project currently has 2 existing Unity test files (`test/test_encoder_pcnt.cpp`, `test/test_stage_scan.cpp`) using PlatformIO's unit test runner with `UNIT_TEST` preprocessor guards.

## 2. Test Infrastructure Setup

### 2.1 PlatformIO Test Environment Configuration

PlatformIO's built-in unit testing (`pio test`) is the recommended approach for this ESP-IDF/Arduino framework project. It provides:

- Cross-platform test execution (Windows/Linux/macOS)
- Host-based testing (no ESP32 required for most unit tests)
- Unity test framework integration
- Automated reporting and coverage

#### Required Changes to `platformio.ini`

Add a dedicated test environment at the end of the file:

```ini
[env:test]
platform = espressif32@6.9.0
framework = espidf, arduino
board = esp32dev
test_build_src = false
test_framework = unity
test_flags =
    -DUNIT_TEST
    -DCORE_DEBUG_LEVEL=0
    -Wno-unused-variable
    -Wno-missing-field-initializers
lib_deps =
    https://github.com/adafruit/Adafruit_NeoPixel.git
    gin66/FastAccelStepper @ ^0.31.5
build_flags =
    -DUNIT_TEST
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCORE_DEBUG_LEVEL=0
    -Wno-format
    -Wno-unused-variable
    -Wmissing-field-initializers
    -Wno-attributes
test_ignore =
    test/test_encoder_pcnt.cpp
    test/test_stage_scan.cpp
```

> **Note:** The two existing test files are excluded from `test_ignore` because they depend on hardware-specific APIs that cannot be compiled in the host test environment. They will be addressed separately (see Section 4.2).

#### Alternative: Host-Based Test Environment

For tests that don't depend on ESP-IDF APIs, a host-based environment can be added:

```ini
[env:unit_test]
platform = native
test_framework = unity
test_flags =
    -DUNIT_TEST
    -Wno-unused-variable
    -Wno-missing-field-initializers
    -std=c++17
build_flags =
    -I.
    -Imain/src
    -Iinclude
test_build_src = false
```

### 2.2 Mocking Strategy for Hardware Dependencies

The firmware has significant hardware dependencies that must be abstracted for testing:

| Hardware Dependency | Mocking Approach | Affected Modules |
|---|---|---|
| `FastAccelStepper` / FocusMotor | Mock class with `MockFocusMotor` | `AxisController`, `AxisCalibration`, `StageScan` |
| ESP-IDF `pcnt_*` functions | Mock function pointers | `LinearEncoderController` |
| ESP-IDF `gpio_*` / `ledc_*` | Mock pin state array | `LaserController`, `LedController` |
| CANopenNode `CO_writeSDO` / `CO_readSDO` | Mock SDO table | `CANopenModule`, `DeviceRouter` |
| NVS key-value storage | In-memory NVS simulator | `NVSConfig` |
| FreeRTOS tasks / queues | Mock task scheduler | `CANopenModule`, `StageScan` |
| `millis()` / `esp_timer` | Mock clock | `CANopenModule` (timeouts), `StageScan` (delays) |
| Serial I/O | Mock stream | `SerialProtocol` |

#### Mocking Pattern

Use a combination of:
1. **Compile-time mocks** — `#ifdef UNIT_TEST` guards that replace hardware calls
2. **Runtime mock objects** — Test doubles passed via dependency injection
3. **Function pointer substitution** — Global function pointers replaced in tests

```cpp
// Example: Mock clock for deterministic timing tests
namespace TestClock {
    extern uint32_t currentMs;
    uint32_t mockMillis();
}

#define millis() TestClock::mockMillis()
```

### 2.3 Build System Integration

#### Directory Structure

```
test/
├── README                              # Test runner documentation
├── test_encoder_pcnt.cpp              # Existing: encoder PCNT tests (host-incompatible)
├── test_stage_scan.cpp                # Existing: stage scan JSON tests (host-incompatible)
├── test_serial_protocol.cpp           # Phase 1: SerialProtocol tests
├── test_motor_json_parser.cpp         # Phase 1: MotorJsonParser tests
├── test_axis_controller.cpp           # Phase 2: AxisController tests
├── test_axis_calibration.cpp          # Phase 2: AxisCalibration tests
├── test_canopen_module.cpp            # Phase 3: CANopenModule tests
├── test_device_router.cpp             # Phase 3: DeviceRouter tests
├── test_routing_table.cpp             # Phase 3: RoutingTable tests
├── test_laser_controller.cpp          # Phase 4: LaserController tests
├── test_led_controller.cpp            # Phase 4: LedController tests
├── test_nvs_config.cpp                # Phase 4: NVSConfig tests
├── test_integration_serial_motor.cpp  # Phase 5: Integration tests
├── test_integration_canopen.cpp       # Phase 5: Integration tests
├── test_performance.cpp               # Phase 5: Performance tests
├── mocks/
│   ├── mock_focus_motor.h             # MockFocusMotor declaration
│   ├── mock_focus_motor.cpp           # MockFocusMotor implementation
│   ├── mock_encoder.h                 # Mock encoder interface
│   ├── mock_canopen.h                 # Mock CANopenNode functions
│   ├── mock_nvs.h                     # In-memory NVS simulator
│   ├── mock_clock.h                   # Deterministic clock mock
│   └── mock_serial.h                  # Mock serial stream
└── helpers/
    ├── test_util.h                    # Common test utilities
    └── assert_json.h                  # JSON assertion helpers
```

#### PlatformIO Test Runner Commands

```bash
# Run all tests
pio test --environment test

# Run a specific test file
pio test --environment test --test-name test_serial_protocol

# Run with verbose output
pio test --environment test --verbose

# Run tests and show coverage
pio test --environment test --environment unit_test
```

## 3. Test Organization

### 3.1 File Structure

Each test file follows a consistent structure:

```
test/<module_name>.cpp
├── Includes (unity, headers, mocks)
├── Mock declarations
├── Test fixture / setup functions
├── Test teardown functions
├── Test cases (grouped by feature)
│   ├── Functional tests
│   ├── Edge-case tests
│   ├── Negative tests
│   └── Boundary tests
```

### 3.2 Test Naming Conventions

Tests follow the pattern: `test_<module>_<feature>_<condition>`

Examples:
- `test_serialProtocol_detectFormat_json`
- `test_serialProtocol_decode_malformedJson_returnsNull`
- `test_axisController_moveTo_validTarget_executes`
- `test_axisController_moveTo_overflow_doesNotCrash`
- `test_routingTable_buildDefault_motorLocal_only`

### 3.3 Setup/Teardown Patterns

```cpp
// Per-test setup
void setUp(void) {
    // Reset global state
    MockClock::reset();
    MockNvs::clear();
    // Re-initialize module under test
    SerialProtocol::reset();
}

// Per-test teardown
void tearDown(void) {
    // Clean up allocations
    // Reset mock state
}

// Suite-level setup (runs once)
void suiteSetUp(void) {
    UNITY_BEGIN();
}

// Suite-level teardown (runs once)
void suiteTearDown(void) {
    UNITY_END();
}
```

## 4. Implementation Phases

### Phase 1: Core Protocol Tests

**Scope:** SerialProtocol, MotorJsonParser

**Priority:** Highest — these modules have pure logic with minimal hardware dependencies

#### 4.1.1 SerialProtocol Tests (`test_serial_protocol.cpp`)

| Test ID | Description | Category |
|---|---|---|
| SP-001 | `detectFormat` with valid JSON header (0x4A) | Functional |
| SP-002 | `detectFormat` with valid MessagePack header (0x4D) | Functional |
| SP-003 | `detectFormat` with legacy `++\n` prefix | Functional |
| SP-004 | `detectFormat` with unknown header | Negative |
| SP-005 | `decode` valid JSON payload | Functional |
| SP-006 | `decode` valid MessagePack payload | Functional |
| SP-007 | `decode` truncated payload returns nullptr | Negative |
| SP-008 | `decode` oversized payload returns nullptr | Negative |
| SP-009 | `encode` JSON document to bytes | Functional |
| SP-010 | `encode` MessagePack document to bytes | Functional |
| SP-011 | `getMessageInfo` extracts length correctly | Functional |
| SP-012 | `isMessageComplete` with partial data | Edge-case |
| SP-013 | `isMessageComplete` with complete data | Functional |
| SP-014 | `isMessageComplete` with overflow length field | Edge-case |
| SP-015 | Legacy format parsing with `++\n...\n--\n` | Functional |
| SP-016 | Empty input handling | Negative |
| SP-017 | Null pointer input safety | Negative |
| SP-018 | MAX_MESSAGE_SIZE boundary (8192 bytes) | Edge-case |
| SP-019 | Format mismatch between header and payload | Negative |
| SP-020 | Throughput: 1000 JSON encode/decode iterations | Performance |

#### 4.1.2 MotorJsonParser Tests (`test_motor_json_parser.cpp`)

| Test ID | Description | Category |
|---|---|---|
| MJ-001 | `act` with valid motor action JSON | Functional |
| MJ-002 | `act` with missing `targetPosition` field | Negative |
| MJ-003 | `act` with invalid axis number | Negative |
| MJ-004 | `get` returns current motor state | Functional |
| MJ-005 | `parseEnableMotor` enables motor | Functional |
| MJ-006 | `parseEnableMotor` invalid axis | Negative |
| MJ-007 | `parseSetPosition` sets position | Functional |
| MJ-008 | `parseSetPosition` overflow handling | Edge-case |
| MJ-009 | `parseMotorPinDirection` sets direction | Functional |
| MJ-010 | `parseSetHardLimits` sets limits | Functional |
| MJ-011 | `parseSetHardLimits` invalid range | Negative |
| MJ-012 | `parseSetJoystickDirection` sets direction | Functional |
| MJ-013 | `parseMotorDriveJson` processes drive command | Functional |
| MJ-014 | `parseStageScan` with valid grid params | Functional |
| MJ-015 | `parseStageScan` with coordinate array | Functional |
| MJ-016 | `parseStageScan` with invalid coordinate count | Negative |
| MJ-017 | `addJsonFloatIfPresent` adds existing float | Functional |
| MJ-018 | `addJsonFloatIfPresent` skips missing key | Functional |
| MJ-019 | `addJsonIntIfPresent` adds existing int | Functional |
| MJ-020 | `addJsonIntIfPresent` skips missing key | Functional |

### Phase 2: Axis Control Tests

**Scope:** AxisController, AxisCalibration

**Dependencies:** MockFocusMotor, mock encoder backend

#### 4.2.1 AxisController Tests (`test_axis_controller.cpp`)

| Test ID | Description | Category |
|---|---|---|
| AC-001 | `moveTo` in OPEN_LOOP mode executes move | Functional |
| AC-002 | `moveTo` with target=0 is no-op | Functional |
| AC-003 | `moveTo` with INT32_MAX target handled safely | Edge-case |
| AC-004 | `moveTo` with INT32_MIN target handled safely | Edge-case |
| AC-005 | `moveTo` while already moving returns false | Negative |
| AC-006 | `moveTo` with faulted axis returns false | Negative |
| AC-007 | `stop` halts active motion | Functional |
| AC-008 | `stop` when not moving is safe | Functional |
| AC-009 | `resetAxis` with RESET_TRUST_ENCODER policy | Functional |
| AC-010 | `resetAxis` with RESET_TRUST_STEPS policy | Functional |
| AC-011 | `resetAxis` with RESET_FORCE_REHOME policy | Functional |
| AC-012 | `calibrateAxis` performs calibration sequence | Functional |
| AC-013 | `calibrateAxis` with stalled encoder detects fault | Negative |
| AC-014 | `calibrateAxis` timeout handling | Edge-case |
| AC-015 | Mode transition OPEN_LOOP → MONITOR | Functional |
| AC-016 | Mode transition MONITOR → CORRECT | Functional |
| AC-017 | Mode transition CORRECT → SERVO | Functional |
| AC-018 | Health transitions: OK → DEGRADED → FAULT | Functional |
| AC-019 | Fault latch prevents motion until reset | Negative |
| AC-020 | Encoder feedback integration with mock encoder | Integration |

#### 4.2.2 AxisCalibration Tests (`test_axis_calibration.cpp`)

| Test ID | Description | Category |
|---|---|---|
| ACal-001 | `calibrate` measures counts per step correctly | Functional |
| ACal-002 | `calibrate` with asymmetric step counts | Edge-case |
| ACal-003 | `calibrate` detects stall (encoder not moving) | Negative |
| ACal-004 | `calibrate` with encoder noise exceeds tolerance | Negative |
| ACal-005 | `countsToSteps` conversion accuracy | Functional |
| ACal-006 | `stepsToCounts` conversion accuracy | Functional |
| ACal-007 | `applyCalibration` updates calibration factors | Functional |
| ACal-008 | Stale calibration detection | Negative |
| ACal-009 | Position error calculation accuracy | Functional |
| ACal-010 | Boundary: zero-step calibration | Edge-case |

### Phase 3: CANopen Tests

**Scope:** CANopenModule, DeviceRouter, RoutingTable

**Dependencies:** Mock CANopenNode SDO functions, mock clock

#### 4.3.1 CANopenModule Tests (`test_canopen_module.cpp`)

| Test ID | Description | Category |
|---|---|---|
| CM-001 | `writeSDO` sends SDO write request | Functional |
| CM-002 | `writeSDO` with valid u8 value | Functional |
| CM-003 | `writeSDO` with valid u16 value | Functional |
| CM-004 | `writeSDO` with valid u32 value | Functional |
| CM-005 | `writeSDO` timeout returns false | Negative |
| CM-006 | `writeSDO` unreachable node returns false | Negative |
| CM-007 | `readSDO` retrieves value from slave | Functional |
| CM-008 | `readSDO` timeout returns false | Negative |
| CM-009 | `readSDO` buffer overflow protection | Negative |
| CM-010 | `isNodeReachable` returns true for recent TPDO | Functional |
| CM-011 | `isNodeReachable` returns false for stale node | Negative |
| CM-012 | `waitForNodeReachable` returns true on success | Functional |
| CM-013 | `waitForNodeReachable` returns false on timeout | Negative |
| CM-014 | `writeSDO_u8` typed helper works | Functional |
| CM-015 | `writeSDO_u16` typed helper works | Functional |
| CM-016 | `writeSDO_u32` typed helper works | Functional |
| CM-017 | OTA SDO write with extended timeout | Functional |
| CM-018 | SDO write with oversized data | Negative |
| CM-019 | Null pointer input safety | Negative |
| CM-020 | SDO write latency < 10ms (performance) | Performance |

#### 4.3.2 DeviceRouter Tests (`test_device_router.cpp`)

| Test ID | Description | Category |
|---|---|---|
| DR-001 | `routeCommand` motor_act with LOCAL route | Functional |
| DR-002 | `routeCommand` motor_act with REMOTE route | Functional |
| DR-003 | `routeCommand` motor_act with OFF route | Negative |
| DR-004 | `routeCommand` motor_get returns state | Functional |
| DR-005 | `routeCommand` laser_act with LOCAL route | Functional |
| DR-006 | `routeCommand` laser_act with REMOTE route | Functional |
| DR-007 | `routeCommand` laser_act with OFF route | Negative |
| DR-008 | `routeCommand` home_act with LOCAL route | Functional |
| DR-009 | `routeCommand` led_act with LOCAL route | Functional |
| DR-010 | `routeCommand` galvo_act with LOCAL route | Functional |
| DR-011 | `routeCommand` digital_out_act LOCAL | Functional |
| DR-012 | `routeCommand` digital_out_act REMOTE | Functional |
| DR-013 | `routeCommand` digital_in_get LOCAL | Functional |
| DR-014 | `routeCommand` gpio_act LOCAL | Functional |
| DR-015 | `routeCommand` gpio_get LOCAL | Functional |
| DR-016 | `routeCommand` i2c_act with node parameter | Functional |
| DR-017 | `routeCommand` unknown task returns nullptr | Negative |
| DR-018 | `routeCommand` null document safety | Negative |
| DR-019 | `routeCommand` malformed JSON safety | Negative |
| DR-020 | Node override forces REMOTE dispatch | Functional |

#### 4.3.3 RoutingTable Tests (`test_routing_table.cpp`)

| Test ID | Description | Category |
|---|---|---|
| RT-001 | `buildDefault` creates expected routes | Functional |
| RT-002 | `buildDefault` motor routes from pinConfig | Functional |
| RT-003 | `find` returns correct entry for motor | Functional |
| RT-004 | `find` returns nullptr for unknown type/id | Negative |
| RT-005 | `set` updates route correctly | Functional |
| RT-006 | `set` with OFF disables route | Functional |
| RT-007 | `set` with REMOTE sets nodeId | Functional |
| RT-008 | `applyNvsOverrides` merges NVS config | Functional |
| RT-009 | `toJson` exports full routing table | Functional |
| RT-010 | `logAll` does not crash | Functional |
| RT-011 | MAX_ROUTES boundary (64 entries) | Edge-case |
| RT-012 | Duplicate logicalId handling | Negative |
| RT-013 | Route type enum values correct | Functional |
| RT-014 | Route Where enum values correct | Functional |
| RT-015 | All RouteEntry::Type values tested | Functional |

### Phase 4: Hardware Controller Tests

**Scope:** LaserController, LedController, NVSConfig

**Dependencies:** Mock PWM/GPIO, mock NVS

#### 4.4.1 LaserController Tests (`test_laser_controller.cpp`)

| Test ID | Description | Category |
|---|---|---|
| LC-001 | `setLaserVal` valid laser ID 0-4 | Functional |
| LC-002 | `setLaserVal` laser ID 5-9 (hybrid mode) | Functional |
| LC-003 | `setLaserVal` out-of-range ID returns false | Negative |
| LC-004 | `setLaserVal` value clamped to PWM max | Edge-case |
| LC-005 | `setLaserVal` value 0 turns off laser | Functional |
| LC-006 | `setLaserVal` value = PWM max turns full on | Functional |
| LC-007 | `getLaserVal` returns current value | Functional |
| LC-008 | `getLaserPin` returns correct pin for ID | Functional |
| LC-009 | `getPWMChannel` returns correct channel | Functional |
| LC-010 | `setLaserVal` with despeckle params | Functional |
| LC-011 | `setLaserVal` despeckle period clamped | Edge-case |
| LC-012 | `getLaserVal` invalid ID returns 0 | Negative |
| LC-013 | PWM resolution boundary (8/10/12 bit) | Edge-case |
| LC-014 | MAX_LASERS boundary (10 lasers) | Edge-case |
| LC-015 | despeckle parameter validation | Negative |

#### 4.4.2 LedController Tests (`test_led_controller.cpp`)

| Test ID | Description | Category |
|---|---|---|
| LDC-001 | `setLedMode` ARRAY mode sets pixels | Functional |
| LDC-002 | `setLedMode` FILL mode sets all pixels | Functional |
| LDC-003 | `setLedMode` SINGLE mode sets one pixel | Functional |
| LDC-004 | `setLedMode` OFF clears all pixels | Functional |
| LDC-005 | `setStatus` sets status LED color | Functional |
| LDC-006 | `setStatus` warn (yellow) | Functional |
| LDC-007 | `setStatus` error (red) | Functional |
| LDC-008 | `setStatus` idle (green) | Functional |
| LDC-009 | `setStatus` success (green) | Functional |
| LDC-010 | `setStatus` busy (blue) | Functional |
| LDC-011 | `setStatus` rainbow mode | Functional |
| LDC-012 | `setLedCommand` with valid command | Functional |
| LDC-013 | `setLedCommand` with invalid mode | Negative |
| LDC-014 | Pixel index bounds checking | Edge-case |
| LDC-015 | LED array size boundary | Edge-case |

#### 4.4.3 NVSConfig Tests (`test_nvs_config.cpp`)

| Test ID | Description | Category |
|---|---|---|
| NVS-001 | `loadConfig` loads from NVS successfully | Functional |
| NVS-002 | `loadConfig` falls back to defaults when NVS empty | Functional |
| NVS-003 | `saveConfig` persists config to NVS | Functional |
| NVS-004 | `saveConfig` then `loadConfig` round-trip | Functional |
| NVS-005 | `resetConfig` clears NVS and reloads defaults | Functional |
| NVS-006 | `toJson` serializes config correctly | Functional |
| NVS-007 | `fromJson` merges partial JSON | Functional |
| NVS-008 | `fromJson` with null document is safe | Negative |
| NVS-009 | Routing table JSON export/import round-trip | Functional |
| NVS-010 | NVS key namespace collision safety | Negative |
| NVS-011 | Large config size handling | Edge-case |
| NVS-012 | Corrupted NVS data handling | Negative |

### Phase 5: Integration and Performance Tests

**Scope:** End-to-end flows, throughput, latency

#### 4.5.1 Integration Tests

| Test ID | Description | Category |
|---|---|---|
| INT-001 | Serial JSON → MotorJsonParser → AxisController flow | Integration |
| INT-002 | Serial MessagePack → MotorJsonParser → AxisController flow | Integration |
| INT-003 | CANopen SDO write → Remote slave state update | Integration |
| INT-004 | CANopen master → DeviceRouter → CANopenModule routing | Integration |
| INT-005 | Stage scan with coordinate-based positions | Integration |
| INT-006 | Stage scan with grid-based positions | Integration |
| INT-007 | Laser control via Serial → DeviceRouter → LaserController | Integration |
| INT-008 | LED control via Serial → DeviceRouter → LedController | Integration |
| INT-009 | NVS save → reboot → load consistency | Integration |
| INT-010 | Routing table rebuild after NVS override | Integration |

#### 4.5.2 Performance Tests

| Test ID | Description | Target |
|---|---|---|
| PERF-001 | SDO write latency (single write) | < 10ms |
| PERF-002 | SDO read latency (single read) | < 10ms |
| PERF-003 | JSON encode throughput (1000 iterations) | < 5s |
| PERF-004 | MessagePack encode throughput (1000 iterations) | < 3s |
| PERF-005 | JSON decode throughput (1000 iterations) | < 5s |
| PERF-006 | MessagePack decode throughput (1000 iterations) | < 3s |
| PERF-007 | MotorJsonParser act throughput (1000 iterations) | < 2s |
| PERF-008 | SerialProtocol getMessageInfo throughput (10000 iterations) | < 1s |
| PERF-009 | RoutingTable find latency (10000 lookups) | < 10ms |
| PERF-010 | NVS save+load round-trip | < 100ms |

## 5. Mocking Strategy Details

### 5.1 MockFocusMotor

```cpp
// mocks/mock_focus_motor.h
#pragma once
#include "motor/FocusMotor.h"

class MockFocusMotor {
public:
    static void reset();
    static void setMoveResult(bool success);
    static void setMoveDelayMs(uint32_t ms);
    static void simulateMoveComplete();
    
    // Track calls
    static uint32_t moveCount;
    static int32_t lastTarget;
    static bool lastEnable;
    
    // Replace actual FocusMotor methods in tests
    static bool mockMove(int32_t target);
    static void mockStop();
    static void mockEnable(bool enable);
    static bool mockIsRunning();
    static int32_t mockGetCurrentPosition();
};
```

### 5.2 Mock Clock

```cpp
// mocks/mock_clock.h
#pragma once
#include <cstdint>

namespace MockClock {
    extern uint32_t currentMs;
    void reset();
    void advanceMs(uint32_t ms);
    uint32_t getMs();
}

// Override millis() in tests
#ifdef UNIT_TEST
    #undef millis
    #define millis() MockClock::getMs()
#endif
```

### 5.3 Mock NVS

```cpp
// mocks/mock_nvs.h
#pragma once
#include <map>
#include <string>

namespace MockNvs {
    extern std::map<std::string, std::string> store;
    void clear();
    void set(const char* namespaceName, const char* key, const void* value, size_t len);
    bool get(const char* namespaceName, const char* key, void* value, size_t* len);
    bool erase(const char* namespaceName);
    bool eraseKey(const char* namespaceName, const char* key);
}
```

### 5.4 Mock CANopenNode

```cpp
// mocks/mock_canopen.h
#pragma once
#include <cstdint>
#include <vector>

struct MockSdoRequest {
    uint8_t nodeId;
    uint16_t index;
    uint8_t subIndex;
    std::vector<uint8_t> data;
};

namespace MockCanopen {
    extern std::vector<MockSdoRequest> receivedWrites;
    extern std::vector<MockSdoRequest> receivedReads;
    extern bool sdoWriteResult;
    extern bool sdoReadResult;
    extern std::vector<uint8_t> sdoReadResponse;
    extern bool nodeReachable;
    
    void reset();
    void setWriteResult(bool result);
    void setReadResult(bool result, const std::vector<uint8_t>& data);
    void setNodeReachable(bool reachable);
}
```

### 5.5 Mock Serial Stream

```cpp
// mocks/mock_serial.h
#pragma once
#include <Arduino.h>
#include <string>

class MockSerial : public Stream {
public:
    void addBytes(const uint8_t* data, size_t len);
    void clear();
    std::string getLastMessage();
    size_t available() override;
    int read() override;
    int peek() override;
    size_t write(uint8_t) override;
    void flush() override;
    
    static std::vector<uint8_t> receivedData;
};
```

### 5.6 FreeRTOS Task Simulation

For tests involving async operations:

```cpp
// mocks/mock_freertos.h
#pragma once
#include <functional>
#include <queue>

namespace MockFreeRTOS {
    extern std::queue<std::function<void()>> taskQueue;
    extern bool vTaskDelayCalled;
    extern uint32_t lastDelayTicks;
    
    void reset();
    void runNextTask();
    void runAllTasks();
    void simulateTaskCompletion();
}

// Override vTaskDelay in tests
#ifdef UNIT_TEST
    #undef vTaskDelay
    #define vTaskDelay(x) MockFreeRTOS::recordDelay(x)
#endif
```

## 6. CI/CD Integration

### 6.1 GitHub Actions Workflow

Create `.github/workflows/unit-tests.yml`:

```yaml
name: Unit Tests

on:
  push:
    branches: [main, develop]
  pull_request:
    branches: [main]
  workflow_dispatch:

jobs:
  test:
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        python-version: ['3.11']
    
    steps:
      - uses: actions/checkout@v4
      
      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: ${{ matrix.python-version }}
      
      - name: Install PlatformIO
        run: |
          python -m pip install --upgrade pip
          pip install --upgrade platformio
      
      - name: Build test environment
        run: pio pkg install && pio run -e test
      
      - name: Run unit tests
        run: pio test -e test --verbose
      
      - name: Upload test results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: test-results
          path: .pio/test/test-*/
  
  test-coverage:
    runs-on: ubuntu-latest
    needs: test
    steps:
      - uses: actions/checkout@v4
      
      - name: Set up Python
        uses: actions/setup-python@v5
        with:
          python-version: '3.11'
      
      - name: Install PlatformIO
        run: |
          python -m pip install --upgrade pip
          pip install --upgrade platformio
      
      - name: Run tests with coverage
        run: |
          pio test -e test --environment unit_test --verbose
          # Coverage report generation
          lcov --capture --directory .pio/build/test/
          lcov --report coverage.info
```

### 6.2 Test Reporting Standards

- **Pass rate target:** 100% for Phase 1-3, 90% for Phase 4-5
- **Coverage target:** 80% for core modules (SerialProtocol, MotorJsonParser, AxisController)
- **Failure classification:**
  - `FAIL`: Test assertion failed
  - `ERROR`: Test crashed or timed out
  - `SKIP`: Test skipped (hardware dependency not met)

### 6.3 Test Result Artifacts

PlatformIO test runner produces:
- XML reports in `.pio/test/<test-name>/test.log`
- Summary in `.pio/test/test-summary.txt`

These are uploaded as GitHub Actions artifacts for post-run analysis.

## 7. Documentation

### 7.1 Test Runner Usage

```bash
# Install PlatformIO if not already installed
pip install platformio

# Run all tests
pio test --environment test

# Run specific test file
pio test --environment test --test-name test_serial_protocol

# Run with verbose output
pio test --environment test --verbose

# Run tests and show output in real-time
pio test --environment test --print

# Clean test build artifacts
pio distclean
```

### 7.2 Adding New Tests

1. Create test file in `test/` directory following naming convention: `test_<module>.cpp`
2. Add includes for Unity and module headers
3. Implement `setUp()` and `tearDown()` if needed
4. Write test functions following `test_<module>_<feature>_<condition>` pattern
5. Add mock implementations in `test/mocks/` if required
6. Build and run: `pio test --environment test --test-name test_<module>`
7. Commit with test description in commit message

### 7.3 Debugging Test Failures

```bash
# Run single test with debug output
pio test --environment test --test-name test_serial_protocol --verbose

# Inspect test logs
cat .pio/test/test_serial_protocol/test.log

# Run with GDB (for crashes)
pio device monitor --print
```

### 7.4 Mock Development Guide

When adding tests that require new mocks:

1. Identify the hardware API being mocked (e.g., `pcnt_unit_get_count`)
2. Create mock header in `test/mocks/mock_<module>.h`
3. Implement mock in `test/mocks/mock_<module>.cpp`
4. Use `#ifdef UNIT_TEST` guards to swap implementations
5. Document mock behavior and limitations in header comments

## 8. Existing Tests Migration

The two existing test files have limitations:

### 8.1 `test_encoder_pcnt.cpp`

**Issues:**
- Depends on ESP-IDF `pcnt_*` APIs not available in host environment
- Uses direct `LinearEncoderController` static calls

**Migration Plan:**
- Split into host-compatible and hardware-specific tests
- Host-compatible tests: test `MotorData` flag handling, bounds checking
- Hardware tests: keep as integration tests requiring ESP32 target

### 8.2 `test_stage_scan.cpp`

**Issues:**
- Uses simplified mock structs instead of real types
- Depends on `cJSON` which is available in host environment

**Migration Plan:**
- Refactor to use real `StageScanningData` struct
- Keep as host-compatible test (cJSON available)
- Add to test runner after Phase 1 completion

## 9. Implementation Timeline

| Phase | Scope | Estimated Effort | Dependencies |
|---|---|---|---|
| Phase 0 | Infrastructure setup | 2-3 days | None |
| Phase 1 | Core protocol tests | 3-4 days | Phase 0 |
| Phase 2 | Axis control tests | 4-5 days | Phase 1, MockFocusMotor |
| Phase 3 | CANopen tests | 4-5 days | Phase 1, MockCanopen |
| Phase 4 | Hardware controller tests | 3-4 days | Phase 2, Mocks |
| Phase 5 | Integration & performance | 2-3 days | Phases 1-4 |
| Phase 6 | CI/CD & documentation | 1-2 days | All phases |

**Total estimated effort:** 19-26 days

## 10. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| PlatformIO test environment compatibility issues | Medium | Medium | Start with simple tests, iterate |
| Mock complexity for FreeRTOS-dependent code | High | Medium | Limit async testing, use deterministic mocks |
| Test maintenance burden as firmware evolves | High | Low | Keep tests focused on public APIs |
| CI/CD environment differences | Low | Medium | Test on same PlatformIO version locally |
| Existing tests incompatibility | Medium | Low | Migrate gradually, keep original files |

## 11. Success Criteria

1. **Phase 1 complete:** All 20 SerialProtocol tests + 20 MotorJsonParser tests passing
2. **Phase 2 complete:** All 20 AxisController tests + 10 AxisCalibration tests passing
3. **Phase 3 complete:** All 20 CANopenModule tests + 20 DeviceRouter tests + 15 RoutingTable tests passing
4. **Phase 4 complete:** All 15 LaserController tests + 15 LedController tests + 12 NVSConfig tests passing
5. **Phase 5 complete:** All 10 integration tests + 10 performance tests passing
6. **CI/CD:** Tests run automatically on every PR, results reported
7. **Documentation:** Test runner guide, adding tests guide, debugging guide complete

## 12. Appendix

### A. Key Type Definitions Reference

| Type | Values | Test Relevance |
|---|---|---|
| `AxisMode` | OPEN_LOOP(0), MONITOR(1), CORRECT(2), SERVO(3) | AC-015 to AC-017 |
| `AxisHealth` | HEALTH_OK(0), HEALTH_DEGRADED(1), HEALTH_FAULT(2) | AC-018 |
| `AxisFault` | FAULT_NONE(0) to FAULT_ENC_NOISE(7) | AC-013, AC-019 |
| `MessageFormat` | JSON(0x4A), MSGPACK(0x4D), BINARY(0x42), UNKNOWN(0x00) | SP-001 to SP-004 |
| `RouteEntry::Where` | LOCAL(0), REMOTE(1), OFF(2) | DR-001 to DR-003 |
| `RouteEntry::Type` | MOTOR(1), LASER, LED, GALVO, HOME, TMC, DAC, AIN, DIN | RT-001 to RT-015 |
| `CanOpenOtaStatus` | IDLE, RECEIVING, VERIFYING, COMPLETE, ERROR | CM-017 |

### B. PlatformIO Test Environment Notes

- Test environment uses `esp32dev` board for maximum API availability
- `test_build_src = false` prevents main source from being compiled into test binary
- Unity framework is included via PlatformIO package manager
- Tests run on host machine (x86_64), not on ESP32 target

### C. References

- [PlatformIO Unit Testing Documentation](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
- [Unity Test Framework](https://www.throwtheswitch.org/unity)
- [ESP-IDF Testing Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-tests.html)

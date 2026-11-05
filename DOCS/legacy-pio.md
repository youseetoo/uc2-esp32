# Legacy PlatformIO Build Instructions

The PlatformIO build system was retired in T6.4 (all 20 boards migrated to ESP-IDF). The old `platformio.ini` is preserved in `legacy/platformio.ini` for developers who need to build with PIO (e.g., for debugging or bisecting).

## How to build with PIO (legacy)

1. Copy the legacy ini to the repository root:
   ```bash
   cp legacy/platformio.ini platformio.ini
   ```

2. Install PlatformIO if not already installed:
   ```bash
   pip install platformio
   ```

3. Build a specific environment:
   ```bash
   pio run --environment <env_name>
   ```

   Example environments:
   - `UC2_2`, `UC2_3`, `UC2_4`, `UC2_WEMOS` — classic ESP32 BT boards
   - `seeed_xiao_esp32s3` — xiao_odmr
   - `UC2_canopen_slave_laser_release` — CANopen laser slave
   - `UC2_canopen_master_release` — CANopen master (all controllers)

4. After building, restore the clean state:
   ```bash
   rm platformio.ini
   ```

## Why PIO was retired

- D6 decision: single build system (IDF only) for CI simplicity
- All 20 boards verified building with ESP-IDF 6.1
- PIO envs had duplicate feature flag definitions that were error-prone
- IDF's `sdkconfig.defaults` provides cleaner per-board configuration

## Feature flags reference

The original PIO feature flags are documented in `DOCS/PORT_PLAN_STEPS/refs/env-app-mapping.md`. These have been converted to Kconfig symbols in each app's `apps/<board>/sdkconfig.defaults`.
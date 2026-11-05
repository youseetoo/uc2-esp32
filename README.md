# UC2-ESP32 Firmware

UC2-ESP32 is a multi-board ESP32 firmware platform for laser, LED, motor, and CANopen applications. All 20 boards build with a single ESP-IDF 6.1 toolchain.

## Building

Each board is a separate IDF app in `apps/<board>/`. To build:

```bash
# Source the ESP-IDF environment (D1: IDF 6.1)
. C:/esp/v6.1/esp-idf/export.ps1  # Windows
# or
source C:/esp/v6.1/esp-idf/export.sh  # Linux/macOS

# Build a specific board
cd apps/canopen_slave_laser
idf.py build merge-bin

# Or use the per-app build script
./build.sh
```

### Build matrix

All 20 boards are built via the `idf-build` CI job in `.github/workflows/build-and-release.yaml`. The board-to-app mapping is documented in `DOCS/PORT_PLAN_STEPS/refs/env-app-mapping.md`.

### Per-axis motor builds

The `canopen_slave_motor` board has 5 axis variants (`""`, `X`, `Y`, `Z`, `A`). These are built in the `build-frame.yaml` workflow using the `UC2_CAN_AXIS_ID` environment variable:

```bash
UC2_CAN_AXIS_ID=CAN_ID_MOT_X idf.py build merge-bin
```

## Legacy PlatformIO builds

The PlatformIO build system has been retired for all boards. The old `platformio.ini` is preserved in `legacy/platformio.ini` for reference. Developers who need to build with PIO (e.g., for debugging) should:

1. Copy `legacy/platformio.ini` to the repository root
2. Run `pio run --environment <env_name>`

See `DOCS/legacy-pio.md` for details.

## Documentation

- `DOCS/PORT_PLAN_STEPS/PORT_PLAN_STEPS.md` — Full port plan and execution steps
- `DOCS/baselines.md` — Size baselines for all boards
- `DOCS/cpp-leaves.md` — Remaining C++ leaf libraries (fastaccelstepper, tmc_stepper)
- `DOCS/bt.md` — Bluetooth stack documentation
- `DOCS/m5gfx.md` — M5GFX display driver documentation

## Testing

```bash
# Host tests (Unity suite)
make test

# CANopen codegen drift check
make canopen-check
```
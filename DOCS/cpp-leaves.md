# C++ Leaves (D2=(b) + D4)

Decision 2(b) confirmed 2026-10-08: the following vendor libraries remain C++
because C-ification is not worth the effort/risk. They are allowed as "leaves"
in the otherwise C firmware tree, accessed through C APIs or C++ islands.

Decision 4 (2026-11-04): the M5GFX display stack (m5unified/m5dial/m5gfx) is
added as a C++ leaf for the M5Stack Dial board, ported to ESP-IDF 6.1.

## tmc_stepper
- **Path:** `components/tmcstepper/`
- **Original:** https://github.com/teemuatlut/TMCStepper
- **Status:** C++ leaf, used through C++ island in uc2_motor (TMCController.cpp)
- **Rationale:** Complex register-based TMC motor driver API with C++ templates and inheritance. C-ification would be high-effort with minimal benefit.

## fastaccelstepper
- **Path:** `components/fastaccelstepper/`
- **Original:** https://github.com/gin66/FastAccelStepper
- **Status:** C++ leaf, used through C++ island in uc2_motor (FAccelStep.cpp)
- **Rationale:** Complex acceleration profile engine with FreeRTOS task integration and hardware timer backends (RMT/MCPWM/PCNT). C-ification would be high-effort with minimal benefit.

## accelstepper
- **Path:** `components/accelstepper/`
- **Original:** https://github.com/adafruit/AccelStepper
- **Status:** C++ leaf, used through C++ island in uc2_motor (AccelStep.cpp)
- **Rationale:** C-ification deferred in T5.1 (complex C++ class with inheritance). Used by xiao_odmr and canopen_slave_accelmotor boards.

## onewire_ds18b20
- **Path:** `components/onewire_ds18b20/`
- **Original:** https://github.com/milesburton/Arduino-Temperature-Control-Library
- **Status:** C++ leaf, used through C++ island in uc2_periph (DS18b20Controller.cpp)
- **Rationale:** C-ification deferred in T5.1 (51KB C++ lib with complex OneWire protocol implementation). Used by BT boards and xiao_odmr.

## neopixel
- **Path:** `components/neopixel/`
- **Original:** https://github.com/adafruit/Adafruit_NeoPixel
- **Status:** C++ leaf, used through C++ island in uc2_led (signal_neopixel_c.cpp)
- **Rationale:** C-ification deferred; NeoPixel protocol timing is complex and well-tested in Adafruit's implementation. Used by LED boards via the signal_neopixel_c.cpp wrapper.

## pscontroller
- **Path:** `components/pscontroller/`
- **Original:** https://github.com/MHeironimus/PS4Controller-Library
- **Status:** C++ leaf, used through C++ island in uc2_bt (BtController_psx.cpp)
- **Rationale:** Bluetooth HID controller library with complex state machine. Used by PS4/PS3 controller boards.

## m5unified / m5dial / m5gfx (D4)
- **Path:** `components/m5unified/`, `components/m5dial/`, `components/m5gfx/`
- **Original:** https://github.com/m5stack/M5Unified, M5Dial, M5GFX
- **Status:** C++ leaf, used for M5Stack Dial board display/encoder/RFID
- **Rationale:** M5's display stack is a tightly-integrated C++ library with template-heavy drawing API, hardware abstraction, and display driver integration. Porting to C would require rewriting the entire graphics layer. Used only by the m5stack_dial app.

## C++ Islands (first-party)
The following first-party files are C++ "islands" that wrap C++ leaves with C APIs:
- `components/uc2_motor/tmc/TMCController.cpp` — wraps tmc_stepper
- `components/uc2_motor/motor/FAccelStep.cpp` — wraps fastaccelstepper
- `components/uc2_motor/motor/AccelStep.cpp` — wraps accelstepper
- `components/uc2_periph/heat/DS18b20Controller.cpp` — wraps onewire_ds18b20
- `components/uc2_led/signal/signal_neopixel_c.cpp` — wraps neopixel
- `components/uc2_bt/BtController_psx.cpp` — wraps pscontroller
- `components/uc2_canopen/canopen/serial_dispatch_island.cpp` — CANopen serial dispatch

## Notes
- These leaves are accessed through the established T4.1 "C++ island" pattern:
  vendor C++ library wrapped with a pure C API (extern "C" functions).
- No C++ code is allowed outside these designated leaves and their C++ islands.
- The C-only compile gate (T4.7) checks that all `uc2_*` source files compile as C,
  with these leaves explicitly excluded.
- This document is the frozen inventory that T6.6 enforces.
- Several first-party files that were converted to C during Phase 1-5 still have .cpp extensions
  (e.g., uc2_app/main.cpp, uc2_hal/src/*.cpp). These are C code in .cpp files and will be
  renamed to .c in a future cleanup task.
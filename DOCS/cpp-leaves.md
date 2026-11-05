# C++ Leaves (D2=(b) + D4)

Decision 2(b) confirmed 2026-10-08: the following vendor libraries remain C++
because C-ification is not worth the effort/risk. They are allowed as "leaves"
in the otherwise C firmware tree, accessed through C APIs or C++ islands.

Decision 4 (2026-11-04): the M5GFX display stack (m5unified/m5dial/m5gfx) is
added as a C++ leaf for the M5Stack Dial board, ported to ESP-IDF 6.1.

## tmc_stepper
- **Path:** `components/tmcstepper/`
- **Original:** https://github.com/teemuatlut/TMCStepper
- **Status:** C++ leaf, used through C++ island in uc2_motor
- **Rationale:** Complex register-based TMC motor driver API with C++ templates and inheritance. C-ification would be high-effort with minimal benefit.

## fastaccelstepper
- **Path:** `components/fastaccelstepper/`
- **Original:** https://github.com/gin66/FastAccelStepper
- **Status:** C++ leaf, used through C++ island in uc2_motor/FAccelStep.cpp
- **Rationale:** Complex acceleration profile engine with FreeRTOS task integration and hardware timer backends (RMT/MCPWM/PCNT). C-ification would be high-effort with minimal benefit.

## m5unified / m5dial / m5gfx (D4)
- **Path:** `components/m5unified/`, `components/m5dial/`, `components/m5gfx/`
- **Original:** https://github.com/m5stack/M5Unified, M5Dial, M5GFX
- **Status:** C++ leaf, used for M5Stack Dial board display/encoder/RFID
- **Rationale:** M5's display stack is a tightly-integrated C++ library with template-heavy drawing API, hardware abstraction, and display driver integration. Porting to C would require rewriting the entire graphics layer. Used only by the m5stack_dial app.

## Notes
- These leaves are accessed through the established T4.1 "C++ island" pattern:
  vendor C++ library wrapped with a pure C API (extern "C" functions).
- No C++ code is allowed outside these designated leaves and their C++ islands.
- The C-only compile gate (T4.7) checks that all `uc2_*` source files compile as C,
  with these leaves explicitly excluded.
- This document is the frozen inventory that T6.6 enforces.

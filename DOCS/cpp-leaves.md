# C++ Leaves (D2=(b))

Decision 2(b) confirmed 2026-10-08: the following vendor libraries remain C++
because C-ification is not worth the effort/risk. They are allowed as "leaves"
in the otherwise C firmware tree, accessed through C APIs or C++ islands.

## tmc_stepper
- **Path:** `components/tmc_stepper/`
- **Original:** https://github.com/teemuatlut/TMCStepper
- **Status:** C++ leaf, used through C++ island in uc2_motor
- **Rationale:** Complex register-based TMC motor driver API with C++ templates and inheritance. C-ification would be high-effort with minimal benefit.

## fastaccelstepper
- **Path:** `components/fastaccelstepper/`
- **Original:** https://github.com/gin66/FastAccelStepper
- **Status:** C++ leaf, used through C++ island in uc2_motor/FAccelStep.cpp
- **Rationale:** Complex acceleration profile engine with FreeRTOS task integration and hardware timer backends (RMT/MCPWM/PCNT). C-ification would be high-effort with minimal benefit.

## Notes
- These leaves are accessed through the established T4.1 "C++ island" pattern:
  vendor C++ library wrapped with a pure C API (extern "C" functions).
- No C++ code is allowed outside these designated leaves and their C++ islands.
- The C-only compile gate (T4.7) checks that all `uc2_*` source files compile as C,
  with these leaves explicitly excluded.
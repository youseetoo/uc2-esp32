# M5 graphics leaf — port notes (M5GFX / M5Unified / M5Dial)

T5.4: the M5Stack Dial + Waveshare 7" boards use the M5Stack vendor graphics stack
(M5GFX + M5Unified + M5Dial). Rather than re-port that stack by hand, it is **vendored as
IDF components** (a C++ "leaf" — see the D2 decision: C core + C++ leaf set behind C
backend headers). This page records what was vendored, the versions, and the specific
fixes that made it build green on raw ESP-IDF (no PlatformIO).

## 1. Components + versions

| Component | Dir | Source (PIO) | Version | What it is |
|-----------|-----|--------------|---------|------------|
| `m5gfx` | `components/m5gfx` | `m5stack/M5GFX` | unversioned (in `platformio.ini` `lib_ignore`); vendored as a specific checkout | The LGFX-based graphics library (the big one: `M5GFX.cpp`, `lgfx/v1/*`, font tables, miniz). |
| `m5unified` | `components/m5unified` | `m5stack/M5Unified` | unversioned (in `lib_ignore`) | M5Stack board-agnostic device API. Single TU: `src/M5Unified.cpp` includes every `*.inl`. |
| `m5dial` | `components/m5dial` | `m5stack/M5Dial` | unversioned (in `lib_ignore`) | The M5Stack **Dial** board API (thin wrapper over M5Unified + M5GFX), plus its vendored `Encoder` (PCNT) + `MFRC522` (NFC) utilities. |
| `esp32_encoder` | `components/esp32_encoder` | `madhephaestus/ESP32Encoder` | `^0.12.0` (pinned) | Standalone PCNT-backed linear encoder driver (`src/ESP32Encoder.cpp` only — `InterruptEncoder.cpp` is dead/self-referenced and dropped). |

Component dependency edges (each `idf_component_register` REQUIRES):

- `esp32_encoder` → `esp_driver_pcnt`, `esp_driver_gpio`, `esp_common`, (PRIV) `freertos`.
- `m5dial` → `m5unified`, `m5gfx`, `uc2_hal`. Compiles **only** `src/M5Dial.cpp`,
  `src/utility/Encoder.cpp`, `src/utility/MFRC522.cpp` — the M5Dial **SdFat / PS4 /
  SD / speaker** utilities are intentionally excluded (unused on the dial).
- `m5unified` → `m5gfx`, `esp_adc`, `driver`, `esp_driver_gpio`, `esp_driver_i2s`,
  `esp_driver_rmt`, `esp_driver_ledc`, `esp_driver_touch_sens` (the IDF 6.x set).
- `m5gfx` → (large; functional hubs share header parsing, data tables stay in separate
  TUs so the linker can discard unused groups).

The M5 components are registered in every app via `EXTRA_COMPONENT_DIRS` (the shared
`components/` dir), and each app's top-level `CMakeLists.txt` adds `m5dial m5unified
m5gfx esp32_encoder` to the `--start-group` link wrapper (see §4).

## 2. The `Encoder.h` IDF-native conversion

The M5Dial dial encoder is the `ESP32Encoder` library's `Encoder` class, which reads
its two pins through **direct GPIO register access** and, in the upstream version,
falls back to the Arduino `WProgram.h` register helpers. On raw IDF that fallback does
not exist, so `components/m5dial/src/utility/Encoder.h` was rewritten **IDF-native**:

- Dropped the `WProgram.h` / Arduino fallback entirely.
- The register shims now resolve from `soc/gpio_reg.h`: `IO_REG_TYPE`,
  `PIN_TO_BASEREG`, `PIN_TO_BITMASK`, `DIRECT_PIN_READ` are built on
  `GPIO_IN_REG` (note: on IDF 6.1 `GPIO_IN_REG` is an **rvalue address constant**,
  not a variable — use it directly, do not `&` it).
- `portInputRegister(port)`, `digitalPinToPort(pin)`, `digitalPinToBitMask(pin)`
  helpers were added. **`digitalPinToPort` returns `0` (not `GPIO_NUM_GPIO`)** so
  `portInputRegister` indexes the single shared input-register bank.
- `CORE_NUM_INTERRUPT` shim for the ISR pinning.

`esp32_encoder` (the separate linear-encoder component) additionally had to move off
the **legacy `driver/pcnt.h`** (removed in IDF 6) to the new **`esp_driver_pcnt`**
(`driver/pulse_cnt.h`, handle-based API).

## 3. The `Arduino.h` + `Wire.h` shims (for the vendored MFRC522 leaf)

`M5_Dial` owns an `MFRC522 Rfid` member (`M5Dial.h`), and `MFRC522.{h,cpp}`
unconditionally `#include <Arduino.h>` / `<Wire.h>`. The dial firmware never drives
the RFID (only the PCNT encoder + the M5GFX display are used), so two **minimal shims**
satisfy the preprocessor + the handful of Arduino symbols the leaf references:

- **`components/m5dial/src/Arduino.h`** — includes `uc2_hal.h` + `<stdint.h>` and
  provides: `typedef uc2::Serial_ Stream;` / `typedef uc2::Serial_ HardwareSerial;`,
  `typedef unsigned int word;` (MFRC522 uses `word` as a loop counter),
  `PROGMEM`/`PSTR(x)` no-ops, and an empty `struct __FlashStringHelper {}` + `F(x)`
  (the leaf only *holds/returns* `F(...)` pointers, never dereferences them).
  This shim is a **strict superset** of `tmcstepper/include/Arduino.h`: a TMC
  translation unit (`uc2_motor/TMCController.cpp -> TMCStepper.h -> <Arduino.h>`) can
  resolve `<Arduino.h>` to *this* file instead of tmcstepper's, and `Stream` still
  resolves either way (both map it to `uc2::Serial_`).
- **`components/m5dial/src/Wire.h`** — `using TwoWire = uc2::Wire_;` and **nothing
  else**. The real `Wire` global is *already* declared by `uc2_hal` (`uc2_wire.h`:
  `extern uc2::Wire_ Wire;`); this shim must **not** redeclare it (a second
  `inline TwoWire Wire;` produced `conflicting declaration 'TwoWire Wire'` — two
  differently-typed global `Wire` symbols).

Two small `uc2_hal` additions were needed for the leaf to compile:

- `Serial_` gained the 2-arg base overloads `print(int/long/unsigned long, int base)`
  for `Serial.print(v, HEX)` (the leaf's debug prints).
- (The `word` typedef lives in the shim above, not in `uc2_hal`.)

## 4. The `--start-group` link fix (app `CMakeLists.txt`)

Every app wraps its project component libs in one `--start-group … --end-group` on
`__idf_main` (the `uc2_*` components form a dense cyclic `REQUIRES` web — see the T3.3
comment in the app `CMakeLists`). When a feature is off, a component can register with
**no sources** (e.g. `neopixel` when `UC2_LED_CONTROLLER` is off registers
`SRCS ""`), which still creates a CMake target but produces **no** `lib<comp>.a`.
Listing it in the group then fails the link with `cannot find -lneopixel`
(`cannot find libneopixel.a`). The fix in the app `CMakeLists.txt`:

```cmake
if(TARGET __idf_${_uc2_c})
    get_target_property(_uc2_csrcs __idf_${_uc2_c} SOURCES)
    if(_uc2_csrcs)   # skip feature-off components that register with no sources
        list(APPEND _uc2_group_libs "LINKER:${CMAKE_BINARY_DIR}/esp-idf/${_uc2_c}/lib${_uc2_c}.a")
    endif()
endif()
```

## 5. Include structure

The M5 headers reach every component through the shared `uc2_modinc.cmake` include
list (`components/m5unified/src`, `components/m5dial/src`, `components/m5gfx/src`),
listed **after** the ported `uc2_*` module dirs and the `main/src` shadow dirs. The
per-app `PinConfig.h` shadow works as before: the app's top-level `CMakeLists.txt` sets
`ENV{UC2_APP_MAIN_DIR}` to its own `main/`, which `uc2_modinc.cmake` **prepends** so
the app's `PinConfig.h` shadows the shared one. `DialController.h` (in `uc2_dial`)
includes `M5Unified.hpp` + `M5Dial.h` under `#ifdef M5DIAL`, so non-dial TUs never pull
the M5 header tree in.

## 6. Build status

- **`apps/m5stack_dial` — GREEN.** `idf.py build` → `m5stack_dial.bin` = `0x93ae0`
  (591 KB); 54 % free in the `0x140000` app partition; target `esp32s3`, 8 MB flash,
  USB-Serial-JTAG console. It is the **first M5GFX board** to build green.
- **`apps/waveshare_d7`** — the PIO env `waveshare_esp32s3_7inchScreen` is actually a
  **CAN motor node** (X-axis TMC slave), *not* an M5GFX display board; it was ported as
  a MOTOR + FAS + CAN + CANopen node (see the T5.4 step note), not via this stack.
- **`m5stack_dial7`** — a PIO env that inherits from `m5stack_dial`, but it is **not in
  the repo** as an app (not ported; optional/D4).

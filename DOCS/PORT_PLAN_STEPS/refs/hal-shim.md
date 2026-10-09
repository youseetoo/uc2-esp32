# Ref: `uc2_hal` shim API surface + leaf-first port order

Index: `../PORT_PLAN_STEPS.md`. Owner step: T1.3.

## 1. Goal

Let existing `.cpp` files compile with **minimal edits** (Phases 1–3 stay C++).
`components/uc2_hal/` = C core (`hal.c`) + a C/C++-compatible header `uc2_hal.h` that also
provides the C++ façade objects (`Serial`, `Wire`) while C++ code is still in place. The
shim is deleted module-by-module as Phase 4 C-ifies (direct IDF calls replace it).

## 2. API table (Arduino → IDF)

| Arduino API (as used in `main/`) | IDF implementation | Notes |
|---|---|---|
| `millis()` | `esp_timer_get_time() / 1000` | monotonic µs→ms |
| `micros()` | `esp_timer_get_time()` | |
| `delay(ms)` | `vTaskDelay(pdMS_TO_TICKS(ms))` | |
| `delayMicroseconds(us)` | `esp_delay_us(us)` | IDF ≥5.4; fine for D1=5.5 too |
| `pinMode(p, mode)` | `gpio_set_direction()` (OUTPUT/INPUT/INPUT_PULLUP) | validate pin ≤ 48 (S3) / 39 (ESP32) |
| `digitalWrite(p, v)` / `digitalRead(p)` | `gpio_set_level()` / `gpio_get_level()` | |
| `attachInterrupt(p, fn, edge)` / `detachInterrupt(p)` | `gpio_set_intr_type()` + `gpio_isr_add()` + shared `esp_intr_alloc()` table; detach = `gpio_set_intr_type(NONE)` + free | internal per-pin user-callback table |
| `noInterrupts()` / `Interrupts()` | `portDISABLE_INTERRUPTS()` / `portENABLE_INTERRUPTS()` | |
| `analogRead(p)` | `adc1_config_channel_atten()` + `adc1_get_raw()` (+ `adc_cali` where code expects mV) | S3: adc1 only |
| `analogWrite(p, d)` | LEDC: `ledc_timer_config()` + `ledc_channel_config()` + `ledc_set_duty/update_duty()` (8-bit) | 2 timers, 16 channels |
| `touchRead`/`touchWrite` (ESP32 only) | `touch_pad_start/read` | only classic-ESP32 apps |
| `Serial.begin(baud)` / `write/println/printf/read/available/flush` | C++ `Serial` façade over IDF UART driver v2 on the **console UART** (classic ESP32: UART1; S3: USB-Serial-JTAG console) | `uart_driver_install(256,0,0)`; `println(x)` → `printf("%s\n")` overloads; keep 115200 default |
| `Wire.begin(addr)` / `beginTransmission/endTransmission/write/read/requestFrom` | C++ `Wire` façade over **I2C master v2** (`i2c_master_bus_handle_t`, `i2c_master_transmit/receive`), multi-device | 9 files use Wire; T2.x steps may later migrate to the `uc2_i2c` helper, but v1 = drop-in |
| `random(n)` | `esp_random() % n` | |
| `constrain(v,a,b)` / `min` / `max` / `abs` | `static inline` macros | watch `std::min/max` collisions |
| `log_i/log_w/log_e(tag,msg,…)` | `ESP_LOGI/W/E("tag", …)` | Arduino-ESP32 `log_*` → ESP_LOG |
| `String` (if present) | small `uc2::String` (fixed-heap buffer + append/concat/c_str) | **T1.3 step 1: grep `String ` usage count; if 0, skip** |
| `ESP.restart()` | `esp_restart()` | |
| `yield()` | `vTaskDelay(1)` (no-op equivalent) | |
| `setup()`/`loop()` | `uc2_app_start()` in `uc2_app` (loop → FreeRTOS task) | |

Shim unit-test hook: every function is a thin wrapper → the 168-test suite (T0.5) can run
it on the host with IDF drivers mocked (same mocks the suite already carries for TWAI).

## 3. Per-module port order (leaf-first, with dependency arrows)

```
uc2_hal (T1.3)
  └─► uc2_core [state, config, qid, message, buzzer] (T1.6)
        └─► uc2_serial [SerialProcess, SerialProtocol, Endpoints] (T1.6)
              └─► leaf controllers, any order within Phase 2:
                    uc2_io ← uc2_core
                    uc2_led (Ws2812Rmt) ← uc2_core, uc2_io        (T2.2/T2.4)
                    uc2_periph (i2c/tca/dac/tmp102/thermal/fan/heat) ← uc2_core  (T2.3–T2.5)
                    uc2_scanner ← uc2_core                         (T2.5)
                    uc2_motor ← uc2_io (encoder), vendor leaves    (T2.6/T2.7)
                    uc2_canopen ← uc2_core, canopen_node, uc2_od, cjson   (T1.6 laser subset → T2.x full)
                    uc2_joystick ← uc2_canopen, esp_usb_host_hid   (T2.10)
  Phase 3: uc2_bt ← uc2_core (Phase 2 done)                        (T3.3–T3.6)
  Phase 4: C-ify bottom-up in the reverse dependency order above:
           pid/axis/signal/state/config → uc2_core/uc2_serial → uc2_motor →
           uc2_canopen (split) → uc2_bt/remaining → uc2_app glue   (T4.1–T4.6)
```

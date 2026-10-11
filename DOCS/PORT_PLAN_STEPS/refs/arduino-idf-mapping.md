# Ref: Arduino → IDF API mapping + leaf-first conversion order

Index: `../PORT_PLAN_STEPS.md`. Owner step: T1.3.

## 1. Goal

Convert the `main/` code to direct IDF (no Arduino APIs, no shim). The table below maps
each Arduino API used in `main/` to its IDF equivalent. The code is converted
module-by-module (leaf-first order in §3); no shim layer is introduced.

## 2. API table (Arduino → IDF)

Site counts verified by grep audit of `main/` (branch `port/idf-c`, T1.3 action 1, 2026-07-09).
"0 sites" rows are kept for completeness but need no façade.

| Arduino API (as used in `main/`) | IDF implementation | Sites | Notes |
|---|---|---|---|
| `millis()` / `micros()` | `esp_timer_get_time() / 1000` / `esp_timer_get_time()` | 152 / 10 | monotonic µs→ms |
| `delay(ms)` / `delayMicroseconds(us)` | `vTaskDelay(pdMS_TO_TICKS(ms))` / `esp_delay_us(us)` | 53 / 8 | |
| `pinMode(p, mode)` | `gpio_set_direction()` (OUTPUT/INPUT/INPUT_PULLUP) | 26 | validate pin ≤ 48 (S3) / 39 (ESP32) |
| `digitalWrite(p, v)` / `digitalRead(p)` | `gpio_set_level()` / `gpio_get_level()` | 44 / 8 | |
| `attachInterrupt(p, fn, edge)` / `detachInterrupt` / `digitalPinToInterrupt` | `gpio_set_intr_type()` + `gpio_isr_add()` + shared `esp_intr_alloc()`; detach = `gpio_set_intr_type(NONE)` | **1** / 0 / 1 | single site: `FanController.cpp:162` FAN_TACHO_PIN, `onTachoEdge`, edge **FALLING** → the per-pin table needs exactly one (FALLING) entry |
| `noInterrupts()` / `Interrupts()` | `portDISABLE_INTERRUPTS()` / `portENABLE_INTERRUPTS()` | 1 / 1 | |
| `analogRead(p)` / `analogReadMilliVolts(p)` | `adc1_config_channel_atten()` + `adc1_get_raw()` (+ `adc_cali` where mV expected) | 6 files (5 + 1) | S3: adc1 only; `analogReadMilliVolts` only in ThermalController → `adc_cali_raw_to_mv()` |
| `analogWrite(p, d)` | LEDC | **0** | unused — Ws2812Rmt/LED PWM already call `rmt*` (30) / `ledc*` (36) IDF drivers directly |
| `touchRead`/`touchWrite` | `touch_pad_start/read` | **0** | unused in `main/` |
| `Serial.begin/write/print/println/printf/read/readBytes/available/flush` + `setTimeout/setRxBufferSize/setTxBufferSize/setTxTimeoutMs/setDebugOutput` | C++ `Serial` façade over IDF UART driver v2 on the **console UART** (classic ESP32: UART1; S3: USB-Serial-JTAG console) | 131 (print 45, println 81, write 3, read 2, readBytes 1, available 5, printf 3, flush 1, begin 4, setTimeout 2, setRx 1, setTx 2, setDebug 1*) | *`setDebugOutput` commented out. `uart_driver_install(8192, 2048, 0)`; `setTimeout(ms)` → `uart_wait_tx_done`/read timeout |
| `Serial1` (TMC UART) | second `Serial` instance over UART2 driver: `begin(115200, SERIAL_8N1, rx, tx)` | 2 (`TMCController.cpp:500`, `stepper_wrapper.cpp:26`) + `PinConfig.h` `#define SERIAL_PORT Serial1` (8 board headers) | `SERIAL_8N1` constant in the façade |
| `Wire.begin(beginTransmission/endTransmission/write/read/available/requestFrom` | C++ `Wire` façade over **I2C master v2** (`i2c_master_bus_handle_t`, `i2c_master_transmit/receive`), multi-device | 114 call sites in **12 files** (i2c_master 56, I2cBridge 21, Tmp102 14, Fan 7, i2c_slave_dial 7, main 3, tca_controller 2, CANopenModule 1, GpioCanSlave 1, FocusMotor 1, TMCController.h 1, I2cBridge.h 1) | T2.x steps may later migrate to the `uc2_i2c` helper, but v1 = drop-in |
| `Wire.onReceive(fn)` / `Wire.onRequest(fn)` | I2C **slave** driver (`i2c_slave_init` + events) | 2 (`i2c_slave_dial.cpp:82-83`) | dial acts as I2C slave; callback dispatch in the façade |
| `random(n)` / `constrain(v,a,b)` / `min`/`max`/`abs` / `map` | `esp_random() % n` / `static inline` macros | 3 / 7 / — / 3 | watch `std::min/max` collisions |
| `log_d/log_i/log_w/log_e(tag,msg,…)` | `ESP_LOGD/I/W/E("tag", …)` | 90 / 591 / 110 / 175 = 966 | **all 4 levels used** |
| `String` | `uc2::String` façade: ctors `()`/`(const char*)`/`(int)`/`(int, base)` (DEC/HEX), `operator+` (String+String, `const char*`+String, String+`const char*`), `c_str()`, `toInt()`, `toLowerCase()` (in-place) | **~70 lines in 13 files**: FocusMotor 35, State 3, HomeMotor 6, MessageController 5, DialController 3, MotorJsonParser 2, LedController 4, PidController 1, ConfigController 1, AxisCalibration 1, JoystickRouter 2, ImageController 2, main 2 | dominant pattern: NVS keys `("motor" + String(axis)).c_str()`; `String(mac[i], HEX)` numeric ctor in State. (The earlier "15 sites" count missed FocusMotor.) |
| `ESP.restart()` / `ESP.getFreeHeap()` / `ESP.getCycleCount()` | `esp_restart()` / `esp_get_free_heap_size()` / `esp_timer_get_time()` | 8 / 7 / 4 | only these 3 used (State, NVSConfig, Ws2812Rmt); a cycle-accurate RMT port (T2.x) may switch to `esp_cpu_get_cyclecount()` |
| `Preferences` | `nvs_open`/`get_*`/`set_*`/`close` wrapper: `begin(name, ro)`, `get {String,Int,Bool,Float,Char}(key[,def])`, `put {Int,Bool,Float,Char}(key,val)`, `end()` | 237 calls in **23 files** | namespaces in use: `"UC2"` (R+W, ConfigController), `"boot_prefs"` (main), `"fan"`, `"heat"`, `"home"` (MessageController), `"tmc"` (TMCController), `"thermal"` (ThermalController) — the earlier "nvsconfig/uc2_prefs/uc2_state" note was wrong. FocusMotor 43, TMCController 40 are the heavy users |
| `WiFi.softAP(ssid)` / `softAPIP()` / `softAPdisconnect(wifiOff)` / `disconnect(wifiOff)` / `IPAddress` | `esp_wifi_init/set_mode(AP)/set_config/start`, `esp_wifi_get_ip_info(WIFI_IF_AP)` | 5 (`State.cpp`) | Phase 3 (T3.x, with uc2_bt) |
| `ArduinoOTA.onStart/onEnd/onProgress/getCommand` / `U_FLASH` | IDF **HTTP OTA** (`esp_https_ota` + a small server) or ESP-IDF web server; `U_FLASH` = full-image OTA | 6 (`State.cpp`) | Phase 3; the OTA *receive* path (`OtaBinaryReceive`) is serial-based and separate |
| `ESP32Encoder` | GPIO ISR + quadrature A/B decode wrapper | 2 includes (`encoder_wrapper.cpp`, `DialController.cpp`) | vendor leaf, T2.6/T2.7 |
| `FastAccelStepper` (`TMCSerial.begin(baud, SERIAL_8N1, rx, tx)`) | keep vendored; its `Serial` subclass maps to the Serial1 façade | 1 include (`stepper_wrapper.cpp`) | vendor leaf, T2.6/T2.7 |
| Arduino BLE API (`BLERemoteService`/`Characteristic`, `ble.*`/`ble_security.*`) | esp_bt (BLE/GATT) or NimBLE directly | BtController.cpp | Phase 3 (T3.3–T3.6); no `BLEDevice.h` include — classes arrive via `Arduino.h` today |
| `MDNS.begin("esp32")` | `mdns_init()` + `mdns_hostname()` (esp_netif mdns) | 1 (`State.cpp:309`) | Phase 3 |
| `yield()` | `vTaskDelay(1)` | **0** | |
| `setup()`/`loop()` | `uc2_app_start()` in `uc2_app` (loop → FreeRTOS task) | `main.cpp` | |

Unit-test hook: converted functions are thin wrappers → the 168-test suite (T0.5) can run
them on the host with IDF drivers mocked (same mocks the suite already carries for TWAI).

## 3. Per-module conversion order (leaf-first, with dependency arrows)

```
T1.3: convert main/ to direct IDF (no shim)
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

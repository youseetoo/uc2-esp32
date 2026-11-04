// T4.4 4h: GpioCanSlave — faithful C port of the former GpioCanSlave.cpp
// (behavior-identical; the C++ `namespace GpioCanSlave` becomes the flat
// `gpio_can_slave_*` C API declared in GpioCanSlave.h).
//
// C conversions (no behavior change):
//   * NVS persistence via nvs.h directly (was the C++ uc2::Preferences wrapper)
//   * JSON via the cJsonTool C helpers (was cJsonTool::)
//   * vendor CANopen headers included plainly (no extern "C" wrapper needed)
//
// Compiled only when GPIO_CAN_SLAVE_CONTROLLER is defined (see
// apps/canopen_slave_gpio/main/PinConfig.h).
#include "GpioCanSlave.h"
#include "PinConfig.h"
#ifdef GPIO_CAN_SLAVE_CONTROLLER

#include <string.h>
#include "uc2_hal.h"
#include "esp_timer.h"
#include "nvs.h"
#include "DigitalOutController.h"
#ifdef I2C_BRIDGE_CONTROLLER
#include "I2cBridge.h"
#endif
#include "cJsonTool.h"

#ifdef CAN_CONTROLLER_CANOPEN
#include "CANopenModule.h"  // provides extern CO_t* CO
#include <CANopen.h>
#include "OD.h"
#endif

static const char *TAG = "GpioCanSlave";

static const char* NVS_NS       = "uc2";
static const char* NVS_KEY_THR  = "gpioCT";
static const char* NVS_KEY_REF  = "gpioCR";
static const char* NVS_KEY_SENS = "gpioCS";
static const char* NVS_KEY_MODE = "gpioCMx  ";

// Detection modes.
//   AUTO   — adaptive baseline + robust noise scale (σ) + z-score vote.
//            Parameter-free: tracks a slowly drifting background and trips
//            on a fast deflection that exceeds K·σ. This is the default.
//   MANUAL — fixed reference ± threshold with a consecutive-sample vote.
//            Deterministic; needs the reference calibrated to the idle level.
enum { MODE_AUTO = 0, MODE_MANUAL = 1 };

// ── Collision-detector config (NVS-backed) ─────────────────────────
static uint8_t  s_mode        = MODE_AUTO;
static uint16_t s_reference   = 0;   // MANUAL idle baseline; 0 = auto-seed pending
static uint16_t s_threshold   = 0;   // MANUAL deviation band (counts)
static uint8_t  s_sensitivity = 4;   // MANUAL consecutive samples to trip/clear

// ── Detector state ─────────────────────────────────────────────────
static uint16_t s_filtered    = 0;   // fast EWMA (detection input)
static uint32_t s_meanX16     = 0;   // MANUAL slow rolling mean, 4 fractional bits
static bool     s_meanSeeded  = false;
static bool     s_trip        = false;
static uint8_t  s_outCount    = 0;   // consecutive out-of-band samples
static uint8_t  s_inCount     = 0;   // consecutive in-band samples
static uint16_t s_bootSamples = 0;   // samples seen since boot (warmup/auto-seed)
static uint16_t s_deviation   = 0;   // last |filtered - baseline| (both modes)

// ── AUTO-mode adaptive state (fixed-point ×16) ─────────────────────
static uint32_t s_baselineX16 = 0;   // slow adaptive baseline
static uint32_t s_sigmaX16    = 0;   // robust noise scale (EWMA of |dev|)
static bool     s_adaptSeeded = false;

// ── TPDO edge tracking ──────────────────────────────────────────────
static uint8_t  s_lastDigByte0 = 0xFF;
static uint8_t  s_lastDigByte3 = 0xFF;
static uint8_t  s_lastOut1Cmd  = 0xFF;
static uint8_t  s_lastOut2Cmd  = 0xFF;
static uint32_t s_lastReadUs   = 0;

// 50 Hz sampling.
static const uint32_t READ_PERIOD_US = 20000;

// ── AUTO-mode constants (locked by simulation on the real sensor trace;
//    all scale-free — expressed as multiples of the self-measured noise,
//    so no per-sensor calibration is ever required) ──────────────────
// Baseline EWMA: b += (x-b)>>9  → τ ≈ 512/50 ≈ 10 s. Tracks the slow
// background drift; far slower than a collision (which is frozen out).
static const uint8_t  AUTO_BASE_SHIFT = 9;
// Sigma EWMA: σ += (|dev|-σ)>>6 → τ ≈ 64/50 ≈ 1.3 s. Adapts the trip band
// to the current noise (e.g. widens after a collision leaves the sensor
// in a noisier regime).
static const uint8_t  AUTO_SIG_SHIFT  = 6;
static const uint16_t AUTO_K_TRIP     = 6;  // trip when |dev| > K·σ
static const uint16_t AUTO_K_CLEAR    = 5;   // clear hysteresis multiplier
static const uint16_t AUTO_DEV_FLOOR  = 30;  // min trip band (counts), dead-quiet guard
static const uint16_t AUTO_SIG_FLOOR  = 3;   // σ never collapses below this
static const uint16_t AUTO_SIG_SEED   = 8;   // σ at boot before it settles
static const uint8_t  AUTO_N_TRIP     = 2;   // consecutive out-of-band to trip (~60 ms)
static const uint8_t  AUTO_N_CLEAR    = 8;   // consecutive in-band to clear (~160 ms)
static const uint16_t AUTO_WARMUP     = 100; // ~2 s settle before trips are armed

// MANUAL slow-mean smoothing: mean += (sample-mean)/64 (calibration source).
static const uint8_t MEAN_SHIFT = 6;
// MANUAL auto-seed reference after ~2 s if never calibrated.
static const uint16_t BOOT_SEED_SAMPLES = 100;

static uint16_t rollingMeanValue(void) { return (uint16_t)(s_meanX16 >> 4); }
static uint16_t baselineValue(void)    { return (uint16_t)(s_baselineX16 >> 4); }
static uint16_t sigmaValue(void)       { return (uint16_t)(s_sigmaX16 >> 4); }
// "mean" reported to the outside world: the value you'd calibrate against.
// In AUTO the adaptive baseline already IS that; in MANUAL it's the slow mean.
static uint16_t meanValue(void)
{
    return (s_mode == MODE_AUTO) ? baselineValue() : rollingMeanValue();
}

static bool canopenReady(void)
{
#ifdef CAN_CONTROLLER_CANOPEN
    return CO != NULL && !CO->nodeIdUnconfigured && CO->CANmodule->CANnormal;
#else
    return false;
#endif
}

// ── NVS persistence (nvs.h directly; was the C++ uc2::Preferences wrapper) ──
static void persistU16(const char* key, uint16_t v)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        if (nvs_set_u16(h, key, v) == ESP_OK) nvs_commit(h);
        nvs_close(h);
    }
}
static void persistU8(const char* key, uint8_t v)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        if (nvs_set_u8(h, key, v) == ESP_OK) nvs_commit(h);
        nvs_close(h);
    }
}
// getUChar/getUShort equivalents: return `def` on missing key / error.
static uint8_t nvsGetU8(nvs_handle_t h, const char* key, uint8_t def)
{
    uint8_t v = def;
    (void)nvs_get_u8(h, key, &v);
    return v;
}
static uint16_t nvsGetU16(nvs_handle_t h, const char* key, uint16_t def)
{
    uint16_t v = def;
    (void)nvs_get_u16(h, key, &v);
    return v;
}

uint16_t gpio_can_slave_get_reference(void)  { return s_reference; }
uint16_t gpio_can_slave_get_threshold(void)  { return s_threshold; }
uint8_t  gpio_can_slave_get_sensitivity(void){ return s_sensitivity; }

void gpio_can_slave_set_reference(uint16_t value)
{
    if (value == s_reference) return;
    s_reference = value;
    persistU16(NVS_KEY_REF, value);
#ifdef CAN_CONTROLLER_CANOPEN
    OD_RAM.x2330_collision_reference = value;
#endif
    log_i("GpioCanSlave reference -> %u", (unsigned)value);
}

void gpio_can_slave_set_threshold(uint16_t value)
{
    if (value == s_threshold) return;
    s_threshold = value;
    persistU16(NVS_KEY_THR, value);
#ifdef CAN_CONTROLLER_CANOPEN
    OD_RAM.x2331_collision_threshold = value;
#endif
    log_i("GpioCanSlave threshold -> %u", (unsigned)value);
}

void gpio_can_slave_set_sensitivity(uint8_t value)
{
    if (value < 1)  value = 1;
    if (value > 50) value = 50;
    if (value == s_sensitivity) return;
    s_sensitivity = value;
    persistU8(NVS_KEY_SENS, value);
#ifdef CAN_CONTROLLER_CANOPEN
    OD_RAM.x2332_collision_sensitivity = value;
#endif
    log_i("GpioCanSlave sensitivity -> %u samples", (unsigned)value);
}

uint16_t gpio_can_slave_calibrate(void)
{
    uint16_t m = meanValue();
    log_i("GpioCanSlave calibrate: reference %u -> %u (rolling mean)",
          (unsigned)s_reference, (unsigned)m);
    gpio_can_slave_set_reference(m);
    return m;
}

uint8_t gpio_can_slave_get_mode(void) { return s_mode; }

void gpio_can_slave_set_mode(uint8_t mode)
{
    mode = (mode == MODE_MANUAL) ? MODE_MANUAL : MODE_AUTO;
    if (mode == s_mode) return;
    s_mode = mode;
    persistU8(NVS_KEY_MODE, mode);
#ifdef CAN_CONTROLLER_CANOPEN
    OD_RAM.x2335_collision_mode = mode;
#endif
    // Re-seed the adaptive state so a mode flip starts clean rather than
    // inheriting a stale baseline/σ from a previous AUTO session.
    s_adaptSeeded = false;
    s_trip = false;
    s_outCount = s_inCount = 0;
    s_bootSamples = 0;
    log_i("GpioCanSlave mode -> %s", mode == MODE_AUTO ? "AUTO" : "MANUAL");
}

static void enableTpdo2(void)
{
#ifdef CAN_CONTROLLER_CANOPEN
    // Clear bit 31 of TPDO2 COB-ID so the stack starts emitting it.
    // Bit 30 stays set so CANopenNode adds the node-id at activation.
    // (OD.c default = 0xC0000280 — invalid; we want 0x40000280.)
    OD_PERSIST_COMM.x1801_TPDOCommunicationParameter.COB_IDUsedByTPDO = 0x40000280u;
#endif
}

void gpio_can_slave_setup(void)
{
    log_i("GpioCanSlave setup");

    // Load collision config from NVS, falling back to pinConfig defaults.
    {
        nvs_handle_t h;
        bool ok = (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK);
        s_mode        = ok ? nvsGetU8 (h, NVS_KEY_MODE, MODE_AUTO) : MODE_AUTO;
        s_threshold   = ok ? nvsGetU16(h, NVS_KEY_THR, pinConfig.GPIO_COLLISION_THRESHOLD_DEFAULT)
                           : pinConfig.GPIO_COLLISION_THRESHOLD_DEFAULT;
        s_reference   = ok ? nvsGetU16(h, NVS_KEY_REF, 0) : 0;  // 0 = auto-seed
        s_sensitivity = ok ? nvsGetU8 (h, NVS_KEY_SENS, pinConfig.GPIO_COLLISION_SENSITIVITY_DEFAULT)
                           : pinConfig.GPIO_COLLISION_SENSITIVITY_DEFAULT;
        if (ok) nvs_close(h);
    }
    if (s_mode != MODE_MANUAL) s_mode = MODE_AUTO;
    if (s_sensitivity < 1) s_sensitivity = 1;
    log_i("GpioCanSlave collision cfg: mode=%s reference=%u (0=auto-seed) threshold=%u sensitivity=%u",
          s_mode == MODE_AUTO ? "AUTO" : "MANUAL",
          (unsigned)s_reference, (unsigned)s_threshold, (unsigned)s_sensitivity);

    // E-stop input — momentary-to-GND with internal pull-up.
    if (pinConfig.GPIO_ESTOP_PIN >= 0) {
        pinMode(pinConfig.GPIO_ESTOP_PIN, INPUT_PULLUP);
        log_i("GpioCanSlave E-stop on GPIO%d (pull-up)", pinConfig.GPIO_ESTOP_PIN);
    }

    // Collision ADC.
    if (pinConfig.GPIO_COLLISION_ADC >= 0) {
        analogReadResolution(12);
        analogSetPinAttenuation(pinConfig.GPIO_COLLISION_ADC, ADC_11db);
        log_i("GpioCanSlave collision ADC on GPIO%d", pinConfig.GPIO_COLLISION_ADC);
    }

#ifdef CAN_CONTROLLER_CANOPEN
    // Mirror the active config into the OD so the master's first SDO
    // read returns live values (not the zero-initialised RAM).
    OD_RAM.x2330_collision_reference   = s_reference;
    OD_RAM.x2331_collision_threshold   = s_threshold;
    OD_RAM.x2332_collision_sensitivity = s_sensitivity;
    OD_RAM.x2333_collision_command     = 0;
    OD_RAM.x2335_collision_mode        = s_mode;
    OD_RAM.x2336_collision_sigma       = 0;
#endif

    // Enable TPDO2 broadcasting. Must happen before canopenModule.setup()
    // builds its TPDO descriptors (see main.cpp ordering).
    enableTpdo2();

#ifdef I2C_BRIDGE_CONTROLLER
    // Generic I2C passthrough (Wire.begin on pinConfig.I2C_SDA/SCL).
    i2c_bridge_setup();
#endif
}

static uint8_t readDigital(int8_t pin, bool activeLow)
{
    if (pin < 0) return 0;
    int raw = digitalRead(pin);
    return activeLow ? (raw == 0 ? 1 : 0) : (raw != 0 ? 1 : 0);
}

static void applyToOut(int outId, uint8_t newCmd, uint8_t* last)
{
    if (newCmd == *last) return;
    *last = newCmd;
    cJSON* doc = cJSON_CreateObject();
    if (!doc) return;
    cjson_tool_set_int(doc, "digitaloutid", outId);
    cjson_tool_set_int(doc, "digitaloutval", (newCmd == 0xFF) ? -1 : (int)newCmd);
    digitalout_act(doc);
    cJSON_Delete(doc);
    log_i("GpioCanSlave OUT%d <- %u (from OD x2301)", outId, (unsigned)newCmd);
}

static void updateOutputsFromOd(void)
{
#ifdef CAN_CONTROLLER_CANOPEN
    uint8_t cmd1 = OD_RAM.x2301_digital_output_command[0];
    uint8_t cmd2 = OD_RAM.x2301_digital_output_command[1];

    // Pulse semantics: master writes 0xFF as a "trigger" sentinel. We
    // forward that to digitalout_act with val=-1 which fires
    // a brief HIGH-LOW pulse. Otherwise treat as direct level.
    applyToOut(1, cmd1, &s_lastOut1Cmd);
    applyToOut(2, cmd2, &s_lastOut2Cmd);
#endif
}

// Master SDO writes land directly in OD_RAM — detect changes and apply
// them to the local (NVS-backed) config. Also execute the command byte.
static void applyConfigFromOd(void)
{
#ifdef CAN_CONTROLLER_CANOPEN
    uint16_t odRef  = OD_RAM.x2330_collision_reference;
    uint16_t odThr  = OD_RAM.x2331_collision_threshold;
    uint8_t  odSens = OD_RAM.x2332_collision_sensitivity;
    uint8_t  odMode = OD_RAM.x2335_collision_mode;
    if (odRef  != s_reference)   gpio_can_slave_set_reference(odRef);
    if (odThr  != s_threshold)   gpio_can_slave_set_threshold(odThr);
    if (odSens != s_sensitivity) gpio_can_slave_set_sensitivity(odSens);
    if (odMode != s_mode)        gpio_can_slave_set_mode(odMode);

    uint8_t cmd = OD_RAM.x2333_collision_command;
    if (cmd != 0) {
        OD_RAM.x2333_collision_command = 0;
        switch (cmd) {
            case 1: gpio_can_slave_calibrate();      break;  // reference := rolling mean (manual)
            case 2: gpio_can_slave_set_mode(MODE_AUTO);   break;
            case 3: gpio_can_slave_set_mode(MODE_MANUAL); break;
            default:
                log_w("GpioCanSlave unknown collision command %u", (unsigned)cmd);
        }
    }
#endif
}

// ── AUTO mode ──────────────────────────────────────────────────────
// Adaptive baseline + robust noise scale (σ) + z-score vote.
// Parameter-free: the trip band is K·σ, and σ is continuously re-estimated
// from the *quiet* samples, so it tracks temperature drift, slow
// background changes and any fixed offset the sensor has. A collision
// shows up as a fast deflection far wider than the ambient noise — that
// is the whole detection problem.
static void detectAuto(void)
{
    uint16_t x = s_filtered;

    if (!s_adaptSeeded) {
        s_baselineX16 = ((uint32_t)x) << 4;
        s_sigmaX16    = ((uint32_t)AUTO_SIG_SEED) << 4;
        s_adaptSeeded = true;
    }

    uint16_t base = baselineValue(); // slow adaptive baseline
    uint16_t dev  = (x > base) ? (uint16_t)(x - base) : (uint16_t)(base - x);
    s_deviation = dev;

    uint16_t sigma     = sigmaValue();
    uint32_t thrTrip   = (uint32_t)AUTO_K_TRIP  * sigma;
    if (thrTrip  < AUTO_DEV_FLOOR)      thrTrip  = AUTO_DEV_FLOOR;
    uint32_t thrClear  = (uint32_t)AUTO_K_CLEAR * sigma;
    if (thrClear < (AUTO_DEV_FLOOR / 2)) thrClear = AUTO_DEV_FLOOR / 2;

    bool warm      = s_bootSamples >= AUTO_WARMUP;
    bool outOfBand = warm && (dev > thrTrip);

    if (!s_trip) {
        if (outOfBand) {
            s_inCount = 0;
            if (s_outCount < 0xFF) s_outCount++;
            if (s_outCount >= AUTO_N_TRIP) {
                s_trip = true;
                log_i("GpioCanSlave AUTO COLLISION TRIP (filt=%u base=%u dev=%u thr=%u sigma=%u)",
                      (unsigned)x, (unsigned)base, (unsigned)dev,
                      (unsigned)thrTrip, (unsigned)sigma);
            }
        } else {
            s_outCount = 0;
            // Adapt baseline + σ only while genuinely quiet.
            s_baselineX16 = (uint32_t)((int32_t)s_baselineX16 +
                            (((int32_t)(x << 4) - (int32_t)s_baselineX16) >> AUTO_BASE_SHIFT));
            s_sigmaX16 = (uint32_t)((int32_t)s_sigmaX16 +
                         (((int32_t)(dev << 4) - (int32_t)s_sigmaX16) >> AUTO_SIG_SHIFT));
            if (s_sigmaX16 < ((uint32_t)AUTO_SIG_FLOOR << 4))
                s_sigmaX16 = (uint32_t)AUTO_SIG_FLOOR << 4;
        }
    } else {
        if (dev < thrClear) {
            s_outCount = 0;
            if (s_inCount < 0xFF) s_inCount++;
            if (s_inCount >= AUTO_N_CLEAR) {
                s_trip = false;
                log_i("GpioCanSlave AUTO collision cleared (filt=%u base=%u dev=%u)",
                      (unsigned)x, (unsigned)base, (unsigned)dev);
            }
        } else {
            s_inCount = 0;
        }
    }
}

// ── MANUAL mode ────────────────────────────────────────────────────
static void detectManual(void)
{
    uint16_t dev = (s_filtered > s_reference)
                   ? (uint16_t)(s_filtered - s_reference)
                   : (uint16_t)(s_reference - s_filtered);
    s_deviation = dev;
    bool outOfBand = (s_reference != 0) && (dev > s_threshold);

    if (outOfBand) {
        s_inCount = 0;
        if (s_outCount < 0xFF) s_outCount++;
        if (!s_trip && s_outCount >= s_sensitivity) {
            s_trip = true;
            log_i("GpioCanSlave MANUAL COLLISION TRIP (filt=%u ref=%u dev=%u thr=%u after %u)",
                  (unsigned)s_filtered, (unsigned)s_reference,
                  (unsigned)dev, (unsigned)s_threshold, (unsigned)s_outCount);
        }
    } else {
        s_outCount = 0;
        if (s_inCount < 0xFF) s_inCount++;
        if (s_trip && s_inCount >= s_sensitivity) {
            s_trip = false;
            log_i("GpioCanSlave MANUAL collision cleared (filt=%u ref=%u)",
                  (unsigned)s_filtered, (unsigned)s_reference);
        }
    }

    // Slow rolling mean (calibration source) — frozen during any deviation.
    if (!s_trip && s_outCount == 0) {
        if (!s_meanSeeded) {
            s_meanX16 = ((uint32_t)s_filtered) << 4;
            s_meanSeeded = true;
        } else {
            int32_t mdelta = ((int32_t)s_filtered << 4) - (int32_t)s_meanX16;
            s_meanX16 = (uint32_t)((int32_t)s_meanX16 + (mdelta >> MEAN_SHIFT));
        }
    }

    // Auto-seed the reference once after boot if never calibrated.
    if (s_reference == 0 && s_meanSeeded && s_bootSamples >= BOOT_SEED_SAMPLES) {
        s_reference = rollingMeanValue();
#ifdef CAN_CONTROLLER_CANOPEN
        OD_RAM.x2330_collision_reference = s_reference;
#endif
        log_i("GpioCanSlave MANUAL auto-seeded reference = %u", (unsigned)s_reference);
    }
}

void gpio_can_slave_loop(void)
{
#ifdef CAN_CONTROLLER_CANOPEN
    if (!canopenReady()) return;

#ifdef I2C_BRIDGE_CONTROLLER
    // Service the I2C passthrough state machine on every iteration (not
    // throttled) so device conversion delays resolve promptly.
    i2c_bridge_loop();
#endif

    uint32_t nowUs = (uint32_t)esp_timer_get_time();
    if ((nowUs - s_lastReadUs) < READ_PERIOD_US) {
        updateOutputsFromOd();
        applyConfigFromOd();
        return;
    }
    s_lastReadUs = nowUs;

    // --- Digital inputs ------------------------------------------------
    uint8_t estop = readDigital(pinConfig.GPIO_ESTOP_PIN, /*activeLow=*/true);

    // --- Collision detection -------------------------------------------
    uint16_t raw = 0;
    if (pinConfig.GPIO_COLLISION_ADC >= 0) {
        raw = (uint16_t)analogRead(pinConfig.GPIO_COLLISION_ADC);
        s_bootSamples++; // TODO: Will there be a flip-over after some time (will warm stay "warm?")

        // Fast EWMA for detection: filtered += alpha * (raw - filtered)
        uint32_t alpha = pinConfig.GPIO_ADC_FILTER_ALPHA_X1024;
        int32_t  delta = (int32_t)raw - (int32_t)s_filtered;
        s_filtered = (uint16_t)((int32_t)s_filtered + (delta * (int32_t)alpha) / 1024);

        if (s_mode == MODE_AUTO) {
            detectAuto();
        } else {
            detectManual();
        }
    }

    // --- Compose status flags byte ------------------------------------
    // bit 0 = collision-trip, bit 1 = E-stop active
    uint8_t flags = 0;
    if (s_trip) flags |= 0x01;
    if (estop)  flags |= 0x02;

    // --- Push into OD --------------------------------------------------
    OD_RAM.x2300_digital_input_state[0] = estop;
    OD_RAM.x2300_digital_input_state[3] = flags;
    OD_RAM.x2310_analog_input_value[0]  = s_filtered;
    OD_RAM.x2310_analog_input_value[1]  = raw;
    OD_RAM.x2334_collision_mean         = meanValue();
    OD_RAM.x2335_collision_mode         = s_mode;
    OD_RAM.x2336_collision_sigma        = (s_mode == MODE_AUTO) ? sigmaValue() : 0;

    // --- Drive remote outputs / apply config writes ---------------------
    updateOutputsFromOd();
    applyConfigFromOd();

    // --- Trigger TPDO2 only on edge changes ---------------------------
    // The ADC value is intentionally NOT part of this predicate: sensor
    // values are never broadcast. Only the digital state (E-stop level,
    // flags byte with the collision-trip bit) fires a frame, and there is
    // no TPDO event-timer either (OD x1801 eventTimer = 0).
    bool edge =
        (estop != s_lastDigByte0) ||
        (flags != s_lastDigByte3);

    // log any value for debugging
    log_i("GpioCanSlave loop: estop=%u flags=0x%02X filt=%u raw=%u base=%u sigma=%u dev=%u trip=%d edge=%d",
          (unsigned)estop, (unsigned)flags,
          (unsigned)s_filtered, (unsigned)raw,
          (unsigned)baselineValue(), (unsigned)sigmaValue(),
          (unsigned)s_deviation, s_trip ? 1 : 0, edge ? 1 : 0);
    if (edge) {
        uint8_t prevEstop = s_lastDigByte0;
        uint8_t prevFlags = s_lastDigByte3;
        s_lastDigByte0 = estop;
        s_lastDigByte3 = flags;
        if (CO != NULL && CO->TPDO != NULL) {
            log_i("GpioCanSlave TPDO2 send  estop %u->%u  flags 0x%02X->0x%02X  filt=%u  (trip=%d)",
                  (unsigned)prevEstop, (unsigned)estop,
                  (unsigned)prevFlags, (unsigned)flags,
                  (unsigned)s_filtered, s_trip ? 1 : 0);
            CO_TPDOsendRequest(&CO->TPDO[1]); // TPDO2 = index 1
        }
    } else {
        // Periodic diagnostic — once every 5 s — so you can see the
        // detector state on the slave's own serial without CAN traffic.
        static uint32_t s_lastDiagMs = 0;
        uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000ULL);
        if ((nowMs - s_lastDiagMs) >= 5000) {
            s_lastDiagMs = nowMs;
            if (s_mode == MODE_AUTO) {
                log_d("GpioCanSlave idle AUTO  estop=%u flags=0x%02X filt=%u raw=%u base=%u sigma=%u dev=%u trip=%d",
                      (unsigned)estop, (unsigned)flags,
                      (unsigned)s_filtered, (unsigned)raw,
                      (unsigned)baselineValue(), (unsigned)sigmaValue(),
                      (unsigned)s_deviation, s_trip ? 1 : 0);
            } else {
                log_d("GpioCanSlave idle MANUAL estop=%u flags=0x%02X filt=%u raw=%u mean=%u ref=%u thr=%u dev=%u sens=%u trip=%d",
                      (unsigned)estop, (unsigned)flags,
                      (unsigned)s_filtered, (unsigned)raw,
                      (unsigned)rollingMeanValue(), (unsigned)s_reference,
                      (unsigned)s_threshold, (unsigned)s_deviation,
                      (unsigned)s_sensitivity, s_trip ? 1 : 0);
            }
        }
    }
#endif
}

// ------------------------------------------------------------------
// JSON entry-points. On the slave these run locally (DeviceRouter
// dispatches them); on the master DeviceRouter forwards the same JSON
// shape via SDO to the remote node instead.
//
//   {"task":"/gpio_act", "threshold":150, "sensitivity":4,
//    "reference":585, "calibrate":1, "qid":N}   (all fields optional)
//   {"task":"/gpio_get"}
// ------------------------------------------------------------------
cJSON* gpio_can_slave_act(cJSON* doc)
{
    int qid = cjson_tool_get_int(doc, "qid", 0);

    // "mode": "auto"|"manual" or 0|1
    cJSON* modeItem = cJSON_GetObjectItemCaseSensitive(doc, "mode");
    if (modeItem) {
        if (cJSON_IsString(modeItem) && modeItem->valuestring) {
            gpio_can_slave_set_mode(strcmp(modeItem->valuestring, "manual") == 0 ? MODE_MANUAL : MODE_AUTO);
        } else if (cJSON_IsNumber(modeItem)) {
            gpio_can_slave_set_mode((uint8_t)modeItem->valueint);
        }
        log_i("GpioCanSlave act qid=%d mode=%s", qid, s_mode == MODE_AUTO ? "AUTO" : "MANUAL");
    }
    cJSON* thr = cJSON_GetObjectItemCaseSensitive(doc, "threshold");
    if (thr && cJSON_IsNumber(thr)) {
        gpio_can_slave_set_threshold((uint16_t)thr->valueint);
        log_i("GpioCanSlave act qid=%d threshold=%d", qid, thr->valueint);
    }
    cJSON* sens = cJSON_GetObjectItemCaseSensitive(doc, "sensitivity");
    if (sens && cJSON_IsNumber(sens)) {
        gpio_can_slave_set_sensitivity((uint8_t)sens->valueint);
        log_i("GpioCanSlave act qid=%d sensitivity=%d", qid, sens->valueint);
    }
    cJSON* ref = cJSON_GetObjectItemCaseSensitive(doc, "reference");
    if (ref && cJSON_IsNumber(ref)) {
        gpio_can_slave_set_reference((uint16_t)ref->valueint);
        log_i("GpioCanSlave act qid=%d reference=%d", qid, ref->valueint);
    }
    cJSON* cal = cJSON_GetObjectItemCaseSensitive(doc, "calibrate");
    if (cal && cJSON_IsNumber(cal) && cal->valueint != 0) {
        uint16_t m = gpio_can_slave_calibrate();
        log_i("GpioCanSlave act qid=%d calibrated reference=%u", qid, (unsigned)m);
    }

    return gpio_can_slave_get(doc);  // answer with the resulting state
}

cJSON* gpio_can_slave_get(cJSON* doc)
{
    int qid = doc ? cjson_tool_get_int(doc, "qid", 0) : 0;
    cJSON* out = cJSON_CreateObject();
    if (!out) return NULL;
    cJSON* g = cJSON_CreateObject();
    cJSON_AddItemToObject(out, "gpio", g);
    cjson_tool_set_int(g, "mode",        (int)s_mode);   // 0=auto, 1=manual
    cjson_tool_set_int(g, "mean",        (int)meanValue());
    cjson_tool_set_int(g, "baseline",    (int)baselineValue());
    cjson_tool_set_int(g, "sigma",       (int)((s_mode == MODE_AUTO) ? sigmaValue() : 0));
    cjson_tool_set_int(g, "deviation",   (int)s_deviation);
    cjson_tool_set_int(g, "filtered",    (int)s_filtered);
    cjson_tool_set_int(g, "reference",   (int)s_reference);
    cjson_tool_set_int(g, "threshold",   (int)s_threshold);
    cjson_tool_set_int(g, "sensitivity", (int)s_sensitivity);
    cjson_tool_set_int(g, "trip",        s_trip ? 1 : 0);
    cjson_tool_set_int(g, "estop",       (pinConfig.GPIO_ESTOP_PIN >= 0)
                                            ? (digitalRead(pinConfig.GPIO_ESTOP_PIN) == 0 ? 1 : 0)
                                            : 0);
    cjson_tool_set_int(out, "qid", qid);
    return out;
}

#endif // GPIO_CAN_SLAVE_CONTROLLER

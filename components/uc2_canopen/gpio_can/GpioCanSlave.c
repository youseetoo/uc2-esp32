#include "uc2_hal.h"
#include "PinConfig.h"
#include "DigitalInController.h"
#include "DigitalOutController.h"
#include "I2cBridge.h"
#include "cJsonTool.h"
#ifdef GPIO_CAN_SLAVE_CONTROLLER
#include "GpioCanSlave.h"
#include <string.h>
#include <math.h>
#include "CANopen.h"
#include "OD.h"
#include "nvs.h"
#include "FreeRTOS.h"

// NVS persistence (survive flash re-write / reboot)
static const char* NVS_NAMESPACE = "gpio_coll";
static const char* NVS_KEY_REF   = "gpioCollRef";
static const char* NVS_KEY_THR   = "gpioCollThr";
static const char* NVS_KEY_SENS  = "gpioCollSens";
// quirk: NVS key for the collision mode is "gpioCMx  " (trailing spaces) —
// kept verbatim for behavioral parity with the original C++ code (4h)
static const char* NVS_KEY_MODE  = "gpioCMx  ";

// ── State ──────────────────────────────────────────────────────────────────
static uint16_t s_reference  = 0;
static uint16_t s_threshold  = 150;
static uint8_t  s_sensitivity = 4;
static uint8_t  s_mode = 0;        // 0 = AUTO (adaptive), 1 = MANUAL
static uint8_t  s_trip = 0;
static uint32_t s_devCount = 0;    // consecutive out-of-band samples (count to trip)
static uint32_t s_clearCount = 0;  // consecutive in-band samples (count to clear)
static bool s_wasColliding = false;

// ── Adaptation (auto mode) ─────────────────────────────────────────────────
static uint32_t s_lastSampleMs = 0;
static bool s_autoSeeded = false;
static float s_baselineFilt = 0.0f; // fast EWMA of the sensor (baseline)
static float s_meanFilt = 0.0f;     // slow rolling mean of the sensor
static float s_sigma2 = 0.0f;       // EWMA variance of the sensor around baseline
static float s_outFilt = 0.0f;      // fast EWMA of the sensor while out of band
static bool s_outSeeded = false;
static uint32_t s_outSamples = 0;
static float s_meanDevFilt = 0.0f;  // EWMA of |sample-baseline| while out of band
static bool s_meanDevSeeded = false;
static uint32_t s_meanDevSamples = 0;
static uint8_t s_autoState = 0;     // 0 idle, 1 candidate, 2 re-adapt
static uint32_t s_lastAdaptMs = 0;
static uint32_t s_recount = 0;
static uint16_t s_candidateRef = 0;
static uint16_t s_candidateThr = 0;

#define BASELINE_ALPHA_X1024 32
#define MEAN_ALPHA_X1024 2
#define SIGMA_ALPHA_X1024 16
#define OUT_ALPHA_X1024 32
#define MIN_ADAPT_SPACING_MS 1000
#define COUNT_STREAK 20
#define MEAN_DEV_SAMPLES_MIN 10
#define RE_ADAPT_SAMPLES_MAX 500
#define RE_ADAPT_SIGMA_FACTOR_X100 300
#define RE_ADAPT_THRESHOLD_MAX 255
#define MODE_AUTO 0
#define MODE_MANUAL 1

static float rollingMeanValue(void)
{
    return s_meanFilt;
}

static float baselineValue(void)
{
    return s_baselineFilt;
}

static float sigmaValue(void)
{
    if (s_sigma2 < 0.0f)
    {
        s_sigma2 = 0.0f;
    }
    return sqrtf(s_sigma2);
}

static float meanValue(void)
{
    if (!s_outSeeded)
    {
        return 0.0f;
    }
    return s_outFilt;
}

static bool canopenReady(void)
{
    extern CANopenNode CO;
    return (!CO.nodeIdUnconfigured);
}

static void persistU16(const char* key, uint16_t value)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
    {
        return;
    }
    nvs_set_u16(handle, key, value);
    nvs_commit(handle);
    nvs_close(handle);
}

static void persistU8(const char* key, uint8_t value)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
    {
        return;
    }
    nvs_set_u8(handle, key, value);
    nvs_commit(handle);
    nvs_close(handle);
}

// ── Public configuration accessors (SDO-backed) ────────────────────────────
uint16_t gpio_can_slave_get_reference(void)
{
    return s_reference;
}

void gpio_can_slave_set_reference(uint16_t value)
{
    s_reference = value;
    s_devCount = 0;
    s_clearCount = 0;
    s_baselineFilt = (float)value;
    s_sigma2 = 0.0f;
    s_outFilt = (float)value;
    s_outSeeded = false;
    s_outSamples = 0;
    s_meanDevFilt = 0.0f;
    s_meanDevSeeded = false;
    s_meanDevSamples = 0;
    s_autoState = 0;
    persistU16(NVS_KEY_REF, value);
    // E1: reset the slow mean to the new reference so it does not drift back
    // toward the pre-reset level over the next seconds.
    s_meanFilt = (float)value;
}

uint16_t gpio_can_slave_get_threshold(void)
{
    return s_threshold;
}

void gpio_can_slave_set_threshold(uint16_t value)
{
    s_threshold = value;
    s_devCount = 0;
    s_clearCount = 0;
    persistU16(NVS_KEY_THR, value);
}

uint8_t gpio_can_slave_get_sensitivity(void)
{
    return s_sensitivity;
}

void gpio_can_slave_set_sensitivity(uint8_t value)
{
    uint8_t clamped = value;
    if (clamped < 1)
    {
        clamped = 1;
    }
    if (clamped > 255)
    {
        clamped = 255;
    }
    s_sensitivity = clamped;
    s_devCount = 0;
    s_clearCount = 0;
    persistU8(NVS_KEY_SENS, clamped);
}

// reference := current rolling mean (user calibration); returns the new reference.
// In AUTO mode the rolling mean has been frozen during the deviation, so it
// still holds the true idle level.
uint16_t gpio_can_slave_calibrate(void)
{
    uint16_t newRef = (uint16_t)(s_meanFilt + 0.5f);
    if (newRef > 4095)
    {
        newRef = 4095;
    }
    gpio_can_slave_set_reference(newRef);
    return newRef;
}

uint8_t gpio_can_slave_get_mode(void)
{
    return s_mode;
}

void gpio_can_slave_set_mode(uint8_t mode)
{
    if (mode != MODE_AUTO && mode != MODE_MANUAL)
    {
        mode = MODE_MANUAL;
    }
    s_mode = mode;
    persistU8(NVS_KEY_MODE, mode);
}

// ── TPDO2 (event push) ─────────────────────────────────────────────────────
static void enableTpdo2(void)
{
    // SDO: 0x1802 sub0 = 8 (8 bytes enabled), sub1-8 = 0 (disabled)
    OD_PERSIST_COMM[0x1802][0] = 8;
    for (int i = 1; i <= 8; i++)
    {
        OD_PERSIST_COMM[0x1802][i] = 0;
    }
    // RAM: 0x1A02 sub0 = 8 (8 bytes enabled), sub1-8 = 0 (disabled)
    OD_RAM[0x1A02][0] = 8;
    for (int i = 1; i <= 8; i++)
    {
        OD_RAM[0x1A02][i] = 0;
    }
    // SDO: 0x1C12 sub0 = 8 (8 bytes), sub1 = 0x2330 sub2, sub2 = 0x2330 sub1
    OD_PERSIST_COMM[0x1C12][0] = 8;
    OD_PERSIST_COMM[0x1C12][1] = 0x2330 << 16 | 2;
    OD_PERSIST_COMM[0x1C12][2] = 0x2330 << 16 | 1;
    // RAM: 0x1E12 sub0 = 8 (8 bytes), sub1 = 0x2330 sub2, sub2 = 0x2330 sub1
    OD_RAM[0x1E12][0] = 8;
    OD_RAM[0x1E12][1] = 0x2330 << 16 | 2;
    OD_RAM[0x1E12][2] = 0x2330 << 16 | 1;
    CO_TPDOsendRequest(&CO->TPDO[1]);
}

void gpio_can_slave_setup(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK)
    {
        uint16_t ref;
        uint16_t thr;
        uint8_t sens;
        uint8_t mode;
        if (nvs_get_u16(handle, NVS_KEY_REF, &ref) == ESP_OK)
        {
            s_reference = ref;
        }
        if (nvs_get_u16(handle, NVS_KEY_THR, &thr) == ESP_OK)
        {
            s_threshold = thr;
        }
        if (nvs_get_u8(handle, NVS_KEY_SENS, &sens) == ESP_OK)
        {
            s_sensitivity = sens;
        }
        if (nvs_get_u8(handle, NVS_KEY_MODE, &mode) == ESP_OK)
        {
            s_mode = (mode == MODE_AUTO) ? MODE_AUTO : MODE_MANUAL;
        }
        nvs_close(handle);
    }
    if (s_sensitivity == 0)
    {
        s_sensitivity = 1;
    }
    s_baselineFilt = (float)s_reference;
    s_meanFilt = (float)s_reference;
    s_sigma2 = 0.0f;
    s_outFilt = (float)s_reference;
    s_outSeeded = false;
    s_outSamples = 0;
    s_meanDevFilt = 0.0f;
    s_meanDevSeeded = false;
    s_meanDevSamples = 0;
    s_autoState = 0;
    s_recount = 0;
    s_autoSeeded = (s_reference != 0);
    s_trip = 0;
    s_devCount = 0;
    s_clearCount = 0;
    s_wasColliding = false;
    enableTpdo2();
    if (s_reference == 0)
    {
        SerialPort::println("Collision auto-seed: reference=0, waiting for first reading");
    }
    else
    {
        SerialPort::printf("Collision mode=%s ref=%u thr=%u sens=%u\n",
            s_mode == MODE_AUTO ? "auto" : "manual",
            s_reference, s_threshold, s_sensitivity);
    }
}

// ── Digital I/O helpers ────────────────────────────────────────────────────
static uint16_t readDigital(void)
{
    uint16_t raw = 0;
#if defined(DIGITAL_IN_CONTROLLER)
    uint8_t mask = 0;
    digitalin_read(&mask);
    for (uint8_t i = 0; i < 8; i++)
    {
        raw |= (uint16_t)((mask >> i) & 1) << i;
    }
#endif
#if defined(DIGITAL_OUT_CONTROLLER)
    uint8_t mask = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        if (digitalRead(pConfigDigitalOut[i]))
        {
            raw |= (uint16_t)1 << (8 + i);
        }
    }
#endif
    return raw;
}

static void apply_out_to_pin(uint8_t idx, bool on)
{
    // idx 0..3 -> pinConfig.DIGITAL_OUT_1..4 ; idx 4..7 -> 2..5 (shared pins)
    const int pin = (idx < 4)
        ? (int)pinConfig.DIGITAL_OUT[idx + 1]
        : (int)pinConfig.DIGITAL_OUT[idx - 3];
    if (pin == -1)
    {
        return; // unassigned
    }
    if (on)
    {
        digitalWrite(pin, HIGH);
    }
    else
    {
        digitalWrite(pin, LOW);
    }
}

static void updateOutputsFromOd(void)
{
    // 0x2301 sub1/sub2 (RAM) — the master writes these via SDO; mirror to pins
    const uint16_t packed = (uint16_t)(OD_RAM[0x2301][0] | (OD_RAM[0x2301][1] << 8));
    for (uint8_t i = 0; i < 8; i++)
    {
        apply_out_to_pin(i, (packed >> i) & 1);
    }
}

static void applyConfigFromOd(void)
{
    uint16_t ref = OD_PERSIST_COMM[0x2330][0];
    uint16_t thr = OD_PERSIST_COMM[0x2330][1];
    uint8_t sens = OD_PERSIST_COMM[0x2330][2];
    if (ref != s_reference)
    {
        gpio_can_slave_set_reference(ref);
    }
    if (thr != s_threshold)
    {
        gpio_can_slave_set_threshold(thr);
    }
    if (sens != s_sensitivity)
    {
        gpio_can_slave_set_sensitivity(sens);
    }
}

static void detectAuto(uint32_t now, uint32_t dtMs, uint16_t raw, uint16_t filtered)
{
    (void)now;
    if (dtMs == 0)
    {
        dtMs = 1;
    }
    if (!s_autoSeeded)
    {
        // First valid sample: seed the baseline from the current value.
        s_reference = filtered;
        s_baselineFilt = (float)filtered;
        s_meanFilt = (float)filtered;
        s_autoSeeded = true;
        SerialPort::printf("Collision auto-seed: reference=%u\n", filtered);
        return;
    }
    float sigma = sigmaValue();
    bool out = (filtered > (uint32_t)s_reference + s_threshold)
        || (filtered + s_threshold < (uint32_t)s_reference);
    float dev = (float)abs((int32_t)filtered - (int32_t)s_reference);
    // Keep a rolling mean of the sensor (frozen while out-of-band / tripping).
    if (!out && !s_trip)
    {
        s_meanFilt += ((float)raw - s_meanFilt) * ((float)MEAN_ALPHA_X1024 / 1024.0f);
    }
    if (out)
    {
        if (!s_outSeeded)
        {
            s_outFilt = (float)raw;
            s_outSeeded = true;
        }
        else
        {
            s_outFilt += ((float)raw - s_outFilt) * ((float)OUT_ALPHA_X1024 / 1024.0f);
        }
        s_outSamples++;
        if (!s_meanDevSeeded)
        {
            s_meanDevFilt = dev;
            s_meanDevSeeded = true;
        }
        else
        {
            s_meanDevFilt += (dev - s_meanDevFilt) * ((float)OUT_ALPHA_X1024 / 1024.0f);
        }
        s_meanDevSamples++;
    }
    // Trip/clear hysteresis (unchanged from manual mode).
    if (out)
    {
        s_devCount++;
        s_clearCount = 0;
        if (s_devCount >= s_sensitivity && !s_trip)
        {
            s_trip = 1;
        }
    }
    else
    {
        s_clearCount++;
        s_devCount = 0;
        if (s_clearCount >= s_sensitivity && s_trip)
        {
            s_trip = 0;
            s_outSeeded = false;
            s_outSamples = 0;
            s_meanDevSeeded = false;
            s_meanDevSamples = 0;
        }
    }
    // Adaptation state machine (only while idle and not tripping).
    if (s_trip)
    {
        s_recount = 0;
        return;
    }
    if (now - s_lastAdaptMs < MIN_ADAPT_SPACING_MS)
    {
        return;
    }
    if (s_autoState == 0)
    {
        // Candidate: stable baseline, low noise, enough samples, and the
        // observed deviation is small — we are probably still at idle level.
        bool stable = sigma < 15.0f;
        bool devLow = (s_outSamples > 0) && (s_meanDevFilt < 60.0f);
        if (stable && devLow && s_outSamples >= 10)
        {
            s_autoState = 1;
            s_candidateRef = filtered;
            s_candidateThr = (uint16_t)(s_meanDevFilt + 2.0f * sigma + 10.0f);
            if (s_candidateThr < 20)
            {
                s_candidateThr = 20;
            }
            s_recount = 0;
            s_lastAdaptMs = now;
        }
    }
    else if (s_autoState == 1)
    {
        s_recount++;
        bool stillStable = sigma < 20.0f && (s_meanDevFilt < 80.0f);
        if (!stillStable)
        {
            s_autoState = 0;
        }
        else if (s_recount >= COUNT_STREAK)
        {
            // Commit: shift the baseline to the new level and tighten the
            // threshold to the observed noise + margin.
            s_reference = s_candidateRef;
            s_threshold = s_candidateThr;
            s_baselineFilt = (float)s_reference;
            s_meanFilt = (float)s_reference;
            s_sigma2 = 0.0f;
            s_outFilt = (float)s_reference;
            s_outSeeded = false;
            s_outSamples = 0;
            s_meanDevFilt = 0.0f;
            s_meanDevSeeded = false;
            s_meanDevSamples = 0;
            s_devCount = 0;
            s_clearCount = 0;
            s_autoState = 0;
            s_lastAdaptMs = now;
            persistU16(NVS_KEY_REF, s_reference);
            persistU16(NVS_KEY_THR, s_threshold);
            SerialPort::printf("Collision auto-readapt: ref=%u thr=%u\n",
                s_reference, s_threshold);
        }
    }
    else if (s_autoState == 2)
    {
        // Re-adapt: we saw a large deviation — wait for the sensor to settle
        // back near the (old) baseline, then re-seed from the new level.
        if (s_outSamples >= RE_ADAPT_SAMPLES_MAX)
        {
            s_autoState = 0;
            s_outSeeded = false;
            s_outSamples = 0;
            s_meanDevSeeded = false;
            s_meanDevSamples = 0;
        }
    }
}

static void detectManual(uint16_t filtered)
{
    bool out = (filtered > (uint32_t)s_reference + s_threshold)
        || (filtered + s_threshold < (uint32_t)s_reference);
    if (out)
    {
        s_devCount++;
        s_clearCount = 0;
        if (s_devCount >= s_sensitivity && !s_trip)
        {
            s_trip = 1;
        }
    }
    else
    {
        s_clearCount++;
        s_devCount = 0;
        if (s_clearCount >= s_sensitivity && s_trip)
        {
            s_trip = 0;
        }
    }
}

void gpio_can_slave_loop(void)
{
    uint16_t raw = 0;
    uint32_t now = millis();
    uint32_t dtMs = now - s_lastSampleMs;
    if (dtMs == 0)
    {
        dtMs = 1;
    }
    s_lastSampleMs = now;
    raw = analogRead(GPIO_COLLISION_ADC);
    uint16_t filtered = (uint16_t)(
        s_baselineFilt + ((float)raw - s_baselineFilt)
        * ((float)BASELINE_ALPHA_X1024 / 1024.0f));
    // Keep the rolling mean of the sensor (frozen while out-of-band / tripping)
    // and the variance of the baseline (for auto-adaptation decisions).
    bool inBand = !(filtered > (uint32_t)s_reference + s_threshold
        || filtered + s_threshold < (uint32_t)s_reference);
    if (inBand && !s_trip)
    {
        float delta = (float)raw - s_meanFilt;
        s_meanFilt += delta * ((float)MEAN_ALPHA_X1024 / 1024.0f);
        s_sigma2 += (delta * delta - s_sigma2) * ((float)SIGMA_ALPHA_X1024 / 1024.0f);
        if (s_sigma2 < 0.0f)
        {
            s_sigma2 = 0.0f;
        }
    }
    // Publish to TPDO payload (0x2330 sub1 = fast-filtered, sub2 = raw).
    OD_RAM[0x2330][0] = filtered;
    OD_RAM[0x2330][1] = raw;
    // E-stop (GPIO_ESTOP_PIN) into 0x2300 sub1; flags byte sub4.
    uint8_t estop = 1;
#ifdef GPIO_ESTOP_PIN
    if (pinConfig.GPIO_ESTOP_PIN != -1)
    {
        estop = digitalRead(pinConfig.GPIO_ESTOP_PIN) ? 1 : 0;
    }
#endif
    OD_RAM[0x2300][0] = estop;
    OD_RAM[0x2300][3] = (uint8_t)((s_trip ? 1 : 0) | (estop ? 2 : 0));
    if (s_mode == MODE_AUTO)
    {
        detectAuto(now, dtMs, raw, filtered);
    }
    else
    {
        detectManual(filtered);
    }
    applyConfigFromOd();
    updateOutputsFromOd();
    // Push TPDO2 only on trip-state change (event semantics).
    if (s_trip != s_wasColliding)
    {
        s_wasColliding = s_trip;
        if (canopenReady())
        {
            CO_TPDOsendRequest(&CO->TPDO[1]);
        }
    }
}

// ── JSON entry-points (dispatched by DeviceRouter) ─────────────────────────
cJSON* gpio_can_slave_act(cJSON* doc)
{
    int qid = cjson_tool_get_int(doc, "qid", 0);
    if (doc && cjson_tool_get_int(doc, "calibrate", 0) != 0)
    {
        gpio_can_slave_calibrate();
    }
    if (doc)
    {
        if (cjson_tool_has_key(doc, "reference"))
        {
            gpio_can_slave_set_reference(
                (uint16_t)cjson_tool_get_int(doc, "reference", 0));
        }
        if (cjson_tool_has_key(doc, "threshold"))
        {
            gpio_can_slave_set_threshold(
                (uint16_t)cjson_tool_get_int(doc, "threshold", 0));
        }
        if (cjson_tool_has_key(doc, "sensitivity"))
        {
            gpio_can_slave_set_sensitivity(
                (uint8_t)cjson_tool_get_int(doc, "sensitivity", 0));
        }
        if (cjson_tool_has_key(doc, "mode"))
        {
            gpio_can_slave_set_mode(
                (uint8_t)cjson_tool_get_int(doc, "mode", 0));
        }
    }
    cJSON* resp = cJSON_CreateObject();
    if (resp)
    {
        cjson_tool_set_int(resp, "ok", 1);
        if (qid)
        {
            cjson_tool_set_int(resp, "qid", qid);
        }
    }
    return resp;
}

cJSON* gpio_can_slave_get(cJSON* doc)
{
    (void)doc;
    cJSON* resp = cJSON_CreateObject();
    if (!resp)
    {
        return NULL;
    }
    cjson_tool_set_int(resp, "ref", s_reference);
    cjson_tool_set_int(resp, "threshold", s_threshold);
    cjson_tool_set_int(resp, "sensitivity", s_sensitivity);
    cjson_tool_set_int(resp, "mode", s_mode);
    cjson_tool_set_int(resp, "trip", s_trip);
    cjson_tool_set_int(resp, "devCount", (int)s_devCount);
    cjson_tool_set_int(resp, "clearCount", (int)s_clearCount);
    cjson_tool_set_int(resp, "raw", readDigital());
    cjson_tool_set_number(resp, "mean", (double)rollingMeanValue());
    cjson_tool_set_number(resp, "sigma", (double)sigmaValue());
    cjson_tool_set_int(resp, "autoState", s_autoState);
    cjson_tool_set_int(resp, "autoSeeded", s_autoSeeded ? 1 : 0);
    cjson_tool_set_int(resp, "outSamples", (int)s_outSamples);
    cjson_tool_set_number(resp, "meanDev", (double)s_meanDevFilt);
    cjson_tool_set_number(resp, "meanDevN", (double)s_meanDevSamples);
    return resp;
}
#endif // GPIO_CAN_SLAVE_CONTROLLER

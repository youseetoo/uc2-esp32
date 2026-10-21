// Tmp102Controller (Phase 4: pure C)
#include "Tmp102Controller.h"
#include <uc2_hal/uc2_wire_c.h>
#include <uc2_hal/uc2_time.h>
#include <uc2_hal/uc2_log.h>
// The handle-based esp_driver_tsens is only available on S3+ (SOC_TEMP_SENSOR
// SUPPORTED); the classic ESP32 has no such HAL, so it is excluded there.
#ifndef CONFIG_IDF_TARGET_ESP32
#include "driver/temperature_sensor.h"
#endif
#include <math.h>
#include <float.h>
#include "cJsonTool.h"

// TMP102 register addresses
#define TMP102_REG_TEMP   0x00
#define TMP102_REG_CONFIG 0x01

// Polling cadence — 1 Hz is plenty for chassis-air monitoring and lets
// us share the I2C bus politely with the rest of the system.
#define TMP102_POLL_INTERVAL_MS 1000

static float    last_pcb_c   = NAN;
static float    last_air_c   = NAN;
static float    last_esp_c   = NAN;
static uint32_t last_poll_ms = 0;
static bool     initialized  = false;
static bool     pcb_available = false;
static bool     air_available = false;
#ifndef CONFIG_IDF_TARGET_ESP32
static temperature_sensor_handle_t tsens = NULL;
#endif

// Configure a TMP102 for the default 12-bit, 4 Hz continuous-conversion
// mode (CR1:CR0 = 10). POR value already does this, but writing it back
// recovers a sensor that booted into an odd state after a glitch.
static bool configure_sensor(uint8_t addr)
{
    wire_begin_transmission(addr);
    wire_write(TMP102_REG_CONFIG);
    // POR config = 0x60A0 (12-bit, continuous, 4Hz, comparator mode).
    wire_write(0x60);
    wire_write(0xA0);
    uint8_t err = wire_end_transmission(true);
    if (err != 0)
    {
        log_w("TMP102 @ 0x%02X: config write failed (err=%u)", addr, err);
        return false;
    }
    return true;
}

// Read the 12-bit temperature register from the given sensor.
// Returns degrees Celsius, or NAN on bus failure.
static float read_sensor(uint8_t addr)
{
    wire_begin_transmission(addr);
    wire_write(TMP102_REG_TEMP);
    if (wire_end_transmission(false) != 0)
    {
        return NAN;
    }
    if (wire_request_from(addr, 2) != 2)
    {
        return NAN;
    }
    int16_t msb = (int16_t)wire_read();
    int16_t lsb = (int16_t)wire_read();
    // 12-bit, left-justified in the 16-bit word, signed.
    int16_t raw = ((msb << 8) | lsb) >> 4;
    if (raw & 0x0800) // sign-extend the 12-bit value
        raw |= 0xF000;
    return raw * 0.0625f;
}

// Read the ESP internal temperature in degrees C, or NAN on failure. On the
// classic ESP32 the handle-based esp_driver_tsens is unavailable (no
// SOC_TEMP_SENSOR_SUPPORTED), so this returns NAN and the two TMP102 I2C
// sensors (pcb/air) are the source of truth.
static float read_esp_c(void)
{
#ifdef CONFIG_IDF_TARGET_ESP32
    return NAN;
#else
    float c = NAN;
    if (tsens && temperature_sensor_get_celsius(tsens, &c) == ESP_OK)
        return c;
    return NAN;
#endif
}

void tmp102_setup(void)
{
    // Ensure the I2C bus is up. On boards that compile i2c_master.cpp the
    // call has already been made and re-issuing it is a no-op once a
    // matching configuration is in place. On the CAN-HAT master build the
    // master-side I2C scanner is not compiled, so this call is the only
    // wire_begin() in the system. The MCP4017 and TMP102 share this bus.
    wire_begin(pinConfig.I2C_SDA, pinConfig.I2C_SCL, 100000);

#ifndef CONFIG_IDF_TARGET_ESP32
    // Internal tsens (S3+): install + enable (read is on-demand below).
    // Skipped on the classic ESP32 — see read_esp_c().
    temperature_sensor_config_t ts_cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-4, 85);
    if (temperature_sensor_install(&ts_cfg, &tsens) == ESP_OK)
    {
        temperature_sensor_enable(tsens);
    }
    else
    {
        tsens = NULL;
    }
#endif

    pcb_available = configure_sensor(TMP102_ADDR_PCB);
    air_available = configure_sensor(TMP102_ADDR_AIR);
    if (!pcb_available && !air_available) {
        log_w("Tmp102Controller: no sensors found — polling disabled");
        return;
    }
    initialized = true;
    log_i("Tmp102Controller setup: pcb=%d air=%d (SDA=%d, SCL=%d)",
          pcb_available, air_available, pinConfig.I2C_SDA, pinConfig.I2C_SCL);
}

void tmp102_loop(void)
{
    if (!initialized) return;
    uint32_t now = millis();
    if ((now - last_poll_ms) < TMP102_POLL_INTERVAL_MS) return;
    last_poll_ms = now;

    if (pcb_available) last_pcb_c = read_sensor(TMP102_ADDR_PCB);
    if (air_available) last_air_c = read_sensor(TMP102_ADDR_AIR);

    // ESP internal temperature (tsens driver, degrees C). It drifts a lot
    // but is OK as a "is the chip hot?" fallback.
    last_esp_c = read_esp_c();
}

float tmp102_get_pcb_c(void) { return last_pcb_c; }
float tmp102_get_air_c(void) { return last_air_c; }
float tmp102_get_esp_c(void) { return last_esp_c; }

float tmp102_get_max_ambient_c(void)
{
    float best = -INFINITY;
    if (!isnan(last_pcb_c) && last_pcb_c > best) best = last_pcb_c;
    if (!isnan(last_air_c) && last_air_c > best) best = last_air_c;
    if (!isnan(last_esp_c) && last_esp_c > best) best = last_esp_c;
    if (best == -INFINITY) return NAN;
    return best;
}

int tmp102_act(cJSON *root)
{
    // /temp_act currently has no settable knobs (the TMP102 runs in its
    // POR continuous-conversion mode). Reserved for future ALERT-pin
    // configuration. Accept the call and echo the qid back.
    int qid = cjson_tool_get_int(root, "qid", 0);
    log_i("/temp_act: qid=%d (no-op)", qid);
    return qid;
}

cJSON *tmp102_get(cJSON *root)
{
    cJSON *ret = cJSON_CreateObject();
    cJSON *tmp = cJSON_CreateObject();

    // extrac the qid:
    int qid = cjson_tool_get_int(root, "qid", 0);
    cjson_tool_set_int(ret, "qid", qid);

    // Force a fresh read so /temp_get is always recent — useful when the
    // caller polls faster than the 1 Hz loop cadence.
    if (initialized)
    {
        last_pcb_c = read_sensor(TMP102_ADDR_PCB);
        last_air_c = read_sensor(TMP102_ADDR_AIR);
        last_esp_c = read_esp_c();
    }

    cjson_tool_set_float(tmp, "pcb", last_pcb_c);
    cjson_tool_set_float(tmp, "air", last_air_c);
    cjson_tool_set_float(tmp, "esp", last_esp_c);
    cjson_tool_set_int(tmp, "pcb_ok", isnan(last_pcb_c) ? 0 : 1);
    cjson_tool_set_int(tmp, "air_ok", isnan(last_air_c) ? 0 : 1);
    cJSON_AddItemToObject(ret, "temp", tmp);
    return ret;
}

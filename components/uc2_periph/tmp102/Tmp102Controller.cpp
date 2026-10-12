#include "Tmp102Controller.h"
#include <uc2_hal.h>
#include <uc2_hal/uc2_wire.h>
#include "driver/temperature_sensor.h"
#include <math.h>
#include "cJsonTool.h"

// ESP32 internal temperature sensor via the IDF tsens driver (handle-based;
// S3 has no ROM temprature_sens_read()).

namespace Tmp102Controller
{
    static const char *TAG = "Tmp102Controller";

    // TMP102 register addresses
    static constexpr uint8_t REG_TEMP   = 0x00;
    static constexpr uint8_t REG_CONFIG = 0x01;

    // Polling cadence — 1 Hz is plenty for chassis-air monitoring and lets
    // us share the I2C bus politely with the rest of the system.
    static constexpr uint32_t POLL_INTERVAL_MS = 1000;

    static float lastPcbC = NAN;
    static float lastAirC = NAN;
    static float lastEspC = NAN;
    static uint32_t lastPollMs = 0;
    static bool initialized = false;
    static bool pcbAvailable = false;
    static bool airAvailable = false;
    static temperature_sensor_handle_t tsens = nullptr;

    // Configure a TMP102 for the default 12-bit, 4 Hz continuous-conversion
    // mode (CR1:CR0 = 10). POR value already does this, but writing it back
    // recovers a sensor that booted into an odd state after a glitch.
    static bool configureSensor(uint8_t addr)
    {
        Wire.beginTransmission(addr);
        Wire.write(REG_CONFIG);
        // POR config = 0x60A0 (12-bit, continuous, 4Hz, comparator mode).
        Wire.write(0x60);
        Wire.write(0xA0);
        uint8_t err = Wire.endTransmission();
        if (err != 0)
        {
            log_w("TMP102 @ 0x%02X: config write failed (err=%u)", addr, err);
            return false;
        }
        return true;
    }

    // Read the 12-bit temperature register from the given sensor.
    // Returns degrees Celsius, or NAN on bus failure.
    static float readSensor(uint8_t addr)
    {
        Wire.beginTransmission(addr);
        Wire.write(REG_TEMP);
        if (Wire.endTransmission(false) != 0)
        {
            return NAN;
        }
        if (Wire.requestFrom((int)addr, 2) != 2)
        {
            return NAN;
        }
        int16_t msb = Wire.read();
        int16_t lsb = Wire.read();
        // 12-bit, left-justified in the 16-bit word, signed.
        int16_t raw = ((msb << 8) | lsb) >> 4;
        if (raw & 0x0800) // sign-extend the 12-bit value
            raw |= 0xF000;
        return raw * 0.0625f;
    }

    // Read the ESP internal temperature in degrees C, or NAN on failure.
    static float readEspC()
    {
        float c = NAN;
        if (tsens && temperature_sensor_get_celsius(tsens, &c) == ESP_OK)
            return c;
        return NAN;
    }

    void setup()
    {
        // Ensure the I2C bus is up. On boards that compile i2c_master.cpp the
        // call has already been made and re-issuing it is a no-op once a
        // matching configuration is in place. On the CAN-HAT master build the
        // master-side I2C scanner is not compiled, so this call is the only
        // Wire.begin() in the system. The MCP4017 and TMP102 share this bus.
        Wire.begin(pinConfig.I2C_SDA, pinConfig.I2C_SCL, 100000);

        // Internal tsens: install + enable (read is on-demand below).
        temperature_sensor_config_t tsCfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-4, 85);
        if (temperature_sensor_install(&tsCfg, &tsens) == ESP_OK)
        {
            temperature_sensor_enable(tsens);
        }
        else
        {
            tsens = nullptr;
        }

        pcbAvailable = configureSensor(ADDR_PCB);
        airAvailable = configureSensor(ADDR_AIR);
        if (!pcbAvailable && !airAvailable) {
            log_w("Tmp102Controller: no sensors found — polling disabled");
            return;
        }
        initialized = true;
        log_i("Tmp102Controller setup: pcb=%d air=%d (SDA=%d, SCL=%d)",
              pcbAvailable, airAvailable, pinConfig.I2C_SDA, pinConfig.I2C_SCL);
    }

    void loop()
    {
        if (!initialized) return;
        uint32_t now = millis();
        if ((now - lastPollMs) < POLL_INTERVAL_MS) return;
        lastPollMs = now;

        if (pcbAvailable) lastPcbC = readSensor(ADDR_PCB);
        if (airAvailable) lastAirC = readSensor(ADDR_AIR);

        // ESP internal temperature (tsens driver, degrees C). It drifts a lot
        // but is OK as a "is the chip hot?" fallback.
        lastEspC = readEspC();
    }

    float getPcbC() { return lastPcbC; }
    float getAirC() { return lastAirC; }
    float getEspC() { return lastEspC; }

    float getMaxAmbientC()
    {
        float best = -INFINITY;
        if (!isnan(lastPcbC) && lastPcbC > best) best = lastPcbC;
        if (!isnan(lastAirC) && lastAirC > best) best = lastAirC;
        if (!isnan(lastEspC) && lastEspC > best) best = lastEspC;
        if (best == -INFINITY) return NAN;
        return best;
    }

    int act(cJSON *root)
    {
        // /temp_act currently has no settable knobs (the TMP102 runs in its
        // POR continuous-conversion mode). Reserved for future ALERT-pin
        // configuration. Accept the call and echo the qid back.
        int qid = cJsonTool::getJsonInt(root, "qid");
        log_i("/temp_act: qid=%d (no-op)", qid);
        return qid;
    }

    cJSON *get(cJSON *root)
    {
        cJSON *ret = cJSON_CreateObject();
        cJSON *tmp = cJSON_CreateObject();

        // extrac the qid:
        int qid = cJsonTool::getJsonInt(root, "qid");
        cJsonTool::setJsonInt(ret, "qid", qid);
        
        // Force a fresh read so /temp_get is always recent — useful when the
        // caller polls faster than the 1 Hz loop cadence.
        if (initialized)
        {
            lastPcbC = readSensor(ADDR_PCB);
            lastAirC = readSensor(ADDR_AIR);
            lastEspC = readEspC();
        }

        cJsonTool::setJsonFloat(tmp, "pcb", lastPcbC);
        cJsonTool::setJsonFloat(tmp, "air", lastAirC);
        cJsonTool::setJsonFloat(tmp, "esp", lastEspC);
        cJsonTool::setJsonInt(tmp, "pcb_ok", isnan(lastPcbC) ? 0 : 1);
        cJsonTool::setJsonInt(tmp, "air_ok", isnan(lastAirC) ? 0 : 1);
        cJSON_AddItemToObject(ret, "temp", tmp);
        return ret;
    }
}

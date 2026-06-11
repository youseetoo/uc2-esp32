#include "FanController.h"
#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <math.h>
#include "cJsonTool.h"
#include "../tmp102/Tmp102Controller.h"

namespace FanController
{
    static const char *TAG = "FanController";

    // ---- State ----
    static bool     initialized   = false;
    static Mode     mode          = MODE_AUTO;
    static uint8_t  currentWiper  = WIPER_QUIET;
    static uint8_t  manualWiper   = WIPER_QUIET;
    static bool     kickEnabled   = true;
    static bool     stalled       = false;
    static float    currentRpm    = 0.0f;
    static uint32_t lastCurveMs   = 0;
    static uint32_t stallStartMs  = 0; // when we first noticed RPM=0 while commanded ON
    static uint32_t kickEndMs     = 0; // !=0 → currently mid-kick, drop back at this time
    static uint8_t  preKickWiper  = WIPER_QUIET;
    static float    lastTempAppliedC = NAN; // temp last used by curve (for hysteresis)

    // ---- Tacho ----
    static volatile uint32_t tachoPulses = 0;
    static uint32_t lastTachoSampleMs    = 0;

    // ---- Curve ----
    static CurvePoint curve[MAX_CURVE_POINTS];
    static uint8_t    curvePoints = 0;

    static Preferences fPrefs;

    // Default curve (ascending temperature thresholds):
    //   < 35 °C  → quiet
    //   < 45 °C  → low      (wiper 0x50 = 80)
    //   < 55 °C  → medium   (wiper 0x30 = 48)
    //   < 65 °C  → high     (wiper 0x10 = 16)
    //   >= 65 °C → full     (wiper 0x00 = 0)
    static void loadDefaultCurve()
    {
        curve[0] = {35.0f, WIPER_QUIET};
        curve[1] = {45.0f, 0x50};
        curve[2] = {55.0f, 0x30};
        curve[3] = {65.0f, 0x10};
        curve[4] = {200.0f, WIPER_FULL}; // anything above 65 -> full speed
        curvePoints = 5;
    }

    // ===== Tacho ISR =====
    // Pulled into IRAM so the ISR can run while flash is busy.
    static void IRAM_ATTR onTachoEdge()
    {
        tachoPulses++;
    }

    // ===== MCP4017 driver =====
    // The MCP4017 is a single-byte device — every write is "set the wiper".
    // No register address. 7-bit value, top bit ignored. Read returns the
    // currently-stored wiper byte.
    bool setWiper(uint8_t value)
    {
        if (!initialized) return false;
        if (value > 0x7F) value = 0x7F;
        Wire.beginTransmission(MCP4017_ADDR);
        Wire.write(value);
        uint8_t err = Wire.endTransmission();
        if (err != 0)
        {
            log_w("MCP4017: write %u failed (err=%u)", value, err);
            return false;
        }
        currentWiper = value;
        return true;
    }

    static uint8_t probeWiperFromChip()
    {
        // requestFrom returns a single byte from the wiper register.
        if (Wire.requestFrom((int)MCP4017_ADDR, 1) != 1)
        {
            log_w("MCP4017: probe read failed");
            return WIPER_POR;
        }
        return (uint8_t)(Wire.read() & 0x7F);
    }

    uint8_t getWiper() { return currentWiper; }
    float   getRpm()   { return currentRpm; }
    bool    isStalled(){ return stalled; }

    // ===== Curve evaluation with hysteresis =====
    static uint8_t evaluateCurve(float tempC)
    {
        if (isnan(tempC) || curvePoints == 0) {
            return WIPER_QUIET;
        }

        // Choose the highest-temp breakpoint whose threshold is <= tempC.
        // If none, pick the lowest-temp band (= quietest).
        uint8_t chosen = curve[0].wiper;
        for (uint8_t i = 0; i < curvePoints; ++i) {
            if (tempC >= curve[i].thresholdC) {
                // Use the *next* point's wiper if available — the band is
                // (curve[i].thresholdC, curve[i+1].thresholdC] so we ramp up
                // as we cross each threshold.
                if (i + 1 < curvePoints) chosen = curve[i + 1].wiper;
                else                     chosen = curve[i].wiper;
            }
        }

        // Apply hysteresis: only allow the wiper to *quieten* (higher value)
        // if the temperature has dropped CURVE_HYSTERESIS below the current
        // band's threshold. This requires us to know which band we're in.
        if (!isnan(lastTempAppliedC) && chosen > currentWiper) {
            // Would quieten — only accept if temp fell more than HYSTERESIS
            // below the threshold that put us in the current (louder) band.
            if (tempC > (lastTempAppliedC - CURVE_HYSTERESIS)) {
                return currentWiper; // hold
            }
        }
        return chosen;
    }

    // ===== Tacho sampling =====
    static void sampleTacho(uint32_t now)
    {
        uint32_t dt = now - lastTachoSampleMs;
        if (dt < TACHO_WINDOW_MS) return;

        // Atomically snapshot + clear the ISR counter.
        noInterrupts();
        uint32_t pulses = tachoPulses;
        tachoPulses = 0;
        interrupts();

        // RPM = (pulses / pulses_per_rev) * (60_000 ms/min / dt_ms)
        currentRpm = ((float)pulses / (float)PULSES_PER_REV)
                     * (60000.0f / (float)dt);
        lastTachoSampleMs = now;
    }

    // ===== Lifecycle =====
    void setup()
    {
        log_i("FanController setup begin");

        // Wire.begin() already done by i2c_master::setup().
        // Probe the MCP4017 — at POR it should return 0x3F.
        uint8_t probe = probeWiperFromChip();
        log_i("MCP4017 probe @0x%02X = 0x%02X (expected 0x3F at POR)",
              MCP4017_ADDR, probe);
        currentWiper = probe;

        // Tacho input — open-drain on the fan side, internal pull-up keeps the
        // line HIGH when idle, falling edge = rotation pulse.
        if (pinConfig.FAN_TACHO_PIN >= 0) {
            pinMode(pinConfig.FAN_TACHO_PIN, INPUT_PULLUP);
            attachInterrupt(digitalPinToInterrupt(pinConfig.FAN_TACHO_PIN),
                            onTachoEdge, FALLING);
            log_i("Tacho ISR attached on GPIO %d", pinConfig.FAN_TACHO_PIN);
        } else {
            log_w("FAN_TACHO_PIN disabled — RPM/stall detection unavailable");
        }

        // Load persisted mode / curve / kick flag from NVS.
        fPrefs.begin("fan", false);
        mode        = (Mode)fPrefs.getUChar("mode",  (uint8_t)MODE_AUTO);
        manualWiper =       fPrefs.getUChar("mwip",  WIPER_QUIET);
        kickEnabled =       fPrefs.getBool ("kick",  true);

        size_t blobSize = fPrefs.getBytesLength("curve");
        if (blobSize == sizeof(CurvePoint) * MAX_CURVE_POINTS) {
            CurvePoint tmp[MAX_CURVE_POINTS];
            fPrefs.getBytes("curve", tmp, sizeof(tmp));
            // Trust persisted curve if it has at least one valid breakpoint.
            uint8_t n = 0;
            for (; n < MAX_CURVE_POINTS; ++n) {
                if (tmp[n].wiper > 0x7F) break;
                curve[n] = tmp[n];
            }
            if (n > 0) curvePoints = n;
            else       loadDefaultCurve();
        } else {
            loadDefaultCurve();
        }
        fPrefs.end();

        initialized = true;

        // Verify the chip is present before proceeding. If the quiet write
        // fails we disable the module so loop() never polls a missing device.
        if (!setWiper(WIPER_QUIET)) {
            log_w("MCP4017 @ 0x%02X not present — FanController disabled", MCP4017_ADDR);
            initialized = false;
            return;
        }

        // Startup blast — drive the fan full for a moment to confirm it
        // unsticks. We do this unconditionally so the user gets feedback
        // that the new control path works.
        log_i("Startup blast: wiper=0x00 for %u ms", (unsigned)STARTUP_BLAST_MS);
        setWiper(WIPER_FULL);
        delay(STARTUP_BLAST_MS);

        // Drop into the configured idle state. The control loop will reach
        // for the curve as soon as it ticks.
        if (mode == MODE_MANUAL)    setWiper(manualWiper);
        else if (mode == MODE_OFF)  setWiper(WIPER_QUIET);
        else                        setWiper(WIPER_QUIET); // auto starts quiet

        lastCurveMs       = millis();
        lastTachoSampleMs = millis();
    }

    static void persistMode()
    {
        fPrefs.begin("fan", false);
        fPrefs.putUChar("mode",  (uint8_t)mode);
        fPrefs.putUChar("mwip",  manualWiper);
        fPrefs.putBool ("kick",  kickEnabled);
        fPrefs.end();
    }

    static void persistCurve()
    {
        CurvePoint blob[MAX_CURVE_POINTS] = {};
        for (uint8_t i = 0; i < curvePoints && i < MAX_CURVE_POINTS; ++i) {
            blob[i] = curve[i];
        }
        // Mark unused slots with an obviously-invalid sentinel so the loader
        // knows where to stop.
        for (uint8_t i = curvePoints; i < MAX_CURVE_POINTS; ++i) {
            blob[i].wiper      = 0xFF;
            blob[i].thresholdC = 0.0f;
        }
        fPrefs.begin("fan", false);
        fPrefs.putBytes("curve", blob, sizeof(blob));
        fPrefs.end();
    }

    void loop()
    {
        if (!initialized) return;
        uint32_t now = millis();

        sampleTacho(now);

        // End an in-progress kick.
        if (kickEndMs != 0 && (int32_t)(now - kickEndMs) >= 0) {
            kickEndMs = 0;
            setWiper(preKickWiper);
            log_i("Kick complete -> wiper=0x%02X", preKickWiper);
        }

        // Stall detection: if we commanded the fan ON (wiper < QUIET) but the
        // tacho has been silent for a while, flag it and (optionally) kick.
        bool commandedOn = (currentWiper < WIPER_QUIET);
        if (commandedOn && currentRpm < 1.0f && pinConfig.FAN_TACHO_PIN >= 0) {
            if (stallStartMs == 0) stallStartMs = now;
            if ((now - stallStartMs) >= KICK_STALL_GRACE_MS) {
                stalled = true;
                if (kickEnabled && kickEndMs == 0) {
                    preKickWiper = currentWiper;
                    setWiper(WIPER_FULL);
                    kickEndMs = now + KICK_DURATION_MS;
                    log_i("Stall detected — kicking from 0x%02X to 0x00",
                          preKickWiper);
                    // Reset stallStartMs so we don't immediately re-trigger.
                    stallStartMs = now;
                }
            }
        } else {
            stallStartMs = 0;
            stalled      = false;
        }

        // Curve / mode application (only when not mid-kick).
        if (kickEndMs == 0 && (now - lastCurveMs) >= CURVE_TICK_MS) {
            lastCurveMs = now;
            uint8_t targetWiper = currentWiper;

            switch (mode) {
                case MODE_AUTO: {
                    float tempC = Tmp102Controller::getMaxAmbientC();
                    targetWiper = evaluateCurve(tempC);
                    lastTempAppliedC = tempC;
                    break;
                }
                case MODE_MANUAL:
                    targetWiper = manualWiper;
                    break;
                case MODE_OFF:
                    targetWiper = WIPER_QUIET;
                    break;
            }

            if (targetWiper != currentWiper) {
                setWiper(targetWiper);
            }
        }
    }

    // ===== JSON I/O =====
    static const char *modeName(Mode m)
    {
        switch (m) {
            case MODE_AUTO:   return "auto";
            case MODE_MANUAL: return "manual";
            case MODE_OFF:    return "off";
        }
        return "?";
    }

    int act(cJSON *root)
    {
        int qid = cJsonTool::getJsonInt(root, "qid");
        cJSON *fan = cJSON_GetObjectItem(root, "fan");
        if (!fan || !cJSON_IsObject(fan)) {
            log_w("/fan_act: missing 'fan' object");
            return qid;
        }

        bool modeChanged  = false;
        bool curveChanged = false;

        cJSON *jmode = cJSON_GetObjectItem(fan, "mode");
        if (jmode && cJSON_IsString(jmode)) {
            const char *s = jmode->valuestring;
            if      (!strcasecmp(s, "auto"))   { mode = MODE_AUTO;   modeChanged = true; }
            else if (!strcasecmp(s, "manual")) { mode = MODE_MANUAL; modeChanged = true; }
            else if (!strcasecmp(s, "off"))    { mode = MODE_OFF;    modeChanged = true; }
            else log_w("/fan_act: unknown mode '%s'", s);
        }

        cJSON *jwiper = cJSON_GetObjectItem(fan, "wiper");
        if (jwiper && cJSON_IsNumber(jwiper)) {
            int w = jwiper->valueint;
            if (w < 0) w = 0; if (w > 0x7F) w = 0x7F;
            manualWiper = (uint8_t)w;
            // If the caller set a wiper without specifying a mode, assume manual.
            if (!jmode) { mode = MODE_MANUAL; modeChanged = true; }
            modeChanged = true;
        }

        cJSON *jkick = cJSON_GetObjectItem(fan, "kick");
        if (jkick && (cJSON_IsBool(jkick) || cJSON_IsNumber(jkick))) {
            kickEnabled = cJSON_IsTrue(jkick) ||
                          (cJSON_IsNumber(jkick) && jkick->valueint != 0);
            modeChanged = true;
        }

        cJSON *jcurve = cJSON_GetObjectItem(fan, "curve");
        if (jcurve && cJSON_IsArray(jcurve)) {
            uint8_t n = 0;
            CurvePoint tmp[MAX_CURVE_POINTS];
            int sz = cJSON_GetArraySize(jcurve);
            for (int i = 0; i < sz && n < MAX_CURVE_POINTS; ++i) {
                cJSON *pair = cJSON_GetArrayItem(jcurve, i);
                if (!pair || !cJSON_IsArray(pair) || cJSON_GetArraySize(pair) < 2) continue;
                cJSON *jt = cJSON_GetArrayItem(pair, 0);
                cJSON *jw = cJSON_GetArrayItem(pair, 1);
                if (!jt || !jw || !cJSON_IsNumber(jt) || !cJSON_IsNumber(jw)) continue;
                int w = jw->valueint;
                if (w < 0) w = 0; if (w > 0x7F) w = 0x7F;
                tmp[n].thresholdC = (float)jt->valuedouble;
                tmp[n].wiper      = (uint8_t)w;
                ++n;
            }
            if (n > 0) {
                curvePoints = n;
                for (uint8_t i = 0; i < n; ++i) curve[i] = tmp[i];
                curveChanged = true;
            } else {
                log_w("/fan_act: empty/invalid curve, keeping current");
            }
        }

        // Apply immediately so the caller sees the change without waiting for
        // the next CURVE_TICK_MS.
        if (mode == MODE_MANUAL)     setWiper(manualWiper);
        else if (mode == MODE_OFF)   setWiper(WIPER_QUIET);
        // MODE_AUTO: let the loop pick up next tick.

        if (modeChanged)  persistMode();
        if (curveChanged) persistCurve();

        log_i("/fan_act: mode=%s wiper=0x%02X manual=0x%02X kick=%d curvePts=%u",
              modeName(mode), currentWiper, manualWiper, kickEnabled, curvePoints);
        return qid;
    }

    cJSON *get(cJSON *root)
    {
        cJSON *ret = cJSON_CreateObject();
        cJSON *fan = cJSON_CreateObject();

        cJSON_AddStringToObject(fan, "mode", modeName(mode));
        cJsonTool::setJsonInt(fan, "wiper",      currentWiper);
        cJsonTool::setJsonInt(fan, "manual",     manualWiper);
        cJsonTool::setJsonFloat(fan, "rpm",      currentRpm);
        cJsonTool::setJsonInt(fan, "stalled",    stalled ? 1 : 0);
        cJsonTool::setJsonInt(fan, "kick",       kickEnabled ? 1 : 0);
        cJsonTool::setJsonFloat(fan, "tempC",    Tmp102Controller::getMaxAmbientC());

        cJSON *jcurve = cJSON_CreateArray();
        for (uint8_t i = 0; i < curvePoints; ++i) {
            cJSON *pair = cJSON_CreateArray();
            cJSON_AddItemToArray(pair, cJSON_CreateNumber(curve[i].thresholdC));
            cJSON_AddItemToArray(pair, cJSON_CreateNumber(curve[i].wiper));
            cJSON_AddItemToArray(jcurve, pair);
        }
        cJSON_AddItemToObject(fan, "curve", jcurve);

        cJSON_AddItemToObject(ret, "fan", fan);
        return ret;
    }
}

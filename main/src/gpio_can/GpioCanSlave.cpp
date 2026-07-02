#include "GpioCanSlave.h"

#ifdef GPIO_CAN_SLAVE_CONTROLLER

#include <Arduino.h>
#include <Preferences.h>
#include "PinConfig.h"
#include "../digitalin/DigitalInController.h"
#include "../digitalout/DigitalOutController.h"
#include "cJsonTool.h"

#ifdef CAN_CONTROLLER_CANOPEN
#include "../canopen/CANopenModule.h"  // provides extern CO_t* CO
extern "C" {
#include <CANopen.h>
#include "OD.h"
}
#endif

namespace GpioCanSlave
{
    static const char* NVS_NS       = "uc2";
    static const char* NVS_KEY_THR  = "gpioCT";
    static const char* NVS_KEY_REF  = "gpioCR";
    static const char* NVS_KEY_SENS = "gpioCS";

    // ── Collision-detector config (NVS-backed) ─────────────────────────
    static uint16_t s_reference   = 0;   // idle baseline; 0 = auto-seed pending
    static uint16_t s_threshold   = 0;   // deviation band (counts)
    static uint8_t  s_sensitivity = 4;   // consecutive samples to trip/clear

    // ── Detector state ─────────────────────────────────────────────────
    static uint16_t s_filtered    = 0;   // fast EWMA (detection input)
    static uint32_t s_meanX16     = 0;   // slow rolling mean, 4 fractional bits
    static bool     s_meanSeeded  = false;
    static bool     s_trip        = false;
    static uint8_t  s_outCount    = 0;   // consecutive out-of-band samples
    static uint8_t  s_inCount     = 0;   // consecutive in-band samples
    static uint16_t s_bootSamples = 0;   // samples seen since boot (for auto-seed)

    // ── TPDO edge tracking ──────────────────────────────────────────────
    static uint8_t  s_lastDigByte0 = 0xFF;
    static uint8_t  s_lastDigByte3 = 0xFF;
    static uint8_t  s_lastOut1Cmd  = 0xFF;
    static uint8_t  s_lastOut2Cmd  = 0xFF;
    static uint32_t s_lastReadUs   = 0;

    // 50 Hz sampling: with sensitivity 4 a collision is confirmed in ~80 ms,
    // and single-sample ADC glitches (e.g. raw 55 or 29 in an otherwise
    // ~585-count idle trace) never accumulate enough votes to trip.
    static constexpr uint32_t READ_PERIOD_US = 20000;

    // Slow-mean smoothing: mean += (sample - mean) / 64 at 50 Hz → time
    // constant ≈ 1.3 s. Slow enough to be a stable calibration source, fast
    // enough that the polled value settles within a few seconds of touching
    // the sensor mount.
    static constexpr uint8_t MEAN_SHIFT = 6;

    // Auto-seed the reference after ~2 s of boot samples if never calibrated.
    static constexpr uint16_t BOOT_SEED_SAMPLES = 100;

    static inline uint16_t meanValue() { return (uint16_t)(s_meanX16 >> 4); }

    static inline bool canopenReady()
    {
#ifdef CAN_CONTROLLER_CANOPEN
        return CO != nullptr && !CO->nodeIdUnconfigured && CO->CANmodule->CANnormal;
#else
        return false;
#endif
    }

    // ── NVS persistence ─────────────────────────────────────────────────
    static void persistU16(const char* key, uint16_t v)
    {
        Preferences p;
        if (p.begin(NVS_NS, false)) { p.putUShort(key, v); p.end(); }
    }
    static void persistU8(const char* key, uint8_t v)
    {
        Preferences p;
        if (p.begin(NVS_NS, false)) { p.putUChar(key, v); p.end(); }
    }

    uint16_t getReference()  { return s_reference; }
    uint16_t getThreshold()  { return s_threshold; }
    uint8_t  getSensitivity(){ return s_sensitivity; }

    void setReference(uint16_t value)
    {
        if (value == s_reference) return;
        s_reference = value;
        persistU16(NVS_KEY_REF, value);
#ifdef CAN_CONTROLLER_CANOPEN
        OD_RAM.x2330_collision_reference = value;
#endif
        log_i("GpioCanSlave reference -> %u", (unsigned)value);
    }

    void setThreshold(uint16_t value)
    {
        if (value == s_threshold) return;
        s_threshold = value;
        persistU16(NVS_KEY_THR, value);
#ifdef CAN_CONTROLLER_CANOPEN
        OD_RAM.x2331_collision_threshold = value;
#endif
        log_i("GpioCanSlave threshold -> %u", (unsigned)value);
    }

    void setSensitivity(uint8_t value)
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

    uint16_t calibrate()
    {
        uint16_t m = meanValue();
        log_i("GpioCanSlave calibrate: reference %u -> %u (rolling mean)",
              (unsigned)s_reference, (unsigned)m);
        setReference(m);
        return m;
    }

    static void enableTpdo2()
    {
#ifdef CAN_CONTROLLER_CANOPEN
        // Clear bit 31 of TPDO2 COB-ID so the stack starts emitting it.
        // Bit 30 stays set so CANopenNode adds the node-id at activation.
        // (OD.c default = 0xC0000280 — invalid; we want 0x40000280.)
        OD_PERSIST_COMM.x1801_TPDOCommunicationParameter.COB_IDUsedByTPDO = 0x40000280u;
#endif
    }

    void setup()
    {
        log_i("GpioCanSlave setup");

        // Load collision config from NVS, falling back to pinConfig defaults.
        {
            Preferences p;
            bool ok = p.begin(NVS_NS, true);
            s_threshold   = ok ? p.getUShort(NVS_KEY_THR, pinConfig.GPIO_COLLISION_THRESHOLD_DEFAULT)
                               : pinConfig.GPIO_COLLISION_THRESHOLD_DEFAULT;
            s_reference   = ok ? p.getUShort(NVS_KEY_REF, 0) : 0;  // 0 = auto-seed
            s_sensitivity = ok ? p.getUChar(NVS_KEY_SENS, pinConfig.GPIO_COLLISION_SENSITIVITY_DEFAULT)
                               : pinConfig.GPIO_COLLISION_SENSITIVITY_DEFAULT;
            if (ok) p.end();
        }
        if (s_sensitivity < 1) s_sensitivity = 1;
        log_i("GpioCanSlave collision cfg: reference=%u (0=auto-seed) threshold=%u sensitivity=%u",
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
#endif

        // Enable TPDO2 broadcasting. Must happen before canopenModule.setup()
        // builds its TPDO descriptors (see main.cpp ordering).
        enableTpdo2();
    }

    static uint8_t readDigital(int8_t pin, bool activeLow)
    {
        if (pin < 0) return 0;
        int raw = digitalRead(pin);
        return activeLow ? (raw == 0 ? 1 : 0) : (raw != 0 ? 1 : 0);
    }

    static void updateOutputsFromOd()
    {
#ifdef CAN_CONTROLLER_CANOPEN
        uint8_t cmd1 = OD_RAM.x2301_digital_output_command[0];
        uint8_t cmd2 = OD_RAM.x2301_digital_output_command[1];

        // Pulse semantics: master writes 0xFF as a "trigger" sentinel. We
        // forward that to DigitalOutController::act with val=-1 which fires
        // a brief HIGH-LOW pulse. Otherwise treat as direct level.
        auto applyToOut = [&](int outId, uint8_t newCmd, uint8_t& last) {
            if (newCmd == last) return;
            last = newCmd;
            cJSON* doc = cJSON_CreateObject();
            if (!doc) return;
            cJsonTool::setJsonInt(doc, "digitaloutid", outId);
            cJsonTool::setJsonInt(doc, "digitaloutval",
                                  (newCmd == 0xFF) ? -1 : (int)newCmd);
            DigitalOutController::act(doc);
            cJSON_Delete(doc);
            log_i("GpioCanSlave OUT%d <- %u (from OD x2301)", outId, (unsigned)newCmd);
        };

        applyToOut(1, cmd1, s_lastOut1Cmd);
        applyToOut(2, cmd2, s_lastOut2Cmd);
#endif
    }

    // Master SDO writes land directly in OD_RAM — detect changes and apply
    // them to the local (NVS-backed) config. Also execute the command byte.
    static void applyConfigFromOd()
    {
#ifdef CAN_CONTROLLER_CANOPEN
        uint16_t odRef  = OD_RAM.x2330_collision_reference;
        uint16_t odThr  = OD_RAM.x2331_collision_threshold;
        uint8_t  odSens = OD_RAM.x2332_collision_sensitivity;
        if (odRef  != s_reference)   setReference(odRef);
        if (odThr  != s_threshold)   setThreshold(odThr);
        if (odSens != s_sensitivity) setSensitivity(odSens);

        uint8_t cmd = OD_RAM.x2333_collision_command;
        if (cmd != 0) {
            OD_RAM.x2333_collision_command = 0;
            if (cmd == 1) {
                calibrate();
            } else {
                log_w("GpioCanSlave unknown collision command %u", (unsigned)cmd);
            }
        }
#endif
    }

    void loop()
    {
#ifdef CAN_CONTROLLER_CANOPEN
        if (!canopenReady()) return;

        uint32_t nowUs = (uint32_t)esp_timer_get_time();
        if ((nowUs - s_lastReadUs) < READ_PERIOD_US) {
            updateOutputsFromOd();
            applyConfigFromOd();
            return;
        }
        s_lastReadUs = nowUs;

        // --- Digital inputs ------------------------------------------------
        uint8_t estop = readDigital(pinConfig.GPIO_ESTOP_PIN, /*activeLow=*/true);

        // --- Collision detection (baseline-deviation vote) -----------------
        uint16_t raw = 0;
        if (pinConfig.GPIO_COLLISION_ADC >= 0) {
            raw = (uint16_t)analogRead(pinConfig.GPIO_COLLISION_ADC);
            s_bootSamples++;

            // Fast EWMA for detection: filtered += alpha * (raw - filtered)
            uint32_t alpha = pinConfig.GPIO_ADC_FILTER_ALPHA_X1024;
            int32_t  delta = (int32_t)raw - (int32_t)s_filtered;
            s_filtered = (uint16_t)((int32_t)s_filtered + (delta * (int32_t)alpha) / 1024);

            uint16_t dev = (s_filtered > s_reference)
                           ? (uint16_t)(s_filtered - s_reference)
                           : (uint16_t)(s_reference - s_filtered);
            bool outOfBand = (s_reference != 0) && (dev > s_threshold);

            // Consecutive-sample vote: `sensitivity` out-of-band samples trip,
            // `sensitivity` in-band samples clear. A lone spike resets to 0 on
            // the next sample and never confirms.
            // log all values for now 
            log_i("GpioCanSlave collision ADC read: raw=%u filt=%u ref=%u dev=%u thr=%u trip=%d outCount=%u inCount=%u",
                  (unsigned)raw, (unsigned)s_filtered, (unsigned)s_reference,
                  (unsigned)dev, (unsigned)s_threshold, s_trip ? 1 : 0,
                  (unsigned)s_outCount, (unsigned)s_inCount);
            if (outOfBand) {
                s_inCount = 0;
                if (s_outCount < 0xFF) s_outCount++;
                if (!s_trip && s_outCount >= s_sensitivity) {
                    s_trip = true;
                    log_i("GpioCanSlave COLLISION TRIP (filt=%u ref=%u dev=%u thr=%u after %u samples)",
                          (unsigned)s_filtered, (unsigned)s_reference,
                          (unsigned)dev, (unsigned)s_threshold, (unsigned)s_outCount);
                }
            } else {
                s_outCount = 0;
                if (s_inCount < 0xFF) s_inCount++;
                if (s_trip && s_inCount >= s_sensitivity) {
                    s_trip = false;
                    log_i("GpioCanSlave collision cleared (filt=%u ref=%u)",
                          (unsigned)s_filtered, (unsigned)s_reference);
                }
            }

            // Slow rolling mean — the calibration/diagnostic value. Freeze it
            // while a deviation is being counted or a trip is active so a
            // collision cannot drag the baseline towards itself.
            if (!s_trip && s_outCount == 0) {
                if (!s_meanSeeded) {
                    s_meanX16 = ((uint32_t)s_filtered) << 4;
                    s_meanSeeded = true;
                } else {
                    int32_t mdelta = ((int32_t)s_filtered << 4) - (int32_t)s_meanX16;
                    s_meanX16 = (uint32_t)((int32_t)s_meanX16 + (mdelta >> MEAN_SHIFT));
                }
            }

            // Auto-seed: if never calibrated (reference 0), take the rolling
            // mean after the boot settling window. Not persisted — an explicit
            // calibrate (or reference write) is what commits to NVS, so a
            // sensor swap doesn't inherit a stale baseline forever.
            if (s_reference == 0 && s_meanSeeded && s_bootSamples >= BOOT_SEED_SAMPLES) {
                s_reference = meanValue();
                OD_RAM.x2330_collision_reference = s_reference;
                log_i("GpioCanSlave auto-seeded reference = %u (uncalibrated boot)",
                      (unsigned)s_reference);
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

        if (edge) {
            uint8_t prevEstop = s_lastDigByte0;
            uint8_t prevFlags = s_lastDigByte3;
            s_lastDigByte0 = estop;
            s_lastDigByte3 = flags;
            if (CO != nullptr && CO->TPDO != nullptr) {
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
                log_d("GpioCanSlave idle  estop=%u flags=0x%02X filt=%u raw=%u mean=%u ref=%u thr=%u sens=%u trip=%d",
                      (unsigned)estop, (unsigned)flags,
                      (unsigned)s_filtered, (unsigned)raw,
                      (unsigned)meanValue(), (unsigned)s_reference,
                      (unsigned)s_threshold, (unsigned)s_sensitivity,
                      s_trip ? 1 : 0);
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
    cJSON* act(cJSON* doc)
    {
        int qid = cJsonTool::getJsonInt(doc, "qid");

        cJSON* thr = cJSON_GetObjectItemCaseSensitive(doc, "threshold");
        if (thr && cJSON_IsNumber(thr)) {
            setThreshold((uint16_t)thr->valueint);
            log_i("GpioCanSlave act qid=%d threshold=%d", qid, thr->valueint);
        }
        cJSON* sens = cJSON_GetObjectItemCaseSensitive(doc, "sensitivity");
        if (sens && cJSON_IsNumber(sens)) {
            setSensitivity((uint8_t)sens->valueint);
            log_i("GpioCanSlave act qid=%d sensitivity=%d", qid, sens->valueint);
        }
        cJSON* ref = cJSON_GetObjectItemCaseSensitive(doc, "reference");
        if (ref && cJSON_IsNumber(ref)) {
            setReference((uint16_t)ref->valueint);
            log_i("GpioCanSlave act qid=%d reference=%d", qid, ref->valueint);
        }
        cJSON* cal = cJSON_GetObjectItemCaseSensitive(doc, "calibrate");
        if (cal && cJSON_IsNumber(cal) && cal->valueint != 0) {
            uint16_t m = calibrate();
            log_i("GpioCanSlave act qid=%d calibrated reference=%u", qid, (unsigned)m);
        }

        return get(doc);  // answer with the resulting state
    }

    cJSON* get(cJSON* doc)
    {
        int qid = doc ? cJsonTool::getJsonInt(doc, "qid") : 0;
        cJSON* out = cJSON_CreateObject();
        if (!out) return nullptr;
        cJSON* g = cJSON_CreateObject();
        cJSON_AddItemToObject(out, "gpio", g);
        cJsonTool::setJsonInt(g, "mean",        (int)meanValue());
        cJsonTool::setJsonInt(g, "filtered",    (int)s_filtered);
        cJsonTool::setJsonInt(g, "reference",   (int)s_reference);
        cJsonTool::setJsonInt(g, "threshold",   (int)s_threshold);
        cJsonTool::setJsonInt(g, "sensitivity", (int)s_sensitivity);
        cJsonTool::setJsonInt(g, "trip",        s_trip ? 1 : 0);
        cJsonTool::setJsonInt(g, "estop",       (pinConfig.GPIO_ESTOP_PIN >= 0)
                                                ? (digitalRead(pinConfig.GPIO_ESTOP_PIN) == 0 ? 1 : 0)
                                                : 0);
        cJsonTool::setJsonInt(out, "qid", qid);
        return out;
    }
}

#endif // GPIO_CAN_SLAVE_CONTROLLER

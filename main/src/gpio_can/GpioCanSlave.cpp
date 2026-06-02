#include "GpioCanSlave.h"
#include "PinConfig.h"

#ifdef GPIO_CAN_SLAVE_CONTROLLER

#include <Arduino.h>
#include <Preferences.h>
#include "../digitalin/DigitalInController.h"
#include "../digitalout/DigitalOutController.h"
#include "cJsonTool.h"
#include "../../JsonKeys.h"

#ifdef CAN_CONTROLLER_CANOPEN
#include "../canopen/CANopenModule.h"  // provides extern CO_t* CO
extern "C" {
#include <CANopen.h>
#include "OD.h"
}
#endif

namespace GpioCanSlave
{
    static const char* NVS_NS  = "uc2";
    static const char* NVS_KEY = "gpioCollThr";

    static uint16_t s_threshold     = 0;
    static uint16_t s_filtered      = 0;
    static bool     s_trip          = false;
    static uint8_t  s_lastDigByte0  = 0xFF;
    static uint8_t  s_lastDigByte3  = 0xFF;
    static uint16_t s_lastAdcFilt   = 0xFFFF;
    static uint8_t  s_lastOut1Cmd   = 0xFF;
    static uint8_t  s_lastOut2Cmd   = 0xFF;
    static uint32_t s_lastReadUs    = 0;

    // Read interval for the collision ADC — 50 Hz is plenty for an end-stop
    // type sensor and keeps the EWMA from saturating on noise.
    static constexpr uint32_t READ_PERIOD_US = 20000;

    static inline bool canopenReady()
    {
#ifdef CAN_CONTROLLER_CANOPEN
        return CO != nullptr && !CO->nodeIdUnconfigured && CO->CANmodule->CANnormal;
#else
        return false;
#endif
    }

    uint16_t getThreshold() { return s_threshold; }

    void setThreshold(uint16_t value)
    {
        if (value == s_threshold) return;
        s_threshold = value;
        Preferences p;
        if (p.begin(NVS_NS, false)) {
            p.putUShort(NVS_KEY, value);
            p.end();
        }
        log_i("GpioCanSlave threshold updated to %u", (unsigned)value);
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

        // Load threshold from NVS, fall back to the pinConfig default.
        Preferences p;
        if (p.begin(NVS_NS, true)) {
            s_threshold = p.getUShort(NVS_KEY, pinConfig.GPIO_COLLISION_THRESHOLD_DEFAULT);
            p.end();
        } else {
            s_threshold = pinConfig.GPIO_COLLISION_THRESHOLD_DEFAULT;
        }
        log_i("GpioCanSlave threshold = %u (default %u)",
              (unsigned)s_threshold,
              (unsigned)pinConfig.GPIO_COLLISION_THRESHOLD_DEFAULT);

        // E-stop input — momentary-to-GND with internal pull-up.
        if (pinConfig.GPIO_ESTOP_PIN >= 0) {
            pinMode(pinConfig.GPIO_ESTOP_PIN, INPUT_PULLUP);
            log_i("GpioCanSlave E-stop on GPIO%d (pull-up)", pinConfig.GPIO_ESTOP_PIN);
        }

        // Collision ADC — pin is already analogRead-capable on ESP32-S3.
        if (pinConfig.GPIO_COLLISION_ADC >= 0) {
            analogReadResolution(12);
            // Default attenuation gives ~0..3.3V range on S3.
            analogSetPinAttenuation(pinConfig.GPIO_COLLISION_ADC, ADC_11db);
            log_i("GpioCanSlave collision ADC on GPIO%d", pinConfig.GPIO_COLLISION_ADC);
        }

        // DigitalOutController::setup() has already configured DIGITAL_OUT_1/2
        // as OUTPUT and pulsed them — we just consume x2301 from here on.

        // Enable TPDO2 broadcasting. Must happen before CANopenNode finishes
        // initialising its TPDOs; in main.cpp we call GpioCanSlave::setup()
        // before canopenModule.setup() (see main.cpp wiring below).
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
    /*
        DIGITAL_OUT_1 and DIGITAL_OUT_2 are driven by writes to OD
        x2301_digital_output_command[0] and [1] respectively. They are also
        settable via the slave's local serial /digitalout_act API.
        E.g. the master can write 0xFF as a "trigger" sentinel for pulse 
        semantics, or any other value for direct level.
    */
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
            cJsonTool::setJsonInt(doc, keyDigitalOutid, outId);
            cJsonTool::setJsonInt(doc, keyDigitalOutVal,
                                  (newCmd == 0xFF) ? -1 : (int)newCmd);
            DigitalOutController::act(doc);
            cJSON_Delete(doc);
            log_i("GpioCanSlave OUT%d <- %u (from OD x2301)", outId, (unsigned)newCmd);
        };

        applyToOut(1, cmd1, s_lastOut1Cmd);
        applyToOut(2, cmd2, s_lastOut2Cmd);
#endif
    }

    void loop()
    {
#ifdef CAN_CONTROLLER_CANOPEN
        if (!canopenReady()) return;

        uint32_t nowUs = (uint32_t)esp_timer_get_time();
        if ((nowUs - s_lastReadUs) < READ_PERIOD_US) {
            updateOutputsFromOd();
            return;
        }
        s_lastReadUs = nowUs;

        // --- Digital inputs ------------------------------------------------
        uint8_t estop = readDigital(pinConfig.GPIO_ESTOP_PIN, /*activeLow=*/true);

        // --- Analog (collision sensor) -------------------------------------
        uint16_t raw = 0;
        if (pinConfig.GPIO_COLLISION_ADC >= 0) {
            raw = (uint16_t)analogRead(pinConfig.GPIO_COLLISION_ADC);
            // EWMA: filtered = filtered + alpha * (raw - filtered)
            uint32_t alpha = pinConfig.GPIO_ADC_FILTER_ALPHA_X1024;
            int32_t  delta = (int32_t)raw - (int32_t)s_filtered;
            s_filtered = (uint16_t)((int32_t)s_filtered + (delta * (int32_t)alpha) / 1024);

            // Schmitt trigger around the configured threshold.
            uint16_t hi = (uint16_t)(s_threshold + pinConfig.GPIO_COLLISION_HYSTERESIS);
            uint16_t lo = (s_threshold > pinConfig.GPIO_COLLISION_HYSTERESIS)
                          ? (uint16_t)(s_threshold - pinConfig.GPIO_COLLISION_HYSTERESIS)
                          : 0;
            if (!s_trip && s_filtered >= hi) {
                s_trip = true;
                log_i("GpioCanSlave COLLISION TRIP (filt=%u thr=%u)",
                      (unsigned)s_filtered, (unsigned)s_threshold);
            } else if (s_trip && s_filtered <= lo) {
                s_trip = false;
                log_i("GpioCanSlave collision cleared (filt=%u)", (unsigned)s_filtered);
            }
        }

        // --- Compose status flags byte ------------------------------------
        // bit 0 = collision-trip, bit 1 = E-stop active
        uint8_t flags = 0;
        if (s_trip) flags |= 0x01;
        if (estop)  flags |= 0x02;

        // --- Push into OD --------------------------------------------------
        OD_RAM.x2300_digital_input_state[0] = estop;
        // sub 2/3 can carry additional inputs later — leave as 0 for now.
        OD_RAM.x2300_digital_input_state[3] = flags;
        OD_RAM.x2310_analog_input_value[0]  = s_filtered;
        OD_RAM.x2310_analog_input_value[1]  = raw;

        // --- Drive remote outputs -----------------------------------------
        updateOutputsFromOd();

        // --- Trigger TPDO2 only on edge changes ---------------------------
        // We INTENTIONALLY exclude `s_filtered != s_lastAdcFilt` from this
        // predicate. The filtered ADC value is in the payload (so the master
        // sees the level at the moment the event fires), but it must not
        // drive the send — otherwise every ADC sample becomes a CAN frame
        // and the bus is flooded. The Schmitt-trigger inside the ADC block
        // above is the only thing allowed to flip `s_trip`, which in turn
        // sets bit 0 of `flags`, which is what triggers a send here.
        //
        // Edge sources we DO send on:
        //   - E-stop digital level changed (estop byte)
        //   - status flags byte changed (collision-trip enter/leave OR
        //     E-stop bit). This catches both the resistance-threshold trip
        //     and an E-stop press in one comparison.
        // CANopenNode's own TPDO event-timer (x1801 sub 5 = 1000 ms) gives
        // the master a periodic heartbeat regardless, so the master can
        // distinguish "slave alive, idle" from "slave dropped off the bus".
        bool edge =
            (estop != s_lastDigByte0) ||
            (flags != s_lastDigByte3);

        s_lastAdcFilt = s_filtered;  // tracked for diagnostic logging only

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
            // slave is reading ADC even though it isn't sending CAN frames.
            static uint32_t s_lastDiagMs = 0;
            uint32_t nowMs = (uint32_t)(esp_timer_get_time() / 1000ULL);
            if ((nowMs - s_lastDiagMs) >= 5000) {
                s_lastDiagMs = nowMs;
                log_d("GpioCanSlave idle  estop=%u flags=0x%02X filt=%u raw=%u thr=%u trip=%d",
                      (unsigned)estop, (unsigned)flags,
                      (unsigned)s_filtered, (unsigned)raw,
                      (unsigned)s_threshold, s_trip ? 1 : 0);
            }
        }
#endif
    }

    // ------------------------------------------------------------------
    // Optional serial JSON entry-point — lets us tune the threshold while
    // bench-testing the slave over USB before it goes on the CAN bus.
    //
    //   {"task":"/gpio_act", "threshold":1800}
    //   {"task":"/gpio_get"}
    // ------------------------------------------------------------------
    int act(cJSON* doc)
    {
        int qid = cJsonTool::getJsonInt(doc, "qid");
        uint16_t prevThr = s_threshold;
        cJSON* thr = cJSON_GetObjectItemCaseSensitive(doc, "threshold");
        if (thr && cJSON_IsNumber(thr)) {
            uint16_t newThr = (uint16_t)thr->valueint;
            setThreshold(newThr);
            log_i("GpioCanSlave act qid=%d threshold %u -> %u (filt=%u trip=%d)",
                  qid, (unsigned)prevThr, (unsigned)newThr,
                  (unsigned)s_filtered, s_trip ? 1 : 0);
        } else {
            log_i("GpioCanSlave act qid=%d (no-op: no 'threshold' field)", qid);
        }
        return qid;
    }

    cJSON* get(cJSON* /*doc*/)
    {
        cJSON* out = cJSON_CreateObject();
        if (!out) return nullptr;
        cJSON* g = cJSON_CreateObject();
        cJSON_AddItemToObject(out, "gpio", g);
        cJsonTool::setJsonInt(g, "threshold", (int)s_threshold);
        cJsonTool::setJsonInt(g, "filtered",  (int)s_filtered);
        cJsonTool::setJsonInt(g, "trip",      s_trip ? 1 : 0);
        cJsonTool::setJsonInt(g, "estop",     (pinConfig.GPIO_ESTOP_PIN >= 0)
                                              ? (digitalRead(pinConfig.GPIO_ESTOP_PIN) == 0 ? 1 : 0)
                                              : 0);
        return out;
    }
}

#endif // GPIO_CAN_SLAVE_CONTROLLER

#include "TMCController.h"


using namespace FocusMotor;

namespace TMCController
{
    // TMC2209 instance
    TMC2209Stepper driver(&Serial1, R_SENSE, DRIVER_ADDRESS);


    uint32_t tstepFromStepsPerSecond(uint32_t stepsPerSecond, uint16_t microsteps)
    {
        // 0 = "never switch" — the register semantics already encode that
        // (TSTEP >= threshold selects StealthChop, and TSTEP is never < 0).
        if (stepsPerSecond == 0)
            return 0;
        if (microsteps == 0)
            microsteps = 1;
        // One input step equals (256 / microsteps) of the 1/256 microsteps the
        // TSTEP timer counts, hence the microsteps factor in the numerator.
        uint64_t tstep = ((uint64_t)TMC_FCLK_HZ * (uint64_t)microsteps) /
                         (256ULL * (uint64_t)stepsPerSecond);
        if (tstep > 0xFFFFF) // 20-bit register
            tstep = 0xFFFFF;
        return (uint32_t)tstep;
    }

    static void writeParamsToPreferences(const TMCData &p)
    {
        preferences.begin("tmc", false);
        preferences.putInt("msteps", p.msteps);
        preferences.putInt("current", p.rms_current);
        preferences.putInt("stall", p.stall_value);
        preferences.putInt("sgthrs", p.sgthrs);
        preferences.putInt("semin", p.semin);
        preferences.putInt("semax", p.semax);
        preferences.putInt("sedn", p.sedn);
        preferences.putInt("tcool", p.tcoolthrs);
        preferences.putInt("blank", p.blank_time);
        preferences.putInt("toff", p.toff);
        preferences.putInt("tpwmsps", p.tpwmthrs_sps);
        preferences.putInt("enspread", p.en_spreadcycle);
        preferences.putInt("holdmult", p.hold_mult_pct);
        preferences.putInt("hstrt", p.hstrt);
        preferences.putInt("hend", p.hend);
        preferences.putInt("ver", TMC_SETTINGS_VERSION);
        preferences.end();
        log_i("TMC2209 settings saved to preferences: msteps: %i, current: %i, stall: %i, sgthrs: %i, semin: %i, semax: %i, sedn: %i, tcool: %i, blank: %i, toff: %i, tpwmthrs_sps: %i, en_spreadcycle: %i, hold_mult: %i%%",
              p.msteps, p.rms_current, p.stall_value, p.sgthrs, p.semin, p.semax, p.sedn, p.tcoolthrs, p.blank_time, p.toff,
              (int)p.tpwmthrs_sps, p.en_spreadcycle, p.hold_mult_pct);
    }

    static TMCData readParamsFromPreferences()
    {
        TMCData p;
        preferences.begin("tmc", true);
        p.msteps = preferences.getInt("msteps", pinConfig.tmc_microsteps);
        p.rms_current = preferences.getInt("current", pinConfig.tmc_rms_current);
        p.stall_value = preferences.getInt("stall", pinConfig.tmc_stall_value);
        p.sgthrs = preferences.getInt("sgthrs", pinConfig.tmc_sgthrs);
        p.semin = preferences.getInt("semin", pinConfig.tmc_semin);
        p.semax = preferences.getInt("semax", pinConfig.tmc_semax);
        p.sedn = preferences.getInt("sedn", pinConfig.tmc_sedn);
        p.tcoolthrs = preferences.getInt("tcool", pinConfig.tmc_tcoolthrs);
        p.blank_time = preferences.getInt("blank", pinConfig.tmc_blank_time);
        p.toff = preferences.getInt("toff", pinConfig.tmc_toff);
        p.tpwmthrs_sps = preferences.getInt("tpwmsps", pinConfig.tmc_tpwmthrs_sps);
        p.en_spreadcycle = preferences.getInt("enspread", pinConfig.tmc_en_spreadcycle);
        p.hold_mult_pct = preferences.getInt("holdmult", pinConfig.tmc_hold_multiplier_pct);
        p.hstrt = preferences.getInt("hstrt", pinConfig.tmc_hstrt);
        p.hend = preferences.getInt("hend", pinConfig.tmc_hend);
        preferences.end();
        return p;
    }

    static TMCData paramsFromPinConfig()
    {
        TMCData p;
        p.msteps = pinConfig.tmc_microsteps;
        p.rms_current = pinConfig.tmc_rms_current;
        p.stall_value = pinConfig.tmc_stall_value;
        p.sgthrs = pinConfig.tmc_sgthrs;
        p.semin = pinConfig.tmc_semin;
        p.semax = pinConfig.tmc_semax;
        p.sedn = pinConfig.tmc_sedn;
        p.tcoolthrs = pinConfig.tmc_tcoolthrs;
        p.blank_time = pinConfig.tmc_blank_time;
        p.toff = pinConfig.tmc_toff;
        p.tpwmthrs_sps = pinConfig.tmc_tpwmthrs_sps;
        p.en_spreadcycle = pinConfig.tmc_en_spreadcycle;
        p.hold_mult_pct = pinConfig.tmc_hold_multiplier_pct;
        p.hstrt = pinConfig.tmc_hstrt;
        p.hend = pinConfig.tmc_hend;
        return p;
    }

    // NVS wins over pinConfig on every boot, so a board that has run older
    // firmware keeps its stored values forever — which is exactly how a changed
    // firmware default (e.g. CoolStep off) can fail to reach the hardware.
    // Re-seed from pinConfig whenever the settings version moves.
    static void migratePreferencesIfNeeded()
    {
        preferences.begin("tmc", true);
        int storedVersion = preferences.getInt("ver", 0);
        preferences.end();
        if (storedVersion == TMC_SETTINGS_VERSION)
            return;
        log_w("TMC settings version %d -> %d: re-seeding NVS from PinConfig",
              storedVersion, TMC_SETTINGS_VERSION);
        writeParamsToPreferences(paramsFromPinConfig());
    }

    void applyParamsToDriver(const TMCData &p, bool saveToPrefs)
    {
        #if !defined(CAN_CONTROLLER_CANOPEN) || (NODE_ROLE == 2)
        // Temporarily disable motor to allow microstep changes
        digitalWrite(pinConfig.MOTOR_ENABLE, HIGH);
        delay(10);

        // TMCStepper's microsteps(ms) setter only accepts {256,128,64,32,16,8,4,2,0}
        // where 0 means "full step" (i.e. 1 microstep). Passing literal 1 is a no-op
        // (silently ignored by the switch in the library), which left msteps_ at the
        // previous value. Translate 1 -> 0 for the API while keeping p.msteps == 1
        // for preferences/log/verify (the getter returns 256>>MRES, which is 1 for MRES=8).
        uint16_t apiMs = (p.msteps == 1) ? 0 : (uint16_t)p.msteps;

        // Set microsteps with proper timing
        driver.microsteps(apiMs);
        delay(10); // Give UART time to process

        // Verify and retry if needed
        for (int iTrial = 0; iTrial < 3 && driver.microsteps() != p.msteps; iTrial++) {
            log_w("Microstep setting mismatch, retrying... (attempt %d)", iTrial + 1);
            driver.microsteps(apiMs);
            delay(10);
        }
        
        // Re-enable motor
        digitalWrite(pinConfig.MOTOR_ENABLE, LOW);
        delay(10);
        
        // Current reference source. The chip's OTP default is "scale by the
        // VREF pin", which silently derates everything rms_current() computes
        // by VREF/2.5 V. Force the internal reference so the commanded current
        // is the current the coils actually see.
        driver.I_scale_analog(!pinConfig.tmc_internal_vref);

        // Chopper mode selection — this is what decides torque vs. silence.
        //   en_spreadcycle = 1 : SpreadCycle everywhere (max torque, loudest)
        //   otherwise          : StealthChop below tpwmthrs_sps, SpreadCycle above.
        // TPWMTHRS lives in the TSTEP domain, so it depends on the microstep
        // resolution and must be recomputed whenever msteps changes — that is
        // why we store the threshold as a velocity and convert here, after
        // the microsteps above have been applied.
        bool spreadAlways = (p.en_spreadcycle != 0);
        uint32_t tpwmthrs = spreadAlways
                                ? 0 // irrelevant once GCONF forces SpreadCycle
                                : tstepFromStepsPerSecond(p.tpwmthrs_sps, p.msteps);
        bool spreadReachable = spreadAlways || (tpwmthrs != 0);

        // CoolStep and SpreadCycle MUST NOT be enabled together on a TMC2209.
        // CoolStep drives the current from the StallGuard4 result, and
        // StallGuard4 only produces a valid SG_RESULT in StealthChop. Leave
        // semin > 0 while SpreadCycle can run and the driver scales the current
        // off meaningless data — in practice down towards IRUN/4, i.e. a large
        // and completely silent torque loss exactly at the speeds where
        // SpreadCycle engages. Force CoolStep off instead.
        uint8_t effSemin = p.semin;
        uint8_t effSemax = p.semax;
        uint32_t effTcoolthrs = p.tcoolthrs;
        if (spreadReachable && effSemin != 0)
        {
            log_w("CoolStep (semin=%u) disabled: it needs StallGuard4, which does not "
                  "work in SpreadCycle — leaving it on would throttle the current",
                  effSemin);
            effSemin = 0;
            effSemax = 0;
        }
        if (effSemin == 0)
        {
            // With CoolStep off there is nothing for TCOOLTHRS to gate, and a
            // wide-open threshold only keeps the StallGuard machinery running.
            effTcoolthrs = 0;
        }

        // Set other parameters. The hold multiplier has to ride along with the
        // current: TMCStepper derives IHOLD from IRUN at the moment
        // rms_current() runs, so setting it afterwards would have no effect.
        float holdMult = (float)p.hold_mult_pct / 100.0f;
        if (holdMult < 0.0f) holdMult = 0.0f;
        if (holdMult > 1.0f) holdMult = 1.0f;
        driver.rms_current(p.rms_current, holdMult);
        driver.SGTHRS(p.sgthrs);
        driver.semin(effSemin);
        driver.semax(effSemax);
        driver.sedn(p.sedn);
        driver.TCOOLTHRS(effTcoolthrs);
        driver.blank_time(p.blank_time);
        driver.toff(p.toff);
        // SpreadCycle chopper hysteresis — only has an effect once SpreadCycle
        // actually runs, which is now the normal case.
        driver.hstrt(p.hstrt);
        driver.hend(p.hend);

        driver.TPWMTHRS(tpwmthrs);
        driver.en_spreadCycle(spreadAlways);

        if (saveToPrefs)
            writeParamsToPreferences(p);

        // The driver silently saturates at CS = 31; with R_SENSE = 0.2 Ohm that
        // caps the achievable current near 1050 mA RMS. Surface it instead of
        // letting a config quietly ask for a current the driver cannot deliver.
        uint16_t actualCurrent = driver.rms_current();
        if (p.rms_current > actualCurrent + (p.rms_current / 20))
            log_w("TMC2209 current clipped: requested %u mA, driver delivers %u mA "
                  "(CS saturated for R_SENSE=%.2f Ohm)",
                  p.rms_current, actualCurrent, (double)R_SENSE);

        log_i("Apply Motor Settings: msteps: %i, msteps_: %i, rms_current: %i, rms_current_: %i, stall_value: %i, sgthrs: %i, semin: %i, semax: %i, sedn: %i, tcoolthrs: %i, blank_time: %i, toff: %i, hstrt: %i, hend: %i, tpwmthrs: %i (@%i steps/s), en_spreadcycle: %i, internal_vref: %i, hold_mult: %i%%",
              p.msteps, driver.microsteps(), p.rms_current, actualCurrent, p.stall_value, p.sgthrs, effSemin, effSemax, p.sedn, (int)effTcoolthrs, p.blank_time, p.toff,
              p.hstrt, p.hend, (int)tpwmthrs, (int)p.tpwmthrs_sps, p.en_spreadcycle,
              pinConfig.tmc_internal_vref ? 1 : 0, p.hold_mult_pct);
        #endif
    }

    // Presence test for keys whose *zero* is a meaningful setting (e.g. semin=0
    // switches CoolStep off). The plain "val > 0" guards below cannot express
    // that, but they still protect the fields where a stray 0 would disable the
    // driver outright (toff) or make it step wrong (msteps, rms_current).
    static bool jsonHasNumber(cJSON *jsonDocument, const char *key)
    {
        cJSON *val = cJSON_GetObjectItemCaseSensitive(jsonDocument, key);
        return (val != NULL) && cJSON_IsNumber(val);
    }

    static void parseTMCDataFromJSON(cJSON *jsonDocument, TMCData &p)
    {
        int val = 0;
        val = cJsonTool::getJsonInt(jsonDocument, "msteps");
        if (val > 0)
            p.msteps = val;
        val = cJsonTool::getJsonInt(jsonDocument, "rms_current");
        if (val > 0)
            p.rms_current = val;
        val = cJsonTool::getJsonInt(jsonDocument, "stall_value");
        if (val > 0)
            p.stall_value = val;
        val = cJsonTool::getJsonInt(jsonDocument, "sgthrs");
        if (val > 0)
            p.sgthrs = val;
        // semin = 0 disables CoolStep, so these have to be presence-checked
        // rather than value-checked or you could never switch CoolStep off.
        if (jsonHasNumber(jsonDocument, "semin"))
            p.semin = cJsonTool::getJsonInt(jsonDocument, "semin");
        if (jsonHasNumber(jsonDocument, "semax"))
            p.semax = cJsonTool::getJsonInt(jsonDocument, "semax");
        if (jsonHasNumber(jsonDocument, "sedn"))
            p.sedn = cJsonTool::getJsonInt(jsonDocument, "sedn");
        val = cJsonTool::getJsonInt(jsonDocument, "tcoolthrs");
        if (val > 0)
            p.tcoolthrs = val;
        val = cJsonTool::getJsonInt(jsonDocument, "blank_time");
        if (val > 0)
            p.blank_time = val;
        val = cJsonTool::getJsonInt(jsonDocument, "toff");
        if (val > 0)
            p.toff = val;
        // Chopper-mode controls — 0 is meaningful for all three
        // (tpwmthrs_sps = 0 -> StealthChop always, en_spreadcycle = 0 -> hybrid).
        if (jsonHasNumber(jsonDocument, "tpwmthrs_sps"))
            p.tpwmthrs_sps = (uint32_t)cJsonTool::getJsonInt(jsonDocument, "tpwmthrs_sps");
        if (jsonHasNumber(jsonDocument, "en_spreadcycle"))
            p.en_spreadcycle = cJsonTool::getJsonInt(jsonDocument, "en_spreadcycle") ? 1 : 0;
        if (jsonHasNumber(jsonDocument, "hstrt"))
            p.hstrt = cJsonTool::getJsonInt(jsonDocument, "hstrt") & 0x07;
        if (jsonHasNumber(jsonDocument, "hend"))
            p.hend = cJsonTool::getJsonInt(jsonDocument, "hend") & 0x0F;
        if (jsonHasNumber(jsonDocument, "hold_mult_pct"))
        {
            int hm = cJsonTool::getJsonInt(jsonDocument, "hold_mult_pct");
            if (hm < 0) hm = 0;
            if (hm > 100) hm = 100;
            p.hold_mult_pct = (uint8_t)hm;
        }
    }

    int act(cJSON *jsonDocument)
    {
        // modify the TMC2209 settings
        // {"task":"/tmc_act", "msteps":16, "rmscurr":400, "stall_value":100, "sgthrs":100, "semin":5, "semax":2, "blank_time":24, "toff":4}
        // {"task":"/tmc_act", "reset": 1}

        // get hold on the axis used - if necessary
        int axis = cJsonTool::getJsonInt(jsonDocument, "axis");

        // extract qid
        int qid = cJsonTool::getJsonInt(jsonDocument, "qid");

        // parse data from json and apply to settings
        TMCData p = readParamsFromPreferences();
        parseTMCDataFromJSON(jsonDocument, p);

        if (pinConfig.tmc_SW_RX == disabled)
        {
            return -1;
        }

        bool tmc_calibrate = (cJsonTool::getJsonInt(jsonDocument, "calibrate") == 1);
        if (tmc_calibrate)
        { // {"task":"/tmc_act", "calibrate": 10000}
            // callibrateStallguard(...)
            log_i("Calibrating TMC2209 Stallguard");
            int speed = cJsonTool::getJsonInt(jsonDocument, "calibrate");
            callibrateStallguard(speed);
            return qid;
        }

        bool tmc_reset = (cJsonTool::getJsonInt(jsonDocument, "reset") == 1);
        if (tmc_reset)
        {
            log_i("Resetting TMC2209 settings to default");
            TMCData defaults = paramsFromPinConfig();
            writeParamsToPreferences(defaults);
            applyParamsToDriver(defaults, false);
            return qid;
        }

        applyParamsToDriver(p, true);
        return qid;

    }

    cJSON *get(cJSON *jsonDocument)
    {
        if (pinConfig.tmc_SW_RX == disabled)
        {
            return jsonDocument;
        }
#if defined(TMC_CONTROLLER) && (!defined(CAN_CONTROLLER_CANOPEN) || (NODE_ROLE == 2))
        TMCData p = readParamsFromPreferences();
        cJSON *monitor_json = cJSON_CreateObject();
        cJSON_AddNumberToObject(monitor_json, "msteps", p.msteps);
        cJSON_AddNumberToObject(monitor_json, "msteps_", driver.microsteps());
        cJSON_AddNumberToObject(monitor_json, "rmscurr", p.rms_current);
        cJSON_AddNumberToObject(monitor_json, "rmscurr_", driver.rms_current());
        cJSON_AddNumberToObject(monitor_json, "stall_value", p.stall_value);
        cJSON_AddNumberToObject(monitor_json, "sgthrs", p.sgthrs);
        cJSON_AddNumberToObject(monitor_json, "semin", p.semin);
        cJSON_AddNumberToObject(monitor_json, "semax", p.semax);
        cJSON_AddNumberToObject(monitor_json, "sedn", p.sedn);
        cJSON_AddNumberToObject(monitor_json, "tcoolthrs", p.tcoolthrs);
        cJSON_AddNumberToObject(monitor_json, "blank_time", p.blank_time);
        cJSON_AddNumberToObject(monitor_json, "toff", p.toff);
        cJSON_AddNumberToObject(monitor_json, "tpwmthrs_sps", p.tpwmthrs_sps);
        cJSON_AddNumberToObject(monitor_json, "tpwmthrs", tstepFromStepsPerSecond(p.tpwmthrs_sps, p.msteps));
        cJSON_AddNumberToObject(monitor_json, "tpwmthrs_", driver.TPWMTHRS());
        cJSON_AddNumberToObject(monitor_json, "en_spreadcycle", p.en_spreadcycle);
        cJSON_AddNumberToObject(monitor_json, "en_spreadcycle_", driver.en_spreadCycle() ? 1 : 0);
        cJSON_AddNumberToObject(monitor_json, "hold_mult_pct", p.hold_mult_pct);
        cJSON_AddNumberToObject(monitor_json, "hstrt", p.hstrt);
        cJSON_AddNumberToObject(monitor_json, "hend", p.hend);
        cJSON_AddNumberToObject(monitor_json, "internal_vref", pinConfig.tmc_internal_vref ? 1 : 0);
        cJSON_AddNumberToObject(monitor_json, "i_scale_analog_", driver.I_scale_analog() ? 1 : 0);
        cJSON_AddNumberToObject(monitor_json, "SG_RESULT", driver.SG_RESULT());
        cJSON_AddNumberToObject(monitor_json, "current", driver.cs2rms(driver.cs_actual()));
        return monitor_json;
#else
        return nullptr;
#endif
    }

    uint16_t getMicrosteps()
    {
        // Reflect the persisted/config value (what was applied at boot) rather
        // than a UART read-back, so this stays cheap and works even if the
        // driver is momentarily unresponsive.
        return readParamsFromPreferences().msteps;
    }

    uint16_t getTMCCurrent()
    {
        if (pinConfig.tmc_SW_RX == disabled)
        {
            log_e("TMC2209 not enabled in this configuration");
            return 0;
        }
#if defined(TMC_CONTROLLER) && (!defined(CAN_CONTROLLER_CANOPEN) || (NODE_ROLE == 2))
        return driver.rms_current();
#else
        return 0;
#endif
    }



    void setTMCCurrent(uint16_t current)
    {
        // This will change the driver's current but will not save it to preferences (e.g. won't survive boot)
        if (pinConfig.tmc_SW_RX == disabled)
        {
            log_e("TMC2209 not enabled in this configuration");
            return;
        }
#if defined(TMC_CONTROLLER) && (!defined(CAN_CONTROLLER_CANOPEN) || (NODE_ROLE == 2))
        // Determine if this board drives the Z axis (needs more current)
        bool isZAxis = (pinConfig.CAN_ID_CURRENT == pinConfig.CAN_ID_MOT_Z);
        if (isZAxis){
            driver.rms_current(current*1.5); // we double the current for the Z axis as it needs more power
        }
        else{
            driver.rms_current(current);
        }
        log_i("TMC2209 Current set to %i", current);
#endif
    }

    void callibrateStallguard(int speed = 10000)
    {
#if defined(TMC_CONTROLLER) && (!defined(CAN_CONTROLLER_CANOPEN) || (NODE_ROLE == 2))
        /*
        We calibrate the Stallguard value from an initial value stall_min in increments of stall_incr until we sense a plausible stallguard value.
        We assume the motor is stopped already (i.e. stalled) and we are in a position where the stallguard value is plausible.
        Call:
        {"task":"/tmc_act", "calibrate": -30000}
        */
        if (pinConfig.tmc_SW_RX == disabled)
        {
            log_e("TMC2209 not enabled in this configuration");
            return;
        }
        // we start moving the motor to get a stallguard value
        int mStepper = Stepper::A; // we assume we are working with motor A
                                   // we may have a dual axis so we would need to start A too
        log_i("Starting A forever");
        getData()[mStepper]->isforever = true;
        getData()[mStepper]->speed = speed;
        getData()[mStepper]->isEnable = 1;
        getData()[mStepper]->isaccelerated = 0;
        FocusMotor::startStepper(mStepper, 0);
        delay(200);

        int START_SGTHRS = 0;
        int MAX_SGTHRS = 255;
        int THRESHOLD_STEP = 5;
        int sgthrs = START_SGTHRS;
        bool obstacleDetected = false;
        // Autotuning loop for sensorless homing
        while (!obstacleDetected && sgthrs <= MAX_SGTHRS)
        {
            esp_task_wdt_reset(); // Reset (feed) the watchdog timer
            // Set the StallGuard threshold
            driver.SGTHRS(sgthrs);
            Serial.print("Testing SGTHRS: ");
            Serial.println(sgthrs);

            // Check for obstacle detection by monitoring the DIAG pin
            if (digitalRead(pinConfig.tmc_pin_diag) == LOW)
            {
                log_i("Obstacle detected at SGTHRS: %i", sgthrs);
                // FocusMotor::stopStepper(mStepper);
                // obstacleDetected = true;
                // break; // Exit the loop as the obstacle is detected
            }

            // print current and stallguard
            log_i("Current: %i, StallGuard: %i", driver.cs2rms(driver.cs_actual()), driver.SG_RESULT());

            // Increment StallGuard threshold if no obstacle is detected
            sgthrs += THRESHOLD_STEP;
            delay(100); // Short delay to allow parameter change to take effect
        }

        if (!obstacleDetected)
        {
            Serial.println("Obstacle not detected within threshold range.");
            // Optionally: Reverse direction or take other actions
        }
        else
        {
            Serial.print("Optimal SGTHRS found: ");
            Serial.println(sgthrs);
        }
        FocusMotor::stopStepper(mStepper);
#endif
    }

    void setup()
    {
        if (pinConfig.tmc_SW_RX == disabled)
        {
            log_e("TMC2209 not enabled in this configuration perhaps you use it via CAN or I2C");
            return;
        }
#if defined(TMC_CONTROLLER) && (!defined(CAN_CONTROLLER_CANOPEN) || (NODE_ROLE == 2))
        log_i("Setting up TMC2209");

        preferences.begin("tmc", false);
        Serial1.begin(115200, SERIAL_8N1, pinConfig.tmc_SW_RX, pinConfig.tmc_SW_TX);
        driver.begin();
        //https://github.com/teemuatlut/TMCStepper/issues/35#issuecomment-498605125
        // Use PDN/UART pin for communication
        driver.pdn_disable(true);
        // Necessary for TMC2208 to set microstep register with UART
        driver.mstep_reg_select(1);
        driver.intpol(true);

        migratePreferencesIfNeeded();

        TMCData p = readParamsFromPreferences();
        applyParamsToDriver(p, false);
        applyParamsToDriver(p, false);
        writeParamsToPreferences(p);
        // Set the stallguard threshold
        pinMode(pinConfig.tmc_pin_diag, INPUT);
       preferences.end();

        log_i("TMC2209 setup done");
#endif
    }

    void loop() {

        if (pinConfig.TMC_DEBUG)
        {
#if defined(TMC_CONTROLLER) && (!defined(CAN_CONTROLLER_CANOPEN) || (NODE_ROLE == 2))
// print stallguard and current in every cycle
            log_i("TMC2209 Debug - Current: %i mA, StallGuard: %i", driver.cs2rms(driver.cs_actual()), driver.SG_RESULT());
            #endif
    };
}

}

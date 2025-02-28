#include "TMCController.h"
#ifdef I2C_MASTER
#include "../i2c/i2c_master.h"
#endif
#ifdef CAN_CONTROLLER
#include "../can/can_controller.h"
#endif

using namespace FocusMotor;

namespace TMCController
{
    // TMC2209 instance
    TMC2209Stepper driver(&Serial1, R_SENSE, DRIVER_ADDRESS);


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
        preferences.end();
        log_i("TMC2209 settings saved to preferences: msteps: %i, current: %i, stall: %i, sgthrs: %i, semin: %i, semax: %i, sedn: %i, tcool: %i, blank: %i, toff: %i",
              p.msteps, p.rms_current, p.stall_value, p.sgthrs, p.semin, p.semax, p.sedn, p.tcoolthrs, p.blank_time, p.toff);
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
        preferences.end();
        return p;
    }

    void applyParamsToDriver(const TMCData &p, bool saveToPrefs)
    {
        #if not defined(CAN_MASTER)
        driver.microsteps(p.msteps);
        driver.rms_current(p.rms_current);
        driver.SGTHRS(p.sgthrs);
        driver.semin(p.semin);
        driver.semax(p.semax);
        driver.sedn(p.sedn);
        driver.TCOOLTHRS(p.tcoolthrs);
        driver.blank_time(p.blank_time);
        driver.toff(p.toff);
        if (saveToPrefs)
            writeParamsToPreferences(p);
        /*
        digitalWrite(pinConfig.MOTOR_ENABLE, HIGH);
        delay(10);
        digitalWrite(pinConfig.MOTOR_ENABLE, LOW);
        */
        log_i("Apply Motor Settings: msteps: %i, msteps_: %i, rms_current: %i, rms_current_: %i, stall_value: %i, sgthrs: %i, semin: %i, semax: %i, sedn: %i, tcoolthrs: %i, blank_time: %i, toff: %i",
              p.msteps, driver.microsteps(), p.rms_current, driver.rms_current(), p.stall_value, p.sgthrs, p.semin, p.semax, p.sedn, p.tcoolthrs, p.blank_time, p.toff);
        #endif
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
        val = cJsonTool::getJsonInt(jsonDocument, "semin");
        if (val > 0)
            p.semin = val;
        val = cJsonTool::getJsonInt(jsonDocument, "semax");
        if (val > 0)
            p.semax = val;
        val = cJsonTool::getJsonInt(jsonDocument, "sedn");
        if (val >= 0)
            p.sedn = val;
        val = cJsonTool::getJsonInt(jsonDocument, "tcoolthrs");
        if (val > 0)
            p.tcoolthrs = val;
        val = cJsonTool::getJsonInt(jsonDocument, "blank_time");
        if (val > 0)
            p.blank_time = val;
        val = cJsonTool::getJsonInt(jsonDocument, "toff");
        if (val > 0)
            p.toff = val;
    }

    int act(cJSON *jsonDocument)
    {
        // modify the TMC2209 settings
        // {"task":"/tmc_act", "msteps":16, "rmscurr":400, "stall_value":100, "sgthrs":100, "semin":5, "semax":2, "blank_time":24, "toff":4}
        // {"task":"/tmc_act", "reset": 1}

        // get hold on the axis used - if necessary
        int axis = cJsonTool::getJsonInt(jsonDocument, "axis");

        // parse data from json and apply to settings
        TMCData p = readParamsFromPreferences();
        parseTMCDataFromJSON(jsonDocument, p);

#ifdef I2C_MASTER
        // send TMC data via I2C
        i2c_master::sendTMCDataI2C(p, axis);
        return 0;
#elif defined(CAN_MASTER)
        can_controller::sendTMCDataToCANDriver(p, axis);
        return 0;
#else
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
            return 0;
        }

        bool tmc_reset = (cJsonTool::getJsonInt(jsonDocument, "reset") == 1);
        if (tmc_reset)
        {
            log_i("Resetting TMC2209 settings to default");
            preferences.begin("tmc", false);
            preferences.putInt("msteps", pinConfig.tmc_microsteps);
            preferences.putInt("current", pinConfig.tmc_rms_current);
            preferences.putInt("stall", pinConfig.tmc_stall_value);
            preferences.putInt("sgthrs", pinConfig.tmc_sgthrs);
            preferences.putInt("semin", pinConfig.tmc_semin);
            preferences.putInt("semax", pinConfig.tmc_semax);
            preferences.putInt("sedn", pinConfig.tmc_sedn);
            preferences.putInt("tcool", pinConfig.tmc_tcoolthrs);
            preferences.putInt("blank", pinConfig.tmc_blank_time);
            preferences.putInt("toff", pinConfig.tmc_toff);
            preferences.end();
            TMCData defaults = readParamsFromPreferences();
            applyParamsToDriver(defaults, false);
            return 0;
        }

        applyParamsToDriver(p, true);
        return 0;
#endif
    }

    cJSON *get(cJSON *jsonDocument)
    {
        if (pinConfig.tmc_SW_RX == disabled)
        {
            return jsonDocument;
        }
#ifdef TMC_CONTROLLER and not defined(CAN_MASTER)
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
        cJSON_AddNumberToObject(monitor_json, "SG_RESULT", driver.SG_RESULT());
        cJSON_AddNumberToObject(monitor_json, "Current", driver.cs2rms(driver.cs_actual()));
        return monitor_json;
#else
        return nullptr;
#endif
    }

    uint16_t getTMCCurrent()
    {
        if (pinConfig.tmc_SW_RX == disabled)
        {
            log_e("TMC2209 not enabled in this configuration");
            return 0;
        }
#ifdef TMC_CONTROLLER and not defined(CAN_MASTER)
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
#ifdef TMC_CONTROLLER and not defined(CAN_MASTER)
        driver.rms_current(current);
        log_i("TMC2209 Current set to %i", current);
#endif
    }

    void callibrateStallguard(int speed = 10000)
    {
#ifdef TMC_CONTROLLER and not defined(CAN_MASTER)
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
        FocusMotor::startStepper(mStepper, false);
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
#ifdef TMC_CONTROLLER and not defined(CAN_MASTER)
        log_i("Setting up TMC2209");

        preferences.begin("tmc", false);
        Serial1.begin(115200, SERIAL_8N1, pinConfig.tmc_SW_RX, pinConfig.tmc_SW_TX);
        driver.begin();
        //https://github.com/teemuatlut/TMCStepper/issues/35#issuecomment-498605125
        // Use PDN/UART pin for communication
        driver.pdn_disable(true);
        // Necessary for TMC2208 to set microstep register with UART
        driver.mstep_reg_select(1);

        TMCData p = readParamsFromPreferences();
        applyParamsToDriver(p, false);
        // Set the stallguard threshold
        pinMode(pinConfig.tmc_pin_diag, INPUT);
       preferences.end();

        log_i("TMC2209 setup done");
#endif
    }

    void loop() {};

}

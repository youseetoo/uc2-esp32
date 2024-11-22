#include "TMCController.h"
#ifdef I2C_MASTER
#include "../i2c/i2c_master.h"
#endif

using namespace FocusMotor;

namespace TMCController
{
    // TMC2209 instance
    TMC2209Stepper driver(&Serial1, R_SENSE, DRIVER_ADDRESS);

    int act(cJSON *jsonDocument)
    {


        #ifdef I2C_MASTER
        TMCData tmcData;

        // convert Json to TMCData
        tmcData.msteps = cJsonTool::getJsonInt(jsonDocument, "msteps");
        tmcData.rms_current = cJsonTool::getJsonInt(jsonDocument, "rms_current");
        tmcData.stall_value = cJsonTool::getJsonInt(jsonDocument, "stall_value");
        tmcData.sgthrs = cJsonTool::getJsonInt(jsonDocument, "sgthrs");
        tmcData.semin = cJsonTool::getJsonInt(jsonDocument, "semin");
        tmcData.semax = cJsonTool::getJsonInt(jsonDocument, "semax");
        int axis = cJsonTool::getJsonInt(jsonDocument, "axis");

        // send TMC data via I2C
        i2c_master::sendTMCDataI2C(tmcData, axis);
        return 0;
        #else
        if (pinConfig.tmc_SW_RX == disabled)
        {
            log_e("TMC2209 not enabled in this configuration");
            return -1;
        }

        // modify the TMC2209 settings
        // {"task":"/tmc_act", "msteps":16, "rms_current":400, "stall_value":100, "sgthrs":100, "semin":5, "semax":2, "blank_time":24, "toff":4}
        // {"task":"/tmc_act", "reset": 1}
        preferences.begin("TMC", false);

        // calibrate stallguard?
        bool tmc_calibrate = cJsonTool::getJsonInt(jsonDocument, "calibrate");
        if (tmc_calibrate == 1)
        {   // {"task":"/tmc_act", "calibrate": 10000}
            int speed = cJsonTool::getJsonInt(jsonDocument, "calibrate");
            callibrateStallguard(speed);
            return 0;
        }

        // reset settings?
        bool tmc_resetsettings = cJsonTool::getJsonInt(jsonDocument, "reset");
        if (tmc_resetsettings == 1)
        {
            // reset all TMC settings to default values
            preferences.begin("TMC", false);
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
            log_i("TMC2209 settings reset to default values");
            // apply default values
            driver.microsteps(pinConfig.tmc_microsteps);
            driver.rms_current(pinConfig.tmc_rms_current);
            driver.SGTHRS(pinConfig.tmc_sgthrs);
            driver.semin(pinConfig.tmc_semin);
            driver.semax(pinConfig.tmc_semax);
            driver.sedn(pinConfig.tmc_sedn);
            driver.TCOOLTHRS(pinConfig.tmc_tcoolthrs);
            driver.blank_time(pinConfig.tmc_blank_time);
            driver.toff(pinConfig.tmc_toff);
            return 0;
        }
        // microsteps
        int tmc_microsteps = cJsonTool::getJsonInt(jsonDocument, "msteps");
        if (tmc_microsteps != driver.microsteps() and tmc_microsteps > 0)
        {
            driver.microsteps(tmc_microsteps);
            preferences.putInt("msteps", tmc_microsteps);
            log_i("TMC2209 microsteps set to %i", tmc_microsteps);
        }

        // RMS current
        int tmc_rms_current = cJsonTool::getJsonInt(jsonDocument, "rms_current");
        if (tmc_rms_current != driver.rms_current() and tmc_rms_current > 0)
        {
            driver.rms_current(tmc_rms_current);
            preferences.putInt("current", tmc_rms_current);
            log_i("TMC2209 RMS current set to %i", tmc_rms_current);
        }

        // StallGuard threshold
        int tmc_sgthrs = cJsonTool::getJsonInt(jsonDocument, "sgthrs");
        if (tmc_sgthrs != driver.SGTHRS() and tmc_sgthrs > 0)
        {
            driver.SGTHRS(tmc_sgthrs);
            preferences.putInt("sgthrs", tmc_sgthrs);
            log_i("TMC2209 StallGuard threshold set to %i", tmc_sgthrs);
        }

        // SeMin
        int tmc_semin = cJsonTool::getJsonInt(jsonDocument, "semin");
        if (tmc_semin != driver.semin() and tmc_semin > 0)
        {
            driver.semin(tmc_semin);
            preferences.putInt("semin", tmc_semin);
            log_i("TMC2209 SeMin set to %i", tmc_semin);
        }

        // SeMax
        int tmc_semax = cJsonTool::getJsonInt(jsonDocument, "semax");
        if (tmc_semax != driver.semax() and tmc_semax > 0)
        {
            driver.semax(tmc_semax);
            preferences.putInt("semax", tmc_semax);
            log_i("TMC2209 SeMax set to %i", tmc_semax);
        }

        // SeDn
        int tmc_sedn = cJsonTool::getJsonInt(jsonDocument, "sedn");
        if (tmc_sedn != driver.sedn() and tmc_sedn > 0)
        {
            driver.sedn(tmc_sedn);
            preferences.putInt("sedn", tmc_sedn);
            log_i("TMC2209 SeDn set to %i", tmc_sedn);
        }

        // TCOOLTHRS
        int tmc_tcoolthrs = cJsonTool::getJsonInt(jsonDocument, "tcoolthrs");
        if (tmc_tcoolthrs != driver.TCOOLTHRS() and tmc_tcoolthrs > 0)
        {
            driver.TCOOLTHRS(tmc_tcoolthrs);
            preferences.putInt("tcool", tmc_tcoolthrs);
            log_i("TMC2209 TCOOLTHRS set to %i", tmc_tcoolthrs);
        }

        // Blank time
        int tmc_blank_time = cJsonTool::getJsonInt(jsonDocument, "blank_time");
        if (tmc_blank_time != driver.blank_time() and tmc_blank_time > 0)
        {
            driver.blank_time(tmc_blank_time);
            preferences.putInt("blankt", tmc_blank_time);
            log_i("TMC2209 Blank time set to %i", tmc_blank_time);
        }

        // TOff
        int tmc_toff = cJsonTool::getJsonInt(jsonDocument, "toff");
        if (tmc_toff != driver.toff() and tmc_toff > 0)
        {
            driver.toff(tmc_toff);
            preferences.putInt("toff", tmc_toff);
            log_i("TMC2209 TOff set to %i", tmc_toff);
        }

        preferences.end();
        Serial.println("TMC Actuated with new parameters.");
        return 0;
        #endif
    }

    cJSON *get(cJSON *jsonDocument)
    {
        if (pinConfig.tmc_SW_RX == disabled)
        {
            log_e("TMC2209 not enabled in this configuration");
            return jsonDocument;
        }
        #ifdef TMC_CONTROLLER
        // print all TMC2209 settings from preferences
        // {"task":"/tmc_get"}
        preferences.begin("TMC", true);
        cJSON *monitor_json = cJSON_CreateObject();
        cJSON_AddItemToObject(monitor_json, "msteps", cJSON_CreateNumber(preferences.getInt("msteps", 16)));
        cJSON_AddItemToObject(monitor_json, "rms_current", cJSON_CreateNumber(preferences.getInt("current", 400)));
        cJSON_AddItemToObject(monitor_json, "stall_value", cJSON_CreateNumber(preferences.getInt("stall", 100)));
        cJSON_AddItemToObject(monitor_json, "sgthrs", cJSON_CreateNumber(preferences.getInt("sgthrs", 100)));
        cJSON_AddItemToObject(monitor_json, "semin", cJSON_CreateNumber(preferences.getInt("semin", 5)));
        cJSON_AddItemToObject(monitor_json, "semax", cJSON_CreateNumber(preferences.getInt("semax", 2)));
        cJSON_AddItemToObject(monitor_json, "sedn", cJSON_CreateNumber(preferences.getInt("sedn", 0b01)));
        cJSON_AddItemToObject(monitor_json, "tcoolthrs", cJSON_CreateNumber(preferences.getInt("tcool", 0xFFFFF)));
        cJSON_AddItemToObject(monitor_json, "blank_time", cJSON_CreateNumber(preferences.getInt("blank", 24)));
        cJSON_AddItemToObject(monitor_json, "toff", cJSON_CreateNumber(preferences.getInt("toff", 4)));
        preferences.end();
        // print driver settings too
        cJSON_AddItemToObject(monitor_json, "SG_RESULT", cJSON_CreateNumber(driver.SG_RESULT())); // Print StallGuard value
        cJSON_AddItemToObject(monitor_json, "Current", cJSON_CreateNumber(driver.cs2rms(driver.cs_actual())));
        return monitor_json;
        #else
        return nullptr;
        #endif
    }

    void setTMCData(TMCData tmcData){
        if (pinConfig.tmc_SW_RX == disabled)
        {
            log_e("TMC2209 not enabled in this configuration");
            return;
        }
        #ifdef TMC_CONTROLLER
        // set TMC2209 settings
        driver.microsteps(tmcData.msteps);
        driver.rms_current(tmcData.rms_current);
        driver.SGTHRS(tmcData.sgthrs);
        driver.semin(tmcData.semin);
        driver.semax(tmcData.semax);
        driver.sedn(tmcData.sedn);
        driver.TCOOLTHRS(tmcData.tcoolthrs);
        driver.blank_time(tmcData.blank_time);
        driver.toff(tmcData.toff);
        log_i("TMC2209 Setup with %i microsteps and %i rms current", tmcData.msteps, tmcData.rms_current);
        #endif    
    }


    void callibrateStallguard(int speed = 10000)
    {
        #ifdef TMC_CONTROLLER
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
                //FocusMotor::stopStepper(mStepper);
                //obstacleDetected = true;
                //break; // Exit the loop as the obstacle is detected
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
            log_e("TMC2209 not enabled in this configuration");
            return;
        }
// TMC2209 Settings
#ifdef TMC_CONTROLLER
        log_i("Setting up TMC2209");
        preferences.begin("TMC", false);
        Serial1.begin(115200, SERIAL_8N1, pinConfig.tmc_SW_RX, pinConfig.tmc_SW_TX);
        int tmc_microsteps = preferences.getInt("msteps", pinConfig.tmc_microsteps);
        int tmc_rms_current = preferences.getInt("current", pinConfig.tmc_rms_current);
        int tmc_stall_value = preferences.getInt("stall", pinConfig.tmc_stall_value);
        int tmc_sgthrs = preferences.getInt("sgthrs", pinConfig.tmc_sgthrs);
        int tmc_semin = preferences.getInt("semin", pinConfig.tmc_semin);
        int tmc_semax = preferences.getInt("semax", pinConfig.tmc_semax);
        int tmc_sedn = preferences.getInt("sedn", pinConfig.tmc_sedn);
        int tmc_tcoolthrs = preferences.getInt("tcool", pinConfig.tmc_tcoolthrs);
        int tmc_blank_time = preferences.getInt("blank", pinConfig.tmc_blank_time);
        int tmc_toff = preferences.getInt("toff", pinConfig.tmc_toff);
        // motor current and stall value
        preferences.end();

        log_i("TMC2209 Setup with %i microsteps and %i rms current", tmc_microsteps, tmc_rms_current);
        driver.begin();
        driver.toff(tmc_toff);
        driver.blank_time(tmc_blank_time);
        driver.rms_current(tmc_rms_current);
        driver.microsteps(tmc_microsteps);
        driver.TCOOLTHRS(tmc_tcoolthrs);
        driver.semin(tmc_semin);
        driver.semax(tmc_semax);
        driver.sedn(tmc_sedn);
        driver.SGTHRS(tmc_sgthrs);
        driver.I_scale_analog(0);
        driver.internal_Rsense(false);

        log_i("TMC2209 Setup done with %i microsteps and %i rms current", tmc_microsteps, tmc_rms_current);

        pinMode(pinConfig.tmc_pin_diag, INPUT);
#endif
    }

    void loop()
    {
        
#ifdef TMC_CONTROLLER
// print sg result, current, and stallguard
//log_i("Current: %i, StallGuard: %i, Diag: %i", driver.cs2rms(driver.cs_actual()), driver.SG_RESULT(), digitalRead(pinConfig.tmc_pin_diag));
#endif 
    }
}

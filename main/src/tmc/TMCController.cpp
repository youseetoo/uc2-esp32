#include "TMCController.h"
namespace TMCController
{
    // TMC2209 instance
    TMC2209Stepper driver(&Serial1, R_SENSE, DRIVER_ADDRESS);

    int act(cJSON *jsonDocument)
    {
        // modify the TMC2209 settings
        // {"task":"/tmc_act", "msteps":16, "rms_current":400, "stall_value":100, "sgthrs":100, "semin":5, "semax":2, "blank_time":24, "toff":4}
        
        preferences.begin("TMC", false);

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
            preferences.putInt("rms_current", tmc_rms_current);
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
            preferences.putInt("tcoolthrs", tmc_tcoolthrs);
            log_i("TMC2209 TCOOLTHRS set to %i", tmc_tcoolthrs);
        }

        // Blank time
        int tmc_blank_time = cJsonTool::getJsonInt(jsonDocument, "blank_time");
        if (tmc_blank_time != driver.blank_time() and tmc_blank_time > 0)
        {
            driver.blank_time(tmc_blank_time);
            preferences.putInt("blank_time", tmc_blank_time);
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
    }

    cJSON *get(cJSON *jsonDocument)
    {
        // print all TMC2209 settings from preferences
        // {"task":"/tmc_get"}
        preferences.begin("TMC", true);
        cJSON *monitor_json = cJSON_CreateObject();
        cJSON_AddItemToObject(monitor_json, "msteps", cJSON_CreateNumber(preferences.getInt("msteps", 16)));
        cJSON_AddItemToObject(monitor_json, "rms_current", cJSON_CreateNumber(preferences.getInt("rms_current", 400)));
        cJSON_AddItemToObject(monitor_json, "stall_value", cJSON_CreateNumber(preferences.getInt("stall_value", 100)));
        cJSON_AddItemToObject(monitor_json, "sgthrs", cJSON_CreateNumber(preferences.getInt("sgthrs", 100)));
        cJSON_AddItemToObject(monitor_json, "semin", cJSON_CreateNumber(preferences.getInt("semin", 5)));
        cJSON_AddItemToObject(monitor_json, "semax", cJSON_CreateNumber(preferences.getInt("semax", 2)));
        cJSON_AddItemToObject(monitor_json, "sedn", cJSON_CreateNumber(preferences.getInt("sedn", 0b01)));
        cJSON_AddItemToObject(monitor_json, "tcoolthrs", cJSON_CreateNumber(preferences.getInt("tcoolthrs", 0xFFFFF)));
        cJSON_AddItemToObject(monitor_json, "blank_time", cJSON_CreateNumber(preferences.getInt("blank_time", 24)));
        cJSON_AddItemToObject(monitor_json, "toff", cJSON_CreateNumber(preferences.getInt("toff", 4)));
        preferences.end();
        // print driver settings too
        cJSTON_AddItemToObject(monitor_json, "SG_RESULT", driver.SG_RESULT());  // Print StallGuard value
        cJSTON_AddItemToObject(monitor_json, "Current", driver.cs2rms(driver.cs_actual());        
        return monitor_json;
    }

    void setup()
    {
// TMC2209 Settings
#define STALL_VALUE 100

        preferences.begin("TMC", false);
        Serial1.begin(115200, SERIAL_8N1, pinConfig.tmc_SW_RX, pinConfig.tmc_SW_TX);
        int tmc_microsteps = preferences.getInt("msteps", 16);
        int tmc_rms_current = preferences.getInt("rms_current", 400);
        int tmc_stall_value = preferences.getInt("stall_value", 100);
        int tmc_sgthrs = preferences.getInt("sgthrs", 100);
        int tmc_semin = preferences.getInt("semin", 5);
        int tmc_semax = preferences.getInt("semax", 2);
        int tmc_sedn = preferences.getInt("sedn", 0b01);
        int tmc_tcoolthrs = preferences.getInt("tcoolthrs", 0xFFFFF);
        int tmc_blank_time = preferences.getInt("blank_time", 24);
        int tmc_toff = preferences.getInt("toff", 4);
        // motor current and stall value
        preferences.end();

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

        pinMode(pinConfig.tmc_pin_diag, INPUT_PULLUP);
    }

    void loop()
    {
        // Diag pin reaction
        if (digitalRead(pinConfig.tmc_pin_diag) == LOW)
        {
            Serial.println("Motor Stopped: Diag Pin Triggered");            
        }

    }
}

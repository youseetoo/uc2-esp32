#include "NVSConfig.h"
#include "RuntimeConfig.h"
#include <Preferences.h>
#include "esp_log.h"
#include "Arduino.h"

static const char *TAG = "NVSConfig";
static const char *NVS_NAMESPACE = "uc2cfg";

// Global singleton
RuntimeConfig runtimeConfig;

// Helper macros to reduce repetition when loading/saving booleans
#define NVS_LOAD_BOOL(prefs, key, field) \
    runtimeConfig.field = prefs.getBool(key, runtimeConfig.field)

#define NVS_SAVE_BOOL(prefs, key, field) \
    prefs.putBool(key, runtimeConfig.field)

namespace NVSConfig
{
    void load()
    {
        Preferences prefs;
        if (!prefs.begin(NVS_NAMESPACE, true)) // read-only
        {
            log_w("NVS namespace '%s' not found, using compile-time defaults", NVS_NAMESPACE);
            return;
        }

        // Module flags
        NVS_LOAD_BOOL(prefs, "motor",      motor_enabled);
        NVS_LOAD_BOOL(prefs, "laser",      laser_enabled);
        NVS_LOAD_BOOL(prefs, "led",        led_enabled);
        NVS_LOAD_BOOL(prefs, "home",       home_motor_enabled);
        NVS_LOAD_BOOL(prefs, "dac",        dac_enabled);
        NVS_LOAD_BOOL(prefs, "ain",        analog_in_enabled);
        NVS_LOAD_BOOL(prefs, "aout",       analog_out_enabled);
        NVS_LOAD_BOOL(prefs, "din",        digital_in_enabled);
        NVS_LOAD_BOOL(prefs, "dout",       digital_out_enabled);
        NVS_LOAD_BOOL(prefs, "linenc",     linear_encoder_enabled);
        NVS_LOAD_BOOL(prefs, "pid",        pid_enabled);
        NVS_LOAD_BOOL(prefs, "scanner",    scanner_enabled);
        NVS_LOAD_BOOL(prefs, "galvo",      galvo_enabled);
        NVS_LOAD_BOOL(prefs, "message",    message_enabled);
        NVS_LOAD_BOOL(prefs, "objective",  objective_enabled);
        NVS_LOAD_BOOL(prefs, "tmc",        tmc_enabled);
        NVS_LOAD_BOOL(prefs, "heat",       heat_enabled);
        NVS_LOAD_BOOL(prefs, "bt",         bluetooth_enabled);
        NVS_LOAD_BOOL(prefs, "wifi",       wifi_enabled);
        NVS_LOAD_BOOL(prefs, "dial",       dial_enabled);

        // CAN flags
        NVS_LOAD_BOOL(prefs, "can",        can_enabled);
        NVS_LOAD_BOOL(prefs, "can_mst",    can_master);
        NVS_LOAD_BOOL(prefs, "can_smot",   can_slave_motor);
        NVS_LOAD_BOOL(prefs, "can_slas",   can_slave_laser);
        NVS_LOAD_BOOL(prefs, "can_sled",   can_slave_led);
        NVS_LOAD_BOOL(prefs, "can_sgal",   can_slave_galvo);
        NVS_LOAD_BOOL(prefs, "can_hyb",    can_hybrid);

        runtimeConfig.can_node_id = prefs.getUChar("can_nid", runtimeConfig.can_node_id);
        runtimeConfig.can_bitrate = prefs.getULong("can_baud", runtimeConfig.can_bitrate);

        // Serial protocol
        runtimeConfig.serial_protocol = prefs.getUChar("serprot", runtimeConfig.serial_protocol);

        prefs.end();
        log_i("RuntimeConfig loaded from NVS");
    }

    void save()
    {
        Preferences prefs;
        if (!prefs.begin(NVS_NAMESPACE, false)) // read-write
        {
            log_e("Failed to open NVS namespace '%s' for writing", NVS_NAMESPACE);
            return;
        }

        // Module flags
        NVS_SAVE_BOOL(prefs, "motor",      motor_enabled);
        NVS_SAVE_BOOL(prefs, "laser",      laser_enabled);
        NVS_SAVE_BOOL(prefs, "led",        led_enabled);
        NVS_SAVE_BOOL(prefs, "home",       home_motor_enabled);
        NVS_SAVE_BOOL(prefs, "dac",        dac_enabled);
        NVS_SAVE_BOOL(prefs, "ain",        analog_in_enabled);
        NVS_SAVE_BOOL(prefs, "aout",       analog_out_enabled);
        NVS_SAVE_BOOL(prefs, "din",        digital_in_enabled);
        NVS_SAVE_BOOL(prefs, "dout",       digital_out_enabled);
        NVS_SAVE_BOOL(prefs, "linenc",     linear_encoder_enabled);
        NVS_SAVE_BOOL(prefs, "pid",        pid_enabled);
        NVS_SAVE_BOOL(prefs, "scanner",    scanner_enabled);
        NVS_SAVE_BOOL(prefs, "galvo",      galvo_enabled);
        NVS_SAVE_BOOL(prefs, "message",    message_enabled);
        NVS_SAVE_BOOL(prefs, "objective",  objective_enabled);
        NVS_SAVE_BOOL(prefs, "tmc",        tmc_enabled);
        NVS_SAVE_BOOL(prefs, "heat",       heat_enabled);
        NVS_SAVE_BOOL(prefs, "bt",         bluetooth_enabled);
        NVS_SAVE_BOOL(prefs, "wifi",       wifi_enabled);
        NVS_SAVE_BOOL(prefs, "dial",       dial_enabled);

        // CAN flags
        NVS_SAVE_BOOL(prefs, "can",        can_enabled);
        NVS_SAVE_BOOL(prefs, "can_mst",    can_master);
        NVS_SAVE_BOOL(prefs, "can_smot",   can_slave_motor);
        NVS_SAVE_BOOL(prefs, "can_slas",   can_slave_laser);
        NVS_SAVE_BOOL(prefs, "can_sled",   can_slave_led);
        NVS_SAVE_BOOL(prefs, "can_sgal",   can_slave_galvo);
        NVS_SAVE_BOOL(prefs, "can_hyb",    can_hybrid);

        prefs.putUChar("can_nid", runtimeConfig.can_node_id);
        prefs.putULong("can_baud", runtimeConfig.can_bitrate);

        // Serial protocol
        prefs.putUChar("serprot", runtimeConfig.serial_protocol);

        prefs.end();
        log_i("RuntimeConfig saved to NVS");
    }

    void reset()
    {
        Preferences prefs;
        prefs.begin(NVS_NAMESPACE, false);
        prefs.clear();
        prefs.end();
        log_i("NVS namespace '%s' erased, rebooting with defaults", NVS_NAMESPACE);
        ESP.restart();
    }

    cJSON *toJSON()
    {
        cJSON *root = cJSON_CreateObject();
        if (root == NULL)
            return NULL;

        // Module flags
        cJSON_AddBoolToObject(root, "motor",      runtimeConfig.motor_enabled);
        cJSON_AddBoolToObject(root, "laser",      runtimeConfig.laser_enabled);
        cJSON_AddBoolToObject(root, "led",        runtimeConfig.led_enabled);
        cJSON_AddBoolToObject(root, "home",       runtimeConfig.home_motor_enabled);
        cJSON_AddBoolToObject(root, "dac",        runtimeConfig.dac_enabled);
        cJSON_AddBoolToObject(root, "analog_in",  runtimeConfig.analog_in_enabled);
        cJSON_AddBoolToObject(root, "analog_out", runtimeConfig.analog_out_enabled);
        cJSON_AddBoolToObject(root, "digital_in", runtimeConfig.digital_in_enabled);
        cJSON_AddBoolToObject(root, "digital_out",runtimeConfig.digital_out_enabled);
        cJSON_AddBoolToObject(root, "linear_encoder", runtimeConfig.linear_encoder_enabled);
        cJSON_AddBoolToObject(root, "pid",        runtimeConfig.pid_enabled);
        cJSON_AddBoolToObject(root, "scanner",    runtimeConfig.scanner_enabled);
        cJSON_AddBoolToObject(root, "galvo",      runtimeConfig.galvo_enabled);
        cJSON_AddBoolToObject(root, "message",    runtimeConfig.message_enabled);
        cJSON_AddBoolToObject(root, "objective",  runtimeConfig.objective_enabled);
        cJSON_AddBoolToObject(root, "tmc",        runtimeConfig.tmc_enabled);
        cJSON_AddBoolToObject(root, "heat",       runtimeConfig.heat_enabled);
        cJSON_AddBoolToObject(root, "bluetooth",  runtimeConfig.bluetooth_enabled);
        cJSON_AddBoolToObject(root, "wifi",       runtimeConfig.wifi_enabled);
        cJSON_AddBoolToObject(root, "dial",       runtimeConfig.dial_enabled);

        // CAN
        cJSON_AddBoolToObject(root, "can",         runtimeConfig.can_enabled);
        cJSON_AddBoolToObject(root, "can_master",  runtimeConfig.can_master);
        cJSON_AddBoolToObject(root, "can_slave_motor", runtimeConfig.can_slave_motor);
        cJSON_AddBoolToObject(root, "can_slave_laser", runtimeConfig.can_slave_laser);
        cJSON_AddBoolToObject(root, "can_slave_led",   runtimeConfig.can_slave_led);
        cJSON_AddBoolToObject(root, "can_slave_galvo", runtimeConfig.can_slave_galvo);
        cJSON_AddBoolToObject(root, "can_hybrid",  runtimeConfig.can_hybrid);
        cJSON_AddNumberToObject(root, "can_node_id", runtimeConfig.can_node_id);
        cJSON_AddNumberToObject(root, "can_bitrate", runtimeConfig.can_bitrate);

        // Serial
        cJSON_AddNumberToObject(root, "serial_protocol", runtimeConfig.serial_protocol);

        return root;
    }

    // Helper to read a bool from JSON if present
    static bool readBool(cJSON *doc, const char *key, bool &out)
    {
        cJSON *item = cJSON_GetObjectItemCaseSensitive(doc, key);
        if (item != NULL && cJSON_IsBool(item))
        {
            out = cJSON_IsTrue(item);
            return true;
        }
        return false;
    }

    bool fromJSON(cJSON *doc)
    {
        if (doc == NULL)
            return false;

        bool changed = false;

        // Module flags — only update fields present in the JSON
        changed |= readBool(doc, "motor",       runtimeConfig.motor_enabled);
        changed |= readBool(doc, "laser",       runtimeConfig.laser_enabled);
        changed |= readBool(doc, "led",         runtimeConfig.led_enabled);
        changed |= readBool(doc, "home",        runtimeConfig.home_motor_enabled);
        changed |= readBool(doc, "dac",         runtimeConfig.dac_enabled);
        changed |= readBool(doc, "analog_in",   runtimeConfig.analog_in_enabled);
        changed |= readBool(doc, "analog_out",  runtimeConfig.analog_out_enabled);
        changed |= readBool(doc, "digital_in",  runtimeConfig.digital_in_enabled);
        changed |= readBool(doc, "digital_out", runtimeConfig.digital_out_enabled);
        changed |= readBool(doc, "linear_encoder", runtimeConfig.linear_encoder_enabled);
        changed |= readBool(doc, "pid",         runtimeConfig.pid_enabled);
        changed |= readBool(doc, "scanner",     runtimeConfig.scanner_enabled);
        changed |= readBool(doc, "galvo",       runtimeConfig.galvo_enabled);
        changed |= readBool(doc, "message",     runtimeConfig.message_enabled);
        changed |= readBool(doc, "objective",   runtimeConfig.objective_enabled);
        changed |= readBool(doc, "tmc",         runtimeConfig.tmc_enabled);
        changed |= readBool(doc, "heat",        runtimeConfig.heat_enabled);
        changed |= readBool(doc, "bluetooth",   runtimeConfig.bluetooth_enabled);
        changed |= readBool(doc, "wifi",        runtimeConfig.wifi_enabled);
        changed |= readBool(doc, "dial",        runtimeConfig.dial_enabled);

        // CAN
        changed |= readBool(doc, "can",            runtimeConfig.can_enabled);
        changed |= readBool(doc, "can_master",     runtimeConfig.can_master);
        changed |= readBool(doc, "can_slave_motor", runtimeConfig.can_slave_motor);
        changed |= readBool(doc, "can_slave_laser", runtimeConfig.can_slave_laser);
        changed |= readBool(doc, "can_slave_led",   runtimeConfig.can_slave_led);
        changed |= readBool(doc, "can_slave_galvo", runtimeConfig.can_slave_galvo);
        changed |= readBool(doc, "can_hybrid",     runtimeConfig.can_hybrid);

        cJSON *item = cJSON_GetObjectItemCaseSensitive(doc, "can_node_id");
        if (item != NULL && cJSON_IsNumber(item))
        {
            runtimeConfig.can_node_id = (uint8_t)item->valueint;
            changed = true;
        }

        item = cJSON_GetObjectItemCaseSensitive(doc, "can_bitrate");
        if (item != NULL && cJSON_IsNumber(item))
        {
            runtimeConfig.can_bitrate = (uint32_t)item->valuedouble;
            changed = true;
        }

        item = cJSON_GetObjectItemCaseSensitive(doc, "serial_protocol");
        if (item != NULL && cJSON_IsNumber(item))
        {
            runtimeConfig.serial_protocol = (uint8_t)item->valueint;
            changed = true;
        }

        return changed;
    }
}

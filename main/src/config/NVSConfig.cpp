#include "NVSConfig.h"
#include "RuntimeConfig.h"
#include <Preferences.h>
#include "cJSON.h"
#include "esp_log.h"

static const char* TAG = "NVSConfig";
static const char* NVS_NAMESPACE = "uc2cfg";

// Global RuntimeConfig instance
RuntimeConfig runtimeConfig;

// ── Compile-time defaults derived from build flags ──────────────────────────

#ifdef MOTOR_CONTROLLER
  #define DEFAULT_MOTOR true
#else
  #define DEFAULT_MOTOR false
#endif

#ifdef LASER_CONTROLLER
  #define DEFAULT_LASER true
#else
  #define DEFAULT_LASER false
#endif

#ifdef LED_CONTROLLER
  #define DEFAULT_LED true
#else
  #define DEFAULT_LED false
#endif

#ifdef HOME_MOTOR
  #define DEFAULT_HOME true
#else
  #define DEFAULT_HOME false
#endif

#ifdef DIGITAL_IN_CONTROLLER
  #define DEFAULT_DIGITALIN true
#else
  #define DEFAULT_DIGITALIN false
#endif

#ifdef DIGITAL_OUT_CONTROLLER
  #define DEFAULT_DIGITALOUT true
#else
  #define DEFAULT_DIGITALOUT false
#endif

#ifdef ANALOG_IN_CONTROLLER
  #define DEFAULT_ANALOGIN true
#else
  #define DEFAULT_ANALOGIN false
#endif

#ifdef ANALOG_OUT_CONTROLLER
  #define DEFAULT_ANALOGOUT true
#else
  #define DEFAULT_ANALOGOUT false
#endif

#ifdef DAC_CONTROLLER
  #define DEFAULT_DAC true
#else
  #define DEFAULT_DAC false
#endif

#ifdef TMC_CONTROLLER
  #define DEFAULT_TMC true
#else
  #define DEFAULT_TMC false
#endif

#ifdef LINEAR_ENCODER_CONTROLLER
  #define DEFAULT_ENCODER true
#else
  #define DEFAULT_ENCODER false
#endif

#ifdef ANALOG_JOYSTICK
  #define DEFAULT_JOYSTICK true
#else
  #define DEFAULT_JOYSTICK false
#endif

#ifdef GALVO_CONTROLLER
  #define DEFAULT_GALVO true
#else
  #define DEFAULT_GALVO false
#endif

#ifdef BLUETOOTH
  #define DEFAULT_BLUETOOTH true
#else
  #define DEFAULT_BLUETOOTH false
#endif

#ifdef WIFI
  #define DEFAULT_WIFI true
#else
  #define DEFAULT_WIFI false
#endif

#ifdef PID_CONTROLLER
  #define DEFAULT_PID true
#else
  #define DEFAULT_PID false
#endif

#ifdef SCANNER_CONTROLLER
  #define DEFAULT_SCANNER true
#else
  #define DEFAULT_SCANNER false
#endif

#ifdef MESSAGE_CONTROLLER
  #define DEFAULT_MESSAGE true
#else
  #define DEFAULT_MESSAGE false
#endif

#ifdef DIAL_CONTROLLER
  #define DEFAULT_DIAL true
#else
  #define DEFAULT_DIAL false
#endif

#ifdef OBJECTIVE_CONTROLLER
  #define DEFAULT_OBJECTIVE true
#else
  #define DEFAULT_OBJECTIVE false
#endif

#ifdef HEAT_CONTROLLER
  #define DEFAULT_HEAT true
#else
  #define DEFAULT_HEAT false
#endif

// CAN role default
#if defined(CAN_SEND_COMMANDS)
  #define DEFAULT_CAN_ROLE ((uint8_t)NodeRole::CAN_MASTER)
#elif defined(CAN_RECEIVE_MOTOR) || defined(CAN_RECEIVE_LASER) || defined(CAN_RECEIVE_LED) || defined(CAN_RECEIVE_GALVO) || defined(CAN_RECEIVE_DIAL)
  #define DEFAULT_CAN_ROLE ((uint8_t)NodeRole::CAN_SLAVE)
#elif defined(CAN_BUS_ENABLED)
  #define DEFAULT_CAN_ROLE ((uint8_t)NodeRole::CAN_MASTER)
#else
  #define DEFAULT_CAN_ROLE ((uint8_t)NodeRole::STANDALONE)
#endif

// ── Implementation ──────────────────────────────────────────────────────────

void NVSConfig::loadConfig()
{
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true); // read-only

    runtimeConfig.motor      = prefs.getBool("motor",      DEFAULT_MOTOR);
    runtimeConfig.laser      = prefs.getBool("laser",      DEFAULT_LASER);
    runtimeConfig.led        = prefs.getBool("led",        DEFAULT_LED);
    runtimeConfig.home       = prefs.getBool("home",       DEFAULT_HOME);
    runtimeConfig.digitalIn  = prefs.getBool("digitalIn",  DEFAULT_DIGITALIN);
    runtimeConfig.digitalOut = prefs.getBool("digitalOut",  DEFAULT_DIGITALOUT);
    runtimeConfig.analogIn   = prefs.getBool("analogIn",   DEFAULT_ANALOGIN);
    runtimeConfig.analogOut  = prefs.getBool("analogOut",  DEFAULT_ANALOGOUT);
    runtimeConfig.dac        = prefs.getBool("dac",        DEFAULT_DAC);
    runtimeConfig.tmc        = prefs.getBool("tmc",        DEFAULT_TMC);
    runtimeConfig.encoder    = prefs.getBool("encoder",    DEFAULT_ENCODER);
    runtimeConfig.joystick   = prefs.getBool("joystick",   DEFAULT_JOYSTICK);
    runtimeConfig.galvo      = prefs.getBool("galvo",      DEFAULT_GALVO);
    runtimeConfig.bluetooth  = prefs.getBool("bluetooth",  DEFAULT_BLUETOOTH);
    runtimeConfig.wifi       = prefs.getBool("wifi",       DEFAULT_WIFI);
    runtimeConfig.pid        = prefs.getBool("pid",        DEFAULT_PID);
    runtimeConfig.scanner    = prefs.getBool("scanner",    DEFAULT_SCANNER);
    runtimeConfig.message    = prefs.getBool("message",    DEFAULT_MESSAGE);
    runtimeConfig.dial       = prefs.getBool("dial",       DEFAULT_DIAL);
    runtimeConfig.objective  = prefs.getBool("objective",  DEFAULT_OBJECTIVE);
    runtimeConfig.heat       = prefs.getBool("heat",       DEFAULT_HEAT);

    runtimeConfig.canRole      = (NodeRole)prefs.getUChar("canRole",   DEFAULT_CAN_ROLE);
    runtimeConfig.canNodeId    = prefs.getUChar("canNodeId",  10);
    runtimeConfig.canMotorAxis = prefs.getUChar("canMotAxis", 1);

    prefs.end();

    ESP_LOGI(TAG, "Loaded: motor=%d laser=%d led=%d home=%d bt=%d wifi=%d canRole=%d",
             runtimeConfig.motor, runtimeConfig.laser, runtimeConfig.led,
             runtimeConfig.home, runtimeConfig.bluetooth, runtimeConfig.wifi,
             (int)runtimeConfig.canRole);
}

void NVSConfig::saveConfig()
{
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false); // read-write

    prefs.putBool("motor",      runtimeConfig.motor);
    prefs.putBool("laser",      runtimeConfig.laser);
    prefs.putBool("led",        runtimeConfig.led);
    prefs.putBool("home",       runtimeConfig.home);
    prefs.putBool("digitalIn",  runtimeConfig.digitalIn);
    prefs.putBool("digitalOut", runtimeConfig.digitalOut);
    prefs.putBool("analogIn",   runtimeConfig.analogIn);
    prefs.putBool("analogOut",  runtimeConfig.analogOut);
    prefs.putBool("dac",        runtimeConfig.dac);
    prefs.putBool("tmc",        runtimeConfig.tmc);
    prefs.putBool("encoder",    runtimeConfig.encoder);
    prefs.putBool("joystick",   runtimeConfig.joystick);
    prefs.putBool("galvo",      runtimeConfig.galvo);
    prefs.putBool("bluetooth",  runtimeConfig.bluetooth);
    prefs.putBool("wifi",       runtimeConfig.wifi);
    prefs.putBool("pid",        runtimeConfig.pid);
    prefs.putBool("scanner",    runtimeConfig.scanner);
    prefs.putBool("message",    runtimeConfig.message);
    prefs.putBool("dial",       runtimeConfig.dial);
    prefs.putBool("objective",  runtimeConfig.objective);
    prefs.putBool("heat",       runtimeConfig.heat);

    prefs.putUChar("canRole",   (uint8_t)runtimeConfig.canRole);
    prefs.putUChar("canNodeId",  runtimeConfig.canNodeId);
    prefs.putUChar("canMotAxis", runtimeConfig.canMotorAxis);

    prefs.end();
    ESP_LOGI(TAG, "Config saved to NVS");
}

void NVSConfig::resetConfig()
{
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    ESP_LOGI(TAG, "NVS namespace cleared, reloading compile-time defaults");
    loadConfig();
}

cJSON* NVSConfig::toJson()
{
    cJSON* json = cJSON_CreateObject();
    if (!json) return nullptr;

    cJSON_AddBoolToObject(json, "motor",      runtimeConfig.motor);
    cJSON_AddBoolToObject(json, "laser",      runtimeConfig.laser);
    cJSON_AddBoolToObject(json, "led",        runtimeConfig.led);
    cJSON_AddBoolToObject(json, "home",       runtimeConfig.home);
    cJSON_AddBoolToObject(json, "digitalIn",  runtimeConfig.digitalIn);
    cJSON_AddBoolToObject(json, "digitalOut", runtimeConfig.digitalOut);
    cJSON_AddBoolToObject(json, "analogIn",   runtimeConfig.analogIn);
    cJSON_AddBoolToObject(json, "analogOut",  runtimeConfig.analogOut);
    cJSON_AddBoolToObject(json, "dac",        runtimeConfig.dac);
    cJSON_AddBoolToObject(json, "tmc",        runtimeConfig.tmc);
    cJSON_AddBoolToObject(json, "encoder",    runtimeConfig.encoder);
    cJSON_AddBoolToObject(json, "joystick",   runtimeConfig.joystick);
    cJSON_AddBoolToObject(json, "galvo",      runtimeConfig.galvo);
    cJSON_AddBoolToObject(json, "bluetooth",  runtimeConfig.bluetooth);
    cJSON_AddBoolToObject(json, "wifi",       runtimeConfig.wifi);
    cJSON_AddBoolToObject(json, "pid",        runtimeConfig.pid);
    cJSON_AddBoolToObject(json, "scanner",    runtimeConfig.scanner);
    cJSON_AddBoolToObject(json, "message",    runtimeConfig.message);
    cJSON_AddBoolToObject(json, "dial",       runtimeConfig.dial);
    cJSON_AddBoolToObject(json, "objective",  runtimeConfig.objective);
    cJSON_AddBoolToObject(json, "heat",       runtimeConfig.heat);

    cJSON_AddNumberToObject(json, "canRole",      (int)runtimeConfig.canRole);
    cJSON_AddNumberToObject(json, "canNodeId",    runtimeConfig.canNodeId);
    cJSON_AddNumberToObject(json, "canMotorAxis", runtimeConfig.canMotorAxis);

    return json;
}

void NVSConfig::fromJson(cJSON* json)
{
    if (!json) return;

    cJSON* item;

    item = cJSON_GetObjectItemCaseSensitive(json, "motor");
    if (item && cJSON_IsBool(item)) runtimeConfig.motor = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "laser");
    if (item && cJSON_IsBool(item)) runtimeConfig.laser = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "led");
    if (item && cJSON_IsBool(item)) runtimeConfig.led = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "home");
    if (item && cJSON_IsBool(item)) runtimeConfig.home = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "digitalIn");
    if (item && cJSON_IsBool(item)) runtimeConfig.digitalIn = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "digitalOut");
    if (item && cJSON_IsBool(item)) runtimeConfig.digitalOut = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "analogIn");
    if (item && cJSON_IsBool(item)) runtimeConfig.analogIn = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "analogOut");
    if (item && cJSON_IsBool(item)) runtimeConfig.analogOut = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "dac");
    if (item && cJSON_IsBool(item)) runtimeConfig.dac = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "tmc");
    if (item && cJSON_IsBool(item)) runtimeConfig.tmc = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "encoder");
    if (item && cJSON_IsBool(item)) runtimeConfig.encoder = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "joystick");
    if (item && cJSON_IsBool(item)) runtimeConfig.joystick = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "galvo");
    if (item && cJSON_IsBool(item)) runtimeConfig.galvo = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "bluetooth");
    if (item && cJSON_IsBool(item)) runtimeConfig.bluetooth = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "wifi");
    if (item && cJSON_IsBool(item)) runtimeConfig.wifi = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "pid");
    if (item && cJSON_IsBool(item)) runtimeConfig.pid = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "scanner");
    if (item && cJSON_IsBool(item)) runtimeConfig.scanner = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "message");
    if (item && cJSON_IsBool(item)) runtimeConfig.message = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "dial");
    if (item && cJSON_IsBool(item)) runtimeConfig.dial = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "objective");
    if (item && cJSON_IsBool(item)) runtimeConfig.objective = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "heat");
    if (item && cJSON_IsBool(item)) runtimeConfig.heat = cJSON_IsTrue(item);

    item = cJSON_GetObjectItemCaseSensitive(json, "canRole");
    if (item && cJSON_IsNumber(item)) {
        int val = item->valueint;
        if (val >= 0 && val <= 2) runtimeConfig.canRole = (NodeRole)val;
    }

    item = cJSON_GetObjectItemCaseSensitive(json, "canNodeId");
    if (item && cJSON_IsNumber(item)) {
        int val = item->valueint;
        if (val >= 1 && val <= 127) runtimeConfig.canNodeId = (uint8_t)val;
    }

    item = cJSON_GetObjectItemCaseSensitive(json, "canMotorAxis");
    if (item && cJSON_IsNumber(item)) {
        int val = item->valueint;
        if (val >= 0 && val <= 3) runtimeConfig.canMotorAxis = (uint8_t)val;
    }
}

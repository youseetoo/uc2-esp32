// T4.1: State — pure-C system state (was a C++ namespace in State.cpp).
//
// C feature gating: raw-IDF build -D feature flags are CXX-only, so this C
// TU gates on the CONFIG_UC2_* Kconfig symbols (sdkconfig.h) — the same
// mapping the CMakeLists use to select sources.
//
// The DigitalInController E-stop dependency is a plain C API (gated on
// CONFIG_UC2_DIGITAL_IN_CONTROLLER); weak no-op defaults below cover builds
// where uc2_io's digitalin is absent.
//
// Dropped vs the C++ original (all dead in every current app — none of the
// legacy -D flags I2C_MASTER / ARDUINO / CAN_CONTROLLER / CAN_CONTROLLER_CANOPEN
// / GIT_COMMIT_HASH is defined for CXX either):
//   * #ifdef I2C_MASTER i2c_master::reboot() before esp_restart()
//   * #elif ARDUINO WiFi-AP OTA body in startOTA (log-warn + flag path kept)
//   * #ifdef CAN_CONTROLLER block in get() (identical fields to the
//     CONFIG_UC2_CAN_CONTROLLER_CANOPEN block below, which is kept)
//   * #ifdef GIT_COMMIT_HASH "git_commit" field (no build defines it)
#include "sdkconfig.h"
#include "State.h"
#include "PinConfigDefault.h"
#include "RuntimeConfig.h"
#include "cJsonTool.h"
#include "uc2_hal/uc2_log.h"
#include "uc2_hal/uc2_gpio.h"
#include "uc2_hal/uc2_time.h"
#include "esp_system.h"    // esp_restart
#include "esp_heap_caps.h" // esp_get_free_heap_size
#include "nvs.h"           // resetPrefs
#if CONFIG_UC2_DIGITAL_IN_CONTROLLER
#include "DigitalInController.h"
#endif

// Weak marker for GCC/Clang (ESP-IDF xtensa). MSVC (host test build) has no
// function-weak, and its link always includes the strong digitalin_*
// definitions (DigitalInController.c), so the weak-default block below is
// excluded under _MSC_VER instead (plain strong definitions would collide,
// LNK2005; __declspec(selectany) is data-only, C2496).
#define UC2_WEAK __attribute__((weak))

// ── Weak defaults for the digitalin E-stop API ──────────────────────────────
// The real implementation lives in uc2_io (DigitalInController.c); uc2_core
// cannot depend on uc2_io (circular), so State.c links against weak no-op
// defaults. When uc2_io's digitalin is in the build
// (CONFIG_UC2_DIGITAL_IN_CONTROLLER), the linker resolves to the real
// implementation (see the QidRegistry/focusmotor precedent).
#if CONFIG_UC2_DIGITAL_IN_CONTROLLER && !defined(_MSC_VER)
UC2_WEAK void digitalin_set_emergency_polarity(int8_t pol) { (void)pol; }
UC2_WEAK int8_t digitalin_get_emergency_polarity(void) { return 0; }
UC2_WEAK int digitalin_get_emergency_raw(void) { return -1; }
UC2_WEAK bool digitalin_is_emergency_active(void) { return false; }
#endif

static const char *TAG = "State";

bool state_isdebug = false;
const char *state_identifier_name = "UC2_Feather";
const char *state_identifier_id = "V2.0";
const char *state_identifier_date = __DATE__ " " __TIME__;
const char *state_identifier_author = "BD";
const char *state_identifier_product = "uc2-esp";
bool state_config_set = false;
unsigned long state_start_millis;
unsigned long state_current_millis;
bool state_is_busy = false; // TODO this is not working!!! (now actually shared)
bool state_otarunning = false;

// Tracks the logical CAN-bus power state (true = ON). Mirrors the
// BUSPOWER_OFF_PIN gate so /state_get can report it without reading the pin.
static bool busPowerOn = true;

void state_set_bus_power(bool on)
{
    busPowerOn = on;
    if (pinConfig.BUSPOWER_OFF_PIN >= 0)
    {
        pinMode((uint8_t)pinConfig.BUSPOWER_OFF_PIN, OUTPUT);
        // Active-low-for-on gate: LOW enables bus power, HIGH cuts it.
        digitalWrite((uint8_t)pinConfig.BUSPOWER_OFF_PIN, on ? LOW : HIGH);
    }
    log_i("CAN-bus power %s", on ? "ON" : "OFF");
}

bool state_get_bus_power(void)
{
    return busPowerOn;
}

void state_setup(void)
{
    log_d("Setup State");

    // CAN-bus power defaults to ON at boot (drives BUSPOWER_OFF_PIN LOW).
    // (The buzzer pin is owned by BuzzerController, which sets it up later.)
    if (pinConfig.BUSPOWER_OFF_PIN >= 0)
        state_set_bus_power(true);
}

// {"task":"/state_act", "restart":1}
// {"task":"/state_act", "delay":1000}
// {"task":"/state_act", "isBusy":1}
// {"task":"/state_act", "resetPreferences":1}
// {"task":"/state_act", "isDEBUG":0} // 0-5
// {"task": "/state_act", "buzzer": 1}
// {"task": "/state_act", "power": 1}  // CAN-bus power: 1=ON (default), 0=OFF
// {"task": "/state_act", "power": 0}  // start OTA update mode

// Custom function accessible by the API
int state_act(cJSON *doc)
{
    // here you can do something
    if (state_isdebug)
        log_i("state_act_fct");

    // add buzzer control here:
    cJSON *buzzer = cJSON_GetObjectItemCaseSensitive(doc, "buzzer");
    if (buzzer != NULL)
    {
        int buzzerVal = buzzer->valueint;
        if (pinConfig.BUZZER_PIN >= 0)
        {
            if (buzzerVal > 0)
            {
                digitalWrite((uint8_t)pinConfig.BUZZER_PIN, HIGH);
            }
            else
            {
                digitalWrite((uint8_t)pinConfig.BUZZER_PIN, LOW);
            }
        }
    }
    // CAN-bus power control: {"task":"/state_act","power":0|1}
    cJSON *power = cJSON_GetObjectItemCaseSensitive(doc, "power");
    if (power != NULL)
    {
        state_set_bus_power(power->valueint > 0);
    }

#if CONFIG_UC2_DIGITAL_IN_CONTROLLER
    // Emergency-STOP polarity: {"task":"/state_act","estopPolarity":0|1}
    // 0 = asserted-LOW (idle HIGH), 1 = asserted-HIGH (idle LOW). Live + persisted.
    cJSON *estopPol = cJSON_GetObjectItemCaseSensitive(doc, "estopPolarity");
    if (estopPol != NULL)
    {
        digitalin_set_emergency_polarity((int8_t)(estopPol->valueint ? 1 : 0));
    }
#endif

    cJSON *restart = cJSON_GetObjectItemCaseSensitive(doc, "restart");
    // assign default values to thhe variables
    if (restart != NULL)
    { // {"task":"/state_act", "restart":1, "busrestart":1}
        // Cut CAN-bus power before rebooting so all slaves power-cycle with
        // the master; state_setup() restores power (pin LOW) after reboot.
        if (cJSON_GetObjectItemCaseSensitive(doc, "busrestart") != NULL && pinConfig.BUSPOWER_OFF_PIN >= 0)
        {
            log_i("Cutting CAN-bus power before restart");
            state_set_bus_power(false);
            delay(200);
        }

        // {"task": "/state_act", "restart": 1}
        esp_restart();
    }

    cJSON *ota = cJSON_GetObjectItemCaseSensitive(doc, "ota");
    if (ota != NULL)
    {
        // {"task": "/state_act", "ota": 1}
        state_start_ota();
    }
    // assign default values to thhe variables
    cJSON *del = cJSON_GetObjectItemCaseSensitive(doc, "delay");
    if (del != NULL)
    {
        int mdelayms = del->valueint;
        delay(mdelayms);
    }
    // set debug level
    /*
    cJSON *debug = cJSON_GetObjectItemCaseSensitive(doc, "debug");
    if (debug != NULL)
    {
        state_isdebug = debug->valueint;
        esp_log_level_set("MessageController", (esp_log_level_t)state_isdebug);
    }
    */
    // assign default values to thhe variables
    cJSON *BUSY = cJSON_GetObjectItemCaseSensitive(doc, "isBusy");
    if (BUSY != NULL)
    {
        state_is_busy = BUSY->valueint;
    }
    cJSON *reset = cJSON_GetObjectItemCaseSensitive(doc, "resetPrefs");
    if (reset != NULL)
    {
        log_i("resetPreferences");
        // Wipe the whole "UC2" NVS namespace (was uc2::Preferences).
        nvs_handle_t h = 0;
        if (nvs_open("UC2", NVS_READWRITE, &h) == ESP_OK)
        {
            nvs_erase_all(h);
            nvs_commit(h);
            nvs_close(h);
        }
        esp_restart();
        return 1;
    }
    return 1;
}

cJSON *state_get(cJSON *docin)
{
    // Custom function accessible by the API
    // return json {"state":{...}} as qid
    // {"task":"/state_get",  "qid":1}
    // {"task":"/state_get", "isBusy":1}
    // {"task":"/state_get", "heap":1}
    // {"task":"/state_get", "power":1}  // CAN-bus power state (1=ON)
    // {"task":"/state_get", "estop":1}  // E-stop polarity + raw level + active
    // {"task":"/state_get", "pindef":1}  // pin definition name from PinConfig struct
    // This returns: {"identifier_name":UC2_Feather, "identifier_id":V2.0, "identifier_date":__DATE__ __TIME__, "identifier_author":BD, "IDENTIFIER_NAME":uc2-esp, "configIsSet":0, "pindef":UC2}
    cJSON *doc = cJSON_CreateObject();
    cJSON *st = cJSON_CreateObject();
    cJSON_AddItemToObject(doc, "state", st);

    // GET SOME PARAMETERS HERE
    int qid = cjson_tool_get_int(docin, "qid", 0);
    cJSON *BUSY = cJSON_GetObjectItemCaseSensitive(docin, "isBusy");
    cJSON *HEAP = cJSON_GetObjectItemCaseSensitive(docin, "heap");
    cJSON *POWER = cJSON_GetObjectItemCaseSensitive(docin, "power");
    cJSON *ESTOP = cJSON_GetObjectItemCaseSensitive(docin, "estop");
    if (BUSY != NULL)
    {
        cJSON_AddItemToObject(st, "isBusy", cJSON_CreateNumber(((int)state_is_busy)));
    }
    else if (HEAP != NULL)
    {
        cJSON_AddItemToObject(st, "heap", cJSON_CreateNumber(esp_get_free_heap_size()));
    }
    else if (POWER != NULL)
    {
        // {"task":"/state_get","power":1} -> CAN-bus power state (1=ON)
        cJSON_AddItemToObject(st, "power", cJSON_CreateNumber((int)state_get_bus_power()));
    }
    else if (ESTOP != NULL)
    {
#if CONFIG_UC2_DIGITAL_IN_CONTROLLER
        // {"task":"/state_get","estop":1} -> E-stop diagnostics for tuning.
        // estopRaw is the live pin level; flip estopPolarity until pressing
        // the button makes estopActive read 1.
        cJSON_AddItemToObject(st, "estopPolarity", cJSON_CreateNumber(digitalin_get_emergency_polarity()));
        cJSON_AddItemToObject(st, "estopRaw", cJSON_CreateNumber(digitalin_get_emergency_raw()));
        cJSON_AddItemToObject(st, "estopActive", cJSON_CreateNumber(digitalin_is_emergency_active() ? 1 : 0));
#endif
    }
    else
    {
        cJSON_AddItemToObject(st, "identifier_name", cJSON_CreateString(state_identifier_name));
        cJSON_AddItemToObject(st, "identifier_id", cJSON_CreateString(state_identifier_id));
        cJSON_AddItemToObject(st, "identifier_date", cJSON_CreateString(state_identifier_date));
        cJSON_AddItemToObject(st, "identifier_author", cJSON_CreateString(state_identifier_author));
        cJSON_AddItemToObject(st, "IDENTIFIER_NAME", cJSON_CreateString(state_identifier_product));
        cJSON_AddItemToObject(st, "configIsSet", cJSON_CreateNumber(state_config_set));
        cJSON_AddItemToObject(st, "pindef", cJSON_CreateString(pinConfig.pindefName));
        cJSON_AddItemToObject(st, "I2C_SLAVE", cJSON_CreateNumber(pinConfig.I2C_CONTROLLER_TYPE));
        if (pinConfig.BUSPOWER_OFF_PIN >= 0)
            cJSON_AddItemToObject(st, "power", cJSON_CreateNumber((int)state_get_bus_power()));
#if CONFIG_UC2_DIGITAL_IN_CONTROLLER
        if (pinConfig.pinEmergencyExit >= 0)
        {
            cJSON_AddItemToObject(st, "estopPolarity", cJSON_CreateNumber(digitalin_get_emergency_polarity()));
            cJSON_AddItemToObject(st, "estopRaw", cJSON_CreateNumber(digitalin_get_emergency_raw()));
            cJSON_AddItemToObject(st, "estopActive", cJSON_CreateNumber(digitalin_is_emergency_active() ? 1 : 0));
        }
#endif
        // Report the active node-id from runtimeConfig (may differ from
        // pinConfig.CAN_ID_CURRENT after LSS conflict resolution / NVS edits)
        // (was #ifdef CAN_CONTROLLER / CAN_CONTROLLER_CANOPEN — identical
        // fields in both blocks; CAN_CONTROLLER is not defined in any current
        // build.)
#if CONFIG_UC2_CAN_CONTROLLER_CANOPEN
        cJSON_AddItemToObject(st, "CAN_SLAVE", cJSON_CreateNumber(runtimeConfig.canNodeId));
        cJSON_AddItemToObject(st, "CAN_SLAVE_DEFAULT", cJSON_CreateNumber(pinConfig.CAN_ID_CURRENT));
#endif
        // cJSON_AddItemToObject(st, "heap", cJSON_CreateNumber(esp_get_free_heap_size()));
    }
    cJSON_AddItemToObject(doc, "qid", cJSON_CreateNumber(qid));

    return doc;
}

void state_set_busy(bool busy)
{
    state_is_busy = busy;
}

bool state_get_busy(void)
{
    return state_is_busy;
}

void state_print_info(void)
{
    if (state_isdebug)
        log_i("You can use this software by sending JSON strings, A full documentation can be found here:");
    if (state_isdebug)
        log_i("https://github.com/openUC2/UC2-REST/");
    // log_i("A first try can be: \{\"task\": \"/state_get\"");
}

void state_stop_ota(void)
{
    // (The ARDUINO WiFi-AP body is not available in raw-IDF builds — flag
    // only, same as the C++ build for every current app.)
    state_otarunning = false;
}

void state_start_ota(void)
{
    // Start a wifi hotspot using an SSID based on the ESPs MAC address with
    // now password and start an OTA server. This is a blocking function.
    // The WiFi-AP body needs the uc2 WiFi/OTA façade (I2C_MASTER or ARDUINO
    // only); neither is defined for any current build (C or CXX), so this is
    // the log-warn + flag path — identical to what the C++ original did in
    // every current app.
    log_w("startOTA: WiFi-AP OTA path needs the uc2 WiFi/OTA façade (I2C_MASTER or ARDUINO only)");
    state_otarunning = true;
}

cJSON *state_get_modules(void)
{
    cJSON *doc = cJSON_CreateObject();
    cJSON *mod = cJSON_CreateObject();
    // T1.3: JsonKeys.h dropped — literal key strings (same values).
    // NOTE: key_gpio was never defined in JsonKeys.h either (dead ref under
    // GPIO_CONTROLLER, no build defines it) — "gpio" literal keeps the shape.
    cJSON_AddItemToObject(doc, "modules", mod);
#if CONFIG_UC2_LED_CONTROLLER
    cJSON_AddItemToObject(mod, "led", cJSON_CreateNumber(pinConfig.LED_PIN >= 0));
#endif
#if CONFIG_UC2_MOTOR_CONTROLLER
    cJSON_AddItemToObject(mod, "motor", cJSON_CreateNumber(pinConfig.MOTOR_ENABLE >= 0));
#endif
#ifdef GPIO_CONTROLLER
    cJSON_AddItemToObject(mod, "gpio", cJSON_CreateNumber((pinConfig.GPIO_PIN_1 >= 0 || pinConfig.GPIO_PIN_2 >= 0 || pinConfig.GPIO_PIN_3 >= 0)));
#endif
#if CONFIG_UC2_MESSAGE_CONTROLLER
    cJSON_AddItemToObject(mod, "message", cJSON_CreateNumber(1));
#endif
#if CONFIG_UC2_HOME_MOTOR
    cJSON_AddItemToObject(mod, "home", cJSON_CreateNumber((pinConfig.DIGITAL_IN_1 >= 0 || pinConfig.DIGITAL_IN_2 >= 0 || pinConfig.DIGITAL_IN_3 >= 0)));
#endif
    cJSON_AddItemToObject(mod, "analogin", cJSON_CreateNumber((pinConfig.analogin_PIN_0 >= 0 || pinConfig.analogin_PIN_1 >= 0 || pinConfig.analogin_PIN_2 >= 0 || pinConfig.analogin_PIN_3 >= 0)));
    cJSON_AddItemToObject(mod, "pid", cJSON_CreateNumber((pinConfig.pid1 >= 0 || pinConfig.pid2 >= 0 || pinConfig.pid3 >= 0)));
    cJSON_AddItemToObject(mod, "laser", cJSON_CreateNumber((pinConfig.LASER_1 >= 0 || pinConfig.LASER_2 >= 0 || pinConfig.LASER_3 >= 0)));
    cJSON_AddItemToObject(mod, "dac", cJSON_CreateNumber((pinConfig.dac_fake_1 >= 0 || pinConfig.dac_fake_2 >= 0)));
    cJSON_AddItemToObject(mod, "analogout", cJSON_CreateNumber((pinConfig.analogout_PIN_1 >= 0 || pinConfig.analogout_PIN_2 >= 0 || pinConfig.analogout_PIN_3 >= 0)));
    cJSON_AddItemToObject(mod, "digitalout", cJSON_CreateNumber((pinConfig.DIGITAL_OUT_1 >= 0 || pinConfig.DIGITAL_OUT_2 >= 0 || pinConfig.DIGITAL_OUT_3 >= 0)));
    cJSON_AddItemToObject(mod, "digitalin", cJSON_CreateNumber((pinConfig.DIGITAL_IN_1 >= 0 || pinConfig.DIGITAL_IN_2 >= 0 || pinConfig.DIGITAL_IN_3 >= 0)));
#ifdef SCANNER_CONTROLLER
    cJSON_AddItemToObject(mod, "scanner", cJSON_CreateNumber(1));
#endif

    return doc;
}

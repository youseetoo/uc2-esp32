// T4.1: C (was C++). namespace NVSConfig -> free functions (nvsconfig_*).
// uc2::Preferences -> raw nvs_* (get-with-default + put/commit semantics
// replicated in the nvsg_* helpers below). The original read the C++ global
// pinConfig.CAN_ID_CURRENT in loadConfig; a .c TU cannot touch a C++ object, so
// the caller now passes (pinConfig.CAN_ID_CURRENT & 0x7F) as pin_cfg_node_id.
// That value is captured (s_pin_cfg_node_id) so nvsconfig_reset can re-load with
// the same board default. log_* -> ESP_LOG* (esp_log.h, C-compatible).
//
// C feature gating: raw-IDF build -D feature flags are CXX-only (toolchain/
// cxxflags), so a C TU must gate on the CONFIG_UC2_* Kconfig symbols
// (sdkconfig.h) — the same mapping the component CMakeLists use. Flags with no
// Kconfig symbol (ANALOG_JOYSTICK, WIFI, PID_CONTROLLER, SCANNER_CONTROLLER,
// OBJECTIVE_CONTROLLER, ...) keep the bare #ifdef: they are undefined for every
// current build (C and CXX alike), so both evaluate false identically.
#include "sdkconfig.h"
#include "NVSConfig.h"
#include "RuntimeConfig.h"

#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "NVSConfig";
static const char *NVS_NAMESPACE = "uc2cfg";

// Global RuntimeConfig instance (C99 designated init; all bool fields default
// to false, matching the old in-class member initializers).
RuntimeConfig runtimeConfig = {
    .canRole = NODE_ROLE_STANDALONE,
    .canNodeId = 10,
    .canMotorAxis = 1,
};

// Board CAN id captured by the most recent nvsconfig_load (used by reset).
static uint8_t s_pin_cfg_node_id = 10;

// raw-nvs helper: nvs_get_u8 with a default (like uc2::Preferences::getUChar).
static uint8_t nvsg_get_u8(nvs_handle_t h, const char *k, uint8_t def)
{
    uint8_t v = def;
    if (h == 0 || nvs_get_u8(h, k, &v) != ESP_OK) {
        return def;
    }
    return v;
}

// ── Compile-time defaults derived from build flags ──────────────────────────

#if CONFIG_UC2_MOTOR_CONTROLLER
  #define DEFAULT_MOTOR true
#else
  #define DEFAULT_MOTOR false
#endif
#if CONFIG_UC2_LASER_CONTROLLER
  #define DEFAULT_LASER true
#else
  #define DEFAULT_LASER false
#endif
#if CONFIG_UC2_LED_CONTROLLER
  #define DEFAULT_LED true
#else
  #define DEFAULT_LED false
#endif
#if CONFIG_UC2_HOME_MOTOR
  #define DEFAULT_HOME true
#else
  #define DEFAULT_HOME false
#endif
#if CONFIG_UC2_DIGITAL_IN_CONTROLLER
  #define DEFAULT_DIGITALIN true
#else
  #define DEFAULT_DIGITALIN false
#endif
#if CONFIG_UC2_DIGITAL_OUT_CONTROLLER
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
#if CONFIG_UC2_DAC_CONTROLLER
  #define DEFAULT_DAC true
#else
  #define DEFAULT_DAC false
#endif
#if CONFIG_UC2_TMC_CONTROLLER
  #define DEFAULT_TMC true
#else
  #define DEFAULT_TMC false
#endif
#if CONFIG_UC2_LINEAR_ENCODER_CONTROLLER
  #define DEFAULT_ENCODER true
#else
  #define DEFAULT_ENCODER false
#endif
#ifdef ANALOG_JOYSTICK
  #define DEFAULT_JOYSTICK true
#else
  #define DEFAULT_JOYSTICK false
#endif
#if CONFIG_UC2_GALVO_CONTROLLER
  #define DEFAULT_GALVO true
#else
  #define DEFAULT_GALVO false
#endif
#if CONFIG_UC2_BLUETOOTH
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
#if CONFIG_UC2_MESSAGE_CONTROLLER
  #define DEFAULT_MESSAGE true
#else
  #define DEFAULT_MESSAGE false
#endif
#if CONFIG_UC2_DIAL_CONTROLLER
  #define DEFAULT_DIAL true
#else
  #define DEFAULT_DIAL false
#endif
#ifdef OBJECTIVE_CONTROLLER
  #define DEFAULT_OBJECTIVE true
#else
  #define DEFAULT_OBJECTIVE false
#endif
#if CONFIG_UC2_HEAT_CONTROLLER
  #define DEFAULT_HEAT true
#else
  #define DEFAULT_HEAT false
#endif
#if CONFIG_UC2_FAN_CONTROLLER || CONFIG_UC2_TMP102_CONTROLLER
  #define DEFAULT_FAN true
#else
  #define DEFAULT_FAN false
#endif

// CAN role default. NODE_ROLE (the legacy -D macro) is undefined in every C TU,
// so the original C++ (NODE_ROLE == 1)/(NODE_ROLE == 2) comparisons always
// folded false and the laser app silently defaulted to STANDALONE. The Kconfig
// int CONFIG_UC2_NODE_ROLE carries the same value, restoring the intended
// branch (laser: CANopen slave).
#if CONFIG_UC2_CAN_CONTROLLER_CANOPEN && (CONFIG_UC2_NODE_ROLE == 1)
  #define DEFAULT_CAN_ROLE (NODE_ROLE_CAN_MASTER)
#elif CONFIG_UC2_CAN_CONTROLLER_CANOPEN && (CONFIG_UC2_NODE_ROLE == 2)
  #define DEFAULT_CAN_ROLE (NODE_ROLE_CAN_SLAVE)
#elif CONFIG_UC2_CAN_BUS_ENABLED
  #define DEFAULT_CAN_ROLE (NODE_ROLE_CAN_MASTER)
#else
  #define DEFAULT_CAN_ROLE (NODE_ROLE_STANDALONE)
#endif

// ── Implementation ──────────────────────────────────────────────────────────

void nvsconfig_load(uint8_t pin_cfg_node_id)
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        h = 0; // namespace absent / not yet initialized -> compile-time defaults
    }

    runtimeConfig.motor      = nvsg_get_u8(h, "motor",      DEFAULT_MOTOR) != 0;
    runtimeConfig.laser      = nvsg_get_u8(h, "laser",      DEFAULT_LASER) != 0;
    runtimeConfig.led        = nvsg_get_u8(h, "led",        DEFAULT_LED) != 0;
    runtimeConfig.home       = nvsg_get_u8(h, "home",       DEFAULT_HOME) != 0;
    runtimeConfig.digitalIn  = nvsg_get_u8(h, "digitalIn",  DEFAULT_DIGITALIN) != 0;
    runtimeConfig.digitalOut = nvsg_get_u8(h, "digitalOut", DEFAULT_DIGITALOUT) != 0;
    runtimeConfig.analogIn   = nvsg_get_u8(h, "analogIn",   DEFAULT_ANALOGIN) != 0;
    runtimeConfig.analogOut  = nvsg_get_u8(h, "analogOut",  DEFAULT_ANALOGOUT) != 0;
    runtimeConfig.dac        = nvsg_get_u8(h, "dac",        DEFAULT_DAC) != 0;
    runtimeConfig.tmc        = nvsg_get_u8(h, "tmc",        DEFAULT_TMC) != 0;
    runtimeConfig.encoder    = nvsg_get_u8(h, "encoder",    DEFAULT_ENCODER) != 0;
    runtimeConfig.joystick   = nvsg_get_u8(h, "joystick",   DEFAULT_JOYSTICK) != 0;
    runtimeConfig.galvo      = nvsg_get_u8(h, "galvo",      DEFAULT_GALVO) != 0;
    runtimeConfig.bluetooth  = nvsg_get_u8(h, "bluetooth",  DEFAULT_BLUETOOTH) != 0;
    runtimeConfig.wifi       = nvsg_get_u8(h, "wifi",       DEFAULT_WIFI) != 0;
    runtimeConfig.pid        = nvsg_get_u8(h, "pid",        DEFAULT_PID) != 0;
    runtimeConfig.scanner    = nvsg_get_u8(h, "scanner",    DEFAULT_SCANNER) != 0;
    runtimeConfig.message    = nvsg_get_u8(h, "message",    DEFAULT_MESSAGE) != 0;
    runtimeConfig.dial       = nvsg_get_u8(h, "dial",       DEFAULT_DIAL) != 0;
    runtimeConfig.objective  = nvsg_get_u8(h, "objective",  DEFAULT_OBJECTIVE) != 0;
    runtimeConfig.heat       = nvsg_get_u8(h, "heat",       DEFAULT_HEAT) != 0;
    runtimeConfig.fan        = nvsg_get_u8(h, "fan",        DEFAULT_FAN) != 0;

    runtimeConfig.canRole = (NodeRole)nvsg_get_u8(h, "canRole", (uint8_t)DEFAULT_CAN_ROLE);

    // Default canNodeId comes from the board's PinConfig (CAN_ID_CURRENT), passed
    // in by the C++ caller. On first boot the NVS key is absent -> the hardware
    // default is used; on later boots the stored value wins (LSS / serial edits).
    if (pin_cfg_node_id < 1 || pin_cfg_node_id > 127) {
        pin_cfg_node_id = 10; // safe fallback
    }
    s_pin_cfg_node_id = pin_cfg_node_id;
    runtimeConfig.canNodeId = nvsg_get_u8(h, "canNodeId", pin_cfg_node_id);
    runtimeConfig.canMotorAxis = nvsg_get_u8(h, "canMotAxis", 1);

    if (h) {
        nvs_close(h);
    }

    ESP_LOGI(TAG, "Loaded: motor=%d laser=%d led=%d home=%d bt=%d wifi=%d canRole=%d canNodeId=%u",
             runtimeConfig.motor, runtimeConfig.laser, runtimeConfig.led,
             runtimeConfig.home, runtimeConfig.bluetooth, runtimeConfig.wifi,
             (int)runtimeConfig.canRole, (unsigned)runtimeConfig.canNodeId);
}

void nvsconfig_save(void)
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }

    nvs_set_u8(h, "motor",      runtimeConfig.motor ? 1 : 0);
    nvs_set_u8(h, "laser",      runtimeConfig.laser ? 1 : 0);
    nvs_set_u8(h, "led",        runtimeConfig.led ? 1 : 0);
    nvs_set_u8(h, "home",       runtimeConfig.home ? 1 : 0);
    nvs_set_u8(h, "digitalIn",  runtimeConfig.digitalIn ? 1 : 0);
    nvs_set_u8(h, "digitalOut", runtimeConfig.digitalOut ? 1 : 0);
    nvs_set_u8(h, "analogIn",   runtimeConfig.analogIn ? 1 : 0);
    nvs_set_u8(h, "analogOut",  runtimeConfig.analogOut ? 1 : 0);
    nvs_set_u8(h, "dac",        runtimeConfig.dac ? 1 : 0);
    nvs_set_u8(h, "tmc",        runtimeConfig.tmc ? 1 : 0);
    nvs_set_u8(h, "encoder",    runtimeConfig.encoder ? 1 : 0);
    nvs_set_u8(h, "joystick",   runtimeConfig.joystick ? 1 : 0);
    nvs_set_u8(h, "galvo",      runtimeConfig.galvo ? 1 : 0);
    nvs_set_u8(h, "bluetooth",  runtimeConfig.bluetooth ? 1 : 0);
    nvs_set_u8(h, "wifi",       runtimeConfig.wifi ? 1 : 0);
    nvs_set_u8(h, "pid",        runtimeConfig.pid ? 1 : 0);
    nvs_set_u8(h, "scanner",    runtimeConfig.scanner ? 1 : 0);
    nvs_set_u8(h, "message",    runtimeConfig.message ? 1 : 0);
    nvs_set_u8(h, "dial",       runtimeConfig.dial ? 1 : 0);
    nvs_set_u8(h, "objective",  runtimeConfig.objective ? 1 : 0);
    nvs_set_u8(h, "heat",       runtimeConfig.heat ? 1 : 0);
    nvs_set_u8(h, "fan",        runtimeConfig.fan ? 1 : 0);

    nvs_set_u8(h, "canRole",    (uint8_t)runtimeConfig.canRole);
    nvs_set_u8(h, "canNodeId",  runtimeConfig.canNodeId);
    nvs_set_u8(h, "canMotAxis", runtimeConfig.canMotorAxis);

    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Config saved to NVS");
}

void nvsconfig_reset(void)
{
    nvs_handle_t h = 0;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "NVS namespace cleared, reloading compile-time defaults");
    nvsconfig_load(s_pin_cfg_node_id);
}

cJSON *nvsconfig_to_json(void)
{
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return NULL;
    }

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
    cJSON_AddBoolToObject(json, "fan",        runtimeConfig.fan);

    cJSON_AddNumberToObject(json, "canRole",      (int)runtimeConfig.canRole);
    cJSON_AddNumberToObject(json, "canNodeId",    runtimeConfig.canNodeId);
    cJSON_AddNumberToObject(json, "canMotorAxis", runtimeConfig.canMotorAxis);

    return json;
}

void nvsconfig_from_json(cJSON *json)
{
    if (!json) {
        return;
    }

    cJSON *item;

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

    item = cJSON_GetObjectItemCaseSensitive(json, "fan");
    if (item && cJSON_IsBool(item)) runtimeConfig.fan = cJSON_IsTrue(item);

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

// T4.1: pure C (was the C++ namespace DigitalInController).
// Serial output goes through the framed C path (serial_out_safe_serialize_json);
// the E-stop polarity is read/written straight from NVS (was uc2::Preferences).
#include <PinConfig.h>
#include "DigitalInController.h"
// T1.3: direct IDF (no Arduino) — uc2 façade for gpio/prefs/time/log.
#include <uc2_hal.h>
#include "cJsonTool.h"
#include "uc2_serial_out.h"      // serial_out_safe_serialize_json
#include "nvs.h"
// JsonKeys.h dropped (Arduino/pgmspace): key_digitalin == "digitalin".

static const char *TAG = "DigitalInController";

// TCA9535 endstop reads (uc2_periph); only needed on USE_TCA9535 boards.
// tca_controller.h is pure C (T4.1); the C++ island is the vendor TCA9555 leaf.
#ifdef USE_TCA9535
#include "tca_controller.h"
#endif

static int digitalin_val_1 = 0;
static int digitalin_val_2 = 0;
static int digitalin_val_3 = 0;

// ── Emergency-STOP state (file-local) ────────────────────────────────
// pinEmergencyExit senses a hardware E-stop. The ASSERTED (emergency) logic
// level is configurable at runtime via emergency_polarity:
//   0 (LOW)  = active-LOW  (asserted reads LOW,  idle HIGH)
//   1 (HIGH) = active-HIGH (asserted reads HIGH, idle LOW)
// Persisted in NVS ("estopPol"); default from pinConfig.pinEmergencyExitPolarity.
static int8_t   emergency_polarity     = 1;
static bool     emergency_initialized  = false; // pin configured + polarity loaded
static bool     emergency_active       = false; // debounced logical state
static int      emergency_raw_last     = -1;    // last raw reading
static uint32_t emergency_last_change_ms = 0;   // for debouncing
static const uint32_t EMERGENCY_DEBOUNCE_MS = 50;

// Custom function accessible by the API
int digitalin_act(cJSON *jsonDocument)
{
	int qid = cjson_tool_get_int(jsonDocument, "qid", 0);
	// here you can do something
	log_d("digitalin_act_fct");
	(void)TAG;
	return qid;
}

// Custom function accessible by the API
cJSON *digitalin_get(cJSON *jsonDocument)
{
	// GET SOME PARAMETERS HERE
	int digitalinid = cjson_tool_get_int(jsonDocument, "digitalinid", 1);
	int digitalinval = digitalin_get_val(digitalinid);

	// create new json document
	cJSON *monitor_json = cJSON_CreateObject();
	cJSON *digitalinholder = cJSON_CreateObject();
	cJSON_AddItemToObject(monitor_json, "digitalin", digitalinholder);
	cJSON_AddItemToObject(digitalinholder, "digitalinid", cJSON_CreateNumber(digitalinid));
	cJSON_AddItemToObject(digitalinholder, "digitalinval", cJSON_CreateNumber(digitalinval));

	cjson_tool_set_int(monitor_json, "qid",
		cjson_tool_get_int(jsonDocument, "qid", 0));

	return monitor_json;
}

bool digitalin_get_val(int digitalinid)
{
	int pin = -1;
	if (digitalinid == 1)
		pin = pinConfig.DIGITAL_IN_1;
	else if (digitalinid == 2)
		pin = pinConfig.DIGITAL_IN_2;
	else if (digitalinid == 3)
		pin = pinConfig.DIGITAL_IN_3;
	if (pin < 0)
		return false;
#ifdef USE_TCA9535
	return tca_read_endstop((uint8_t)pin);
#else
	return digitalRead((uint8_t)pin);
#endif
}

void digitalin_setup(void)
{
#ifdef USE_TCA9535
	log_i("Setting Up TCA9535 for digitalin");
#else
	log_i("Setting Up digitalin");
	if (pinConfig.DIGITAL_IN_1 >= 0)
	{
		log_i("DigitalIn 1: %i", pinConfig.DIGITAL_IN_1);
		pinMode((uint8_t)pinConfig.DIGITAL_IN_1, INPUT_PULLDOWN);
	}
	if (pinConfig.DIGITAL_IN_2 >= 0)
	{
		log_i("DigitalIn 2: %i", pinConfig.DIGITAL_IN_2);
		pinMode((uint8_t)pinConfig.DIGITAL_IN_2, INPUT_PULLDOWN);
	}
	if (pinConfig.DIGITAL_IN_3 >= 0)
	{
		log_i("DigitalIn 3: %i", pinConfig.DIGITAL_IN_3);
		pinMode((uint8_t)pinConfig.DIGITAL_IN_3, INPUT_PULLDOWN);
	}
#endif
}

void digitalin_loop(void)
{
	if (pinConfig.DIGITAL_IN_1 >= 0)
		digitalin_val_1 = digitalin_get_val(1);
	if (pinConfig.DIGITAL_IN_2 >= 0)
		digitalin_val_2 = digitalin_get_val(2);
	if (pinConfig.DIGITAL_IN_3 >= 0)
		digitalin_val_3 = digitalin_get_val(3);
}

// ── Emergency-STOP implementation ────────────────────────────────────

// Read the E-stop polarity from NVS (default from pinConfig); was
// uc2::Preferences getChar("estopPol", ...).
static int8_t estop_pol_read(void)
{
	nvs_handle_t h;
	int8_t v = pinConfig.pinEmergencyExitPolarity;
	if (nvs_open("UC2", NVS_READONLY, &h) == ESP_OK)
	{
		if (nvs_get_i8(h, "estopPol", &v) != ESP_OK)
			v = pinConfig.pinEmergencyExitPolarity;
		nvs_close(h);
	}
	return v;
}

// Persist the E-stop polarity (was uc2::Preferences putChar("estopPol", ...)).
static void estop_pol_write(int8_t pol)
{
	nvs_handle_t h;
	if (nvs_open("UC2", NVS_READWRITE, &h) != ESP_OK)
		return;
	nvs_set_i8(h, "estopPol", pol);
	nvs_commit(h);
	nvs_close(h);
}

// Emit the async {"emergency":...} serial event so the UC2-REST bridge can
// react via its registered callback. qid 0 marks an unsolicited event
// (host-issued command qids start at 1).
static void emit_emergency_event(bool active)
{
	cJSON *doc = cJSON_CreateObject();
	if (doc == NULL)
		return;
	cJSON *em = cJSON_CreateObject();
	cJSON_AddItemToObject(doc, "emergency", em);
	cjson_tool_set_int(em, "active", active ? 1 : 0);
	cJSON_AddStringToObject(em, "reason", "estop");
	if (active)
		cJSON_AddStringToObject(em, "msg",
			"Emergency stop activated! Shutting down all systems.");
	cjson_tool_set_int(doc, "qid", 0);
	serial_out_safe_serialize_json(doc); // frames with ++\n..\n--\n
	cJSON_Delete(doc);
}

// Fire once per stable transition of the E-stop line.
static void trigger_emergency(bool active)
{
	emergency_active = active;
	if (active)
	{
		log_e("Emergency stop activated! Shutting down all systems.");
		// Cutting CAN-bus power stops every slave at once: motors, lasers,
		// heaters and lights all lose power immediately.
		// TODO: Decide what's the right action: State::setBusPower(false);
	}
	else
	{
		// Power is NOT auto-restored on release — the host must explicitly
		// re-enable it via /state_act {"power":1} once it is safe.
		log_w("Emergency stop released (bus power stays OFF until re-enabled)");
	}
	emit_emergency_event(active);
}

// Configure the E-stop pin. On pins that support internal pulls we bias the
// idle level OPPOSITE to the asserted level, so a disconnected switch settles
// to a defined (non-emergency) level instead of floating. NOTE: GPIO34-39 are
// input-only with NO internal pulls — those require an EXTERNAL pull resistor.
static void apply_emergency_pin_mode(void)
{
	if (pinConfig.pinEmergencyExit < 0)
		return;
	if (emergency_polarity == HIGH)
		pinMode((uint8_t)pinConfig.pinEmergencyExit, INPUT_PULLDOWN); // asserted HIGH -> idle LOW
	else
		pinMode((uint8_t)pinConfig.pinEmergencyExit, INPUT_PULLUP);   // asserted LOW  -> idle HIGH
}

static void init_emergency(void)
{
	// Load the configurable polarity from NVS (default from pinConfig).
	emergency_polarity = (int8_t)estop_pol_read();
	emergency_polarity = emergency_polarity ? HIGH : LOW; // clamp to 0/1

	apply_emergency_pin_mode();
	emergency_raw_last = digitalRead((uint8_t)pinConfig.pinEmergencyExit);
	emergency_last_change_ms = millis();
	emergency_active = (emergency_raw_last == emergency_polarity);
	emergency_initialized = true;
	log_i("Emergency-STOP on GPIO%d: polarity=%s, raw=%d, initial=%s",
		  pinConfig.pinEmergencyExit,
		  (emergency_polarity == HIGH) ? "active-HIGH" : "active-LOW",
		  emergency_raw_last, emergency_active ? "ASSERTED" : "normal");
	// If the button is already held at boot, enter the safe state at once.
	if (emergency_active)
		trigger_emergency(true);
}

// Polled every loop. The first call lazily configures the pin (so there is
// no separate setup step); subsequent calls run the debounced edge detector.
void digitalin_check_emergency_stop(void)
{
	if (pinConfig.pinEmergencyExit < 0)
		return;

	if (!emergency_initialized)
	{
		init_emergency();
		return;
	}

	int raw = digitalRead((uint8_t)pinConfig.pinEmergencyExit);
	uint32_t now = millis();
	if (raw != emergency_raw_last)
	{
		// Reading changed — restart the debounce window.
		emergency_raw_last = raw;
		emergency_last_change_ms = now;
		return;
	}
	if ((now - emergency_last_change_ms) < EMERGENCY_DEBOUNCE_MS)
		return; // not stable long enough yet
	bool pressed = (raw == emergency_polarity);
	if (pressed != emergency_active)
		trigger_emergency(pressed);
}

bool digitalin_is_emergency_active(void)
{
	return emergency_active;
}

int8_t digitalin_get_emergency_polarity(void)
{
	// Before first poll, report the compile-time default.
	if (emergency_polarity < 0)
		return pinConfig.pinEmergencyExitPolarity ? HIGH : LOW;
	return emergency_polarity;
}

// Live raw reading of the E-stop pin (HIGH/LOW), or -1 if not configured.
// Useful for picking the correct polarity from the host.
int digitalin_get_emergency_raw(void)
{
	if (pinConfig.pinEmergencyExit < 0)
		return -1;
	return digitalRead((uint8_t)pinConfig.pinEmergencyExit);
}

// Set + persist the asserted (emergency) logic level. Takes effect live.
void digitalin_set_emergency_polarity(int8_t pol)
{
	emergency_polarity = pol ? HIGH : LOW;
	estop_pol_write(emergency_polarity);

	if (pinConfig.pinEmergencyExit < 0)
		return;
	apply_emergency_pin_mode();
	// Re-baseline against the current level; emit if the logical state
	// changed under the new polarity so the host stays in sync.
	emergency_raw_last = digitalRead((uint8_t)pinConfig.pinEmergencyExit);
	emergency_last_change_ms = millis();
	emergency_initialized = true;
	bool pressed = (emergency_raw_last == emergency_polarity);
	log_i("E-stop polarity -> %s (raw=%d -> %s)",
		  (emergency_polarity == HIGH) ? "active-HIGH" : "active-LOW",
		  emergency_raw_last, pressed ? "ASSERTED" : "normal");
	if (pressed != emergency_active)
		trigger_emergency(pressed);
}

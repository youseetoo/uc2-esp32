#include <PinConfig.h>
#include "DigitalInController.h"
#include "Arduino.h"
#include "Preferences.h"
#include "../../JsonKeys.h"
#include "cJsonTool.h"
#include "../i2c/tca_controller.h"
#include "../state/State.h"
#include "../serial/SerialProcess.h"

namespace DigitalInController
{
	// ── Emergency-STOP state (file-local) ────────────────────────────────
	// pinEmergencyExit senses a hardware E-stop. The ASSERTED (emergency) logic
	// level is configurable at runtime via emergencyPolarity:
	//   0 (LOW)  = active-LOW  (asserted reads LOW,  idle HIGH)
	//   1 (HIGH) = active-HIGH (asserted reads HIGH, idle LOW)
	// Persisted in NVS ("estopPol"); default from pinConfig.pinEmergencyExitPolarity.
	static int8_t   emergencyPolarity     = 1;    // -1 = not yet loaded from NVS
	static bool     emergencyInitialized  = false; // pin configured + polarity loaded
	static bool     emergencyActive       = false; // debounced logical state
	static int      emergencyRawLast      = -1;    // last raw reading
	static uint32_t emergencyLastChangeMs = 0;     // for debouncing
	static const uint32_t EMERGENCY_DEBOUNCE_MS = 50;

	// Custom function accessible by the API
	int act(cJSON *jsonDocument)
	{
		int qid = cJsonTool::getJsonInt(jsonDocument, "qid");
		// here you can do something
		log_d("digitalin_act_fct");
		return qid;
	}

	// Custom function accessible by the API
	cJSON *get(cJSON *jsonDocument)
	{

		// GET SOME PARAMETERS HERE
		int digitalinid = cJsonTool::getJsonInt(jsonDocument, "digitalinid", 1);
		int digitalinval = getDigitalVal(digitalinid);

		// create new json document
		cJSON *monitor_json = cJSON_CreateObject();
		cJSON *digitalinholder = cJSON_CreateObject();
		cJSON_AddItemToObject(monitor_json, key_digitalin, digitalinholder);
		cJSON_AddItemToObject(digitalinholder, "digitalinid", cJSON_CreateNumber(digitalinid));
		cJSON_AddItemToObject(digitalinholder, "digitalinval", cJSON_CreateNumber(digitalinval));
		return monitor_json;
	}

	bool getDigitalVal(int digitalinid)
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
#if defined USE_TCA9535
		return tca_controller::tca_read_endstop(pin);
#else
		return digitalRead(pin);
#endif
	}

	void setup()
	{
#ifdef USE_TCA9535
		log_i("Setting Up TCA9535 for digitalin");
#else
		log_i("Setting Up digitalin");
		if (pinConfig.DIGITAL_IN_1 >= 0)
		{
			log_i("DigitalIn 1: %i", pinConfig.DIGITAL_IN_1);
			pinMode(pinConfig.DIGITAL_IN_1, INPUT_PULLDOWN);
		}
		if (pinConfig.DIGITAL_IN_2 >= 0)
		{
			log_i("DigitalIn 2: %i", pinConfig.DIGITAL_IN_2);
			pinMode(pinConfig.DIGITAL_IN_2, INPUT_PULLDOWN);
		}
		if (pinConfig.DIGITAL_IN_3 >= 0)
		{
			log_i("DigitalIn 3: %i", pinConfig.DIGITAL_IN_3);
			pinMode(pinConfig.DIGITAL_IN_3, INPUT_PULLDOWN);
		}
#endif
	}

	void loop()
	{
		if (pinConfig.DIGITAL_IN_1 >= 0)
			digitalin_val_1 = getDigitalVal(1);
		if (pinConfig.DIGITAL_IN_2 >= 0)
			digitalin_val_2 = getDigitalVal(2);
		if (pinConfig.DIGITAL_IN_3 >= 0)
			digitalin_val_3 = getDigitalVal(3);
	}

	// ── Emergency-STOP implementation ────────────────────────────────────

	// Emit the async {"emergency":...} serial event so the UC2-REST bridge can
	// react via its registered callback. qid 0 marks an unsolicited event
	// (host-issued command qids start at 1).
	static void emitEmergencyEvent(bool active)
	{
		cJSON *doc = cJSON_CreateObject();
		if (doc == NULL)
			return;
		cJSON *em = cJSON_CreateObject();
		cJSON_AddItemToObject(doc, "emergency", em);
		cJsonTool::setJsonInt(em, "active", active ? 1 : 0);
		cJSON_AddStringToObject(em, "reason", "estop");
		if (active)
			cJSON_AddStringToObject(em, "msg",
				"Emergency stop activated! Shutting down all systems.");
		cJsonTool::setJsonInt(doc, "qid", 0);
		SerialProcess::serialize(doc); // wraps in ++\n..\n--\n and frees doc
	}

	// Fire once per stable transition of the E-stop line.
	static void triggerEmergency(bool active)
	{
		emergencyActive = active;
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
		emitEmergencyEvent(active);
	}

	// Configure the E-stop pin. On pins that support internal pulls we bias the
	// idle level OPPOSITE to the asserted level, so a disconnected switch settles
	// to a defined (non-emergency) level instead of floating. NOTE: GPIO34-39 are
	// input-only with NO internal pulls — those require an EXTERNAL pull resistor.
	static void applyEmergencyPinMode()
	{
		if (pinConfig.pinEmergencyExit < 0)
			return;
		if (emergencyPolarity == HIGH)
			pinMode(pinConfig.pinEmergencyExit, INPUT_PULLDOWN); // asserted HIGH -> idle LOW
		else
			pinMode(pinConfig.pinEmergencyExit, INPUT_PULLUP);   // asserted LOW  -> idle HIGH
	}

	static void initEmergency()
	{
		// Load the configurable polarity from NVS (default from pinConfig).
		Preferences prefs;
		prefs.begin("UC2", true); // read-only
		emergencyPolarity = (int8_t)prefs.getChar("estopPol", pinConfig.pinEmergencyExitPolarity);
		prefs.end();
		emergencyPolarity = emergencyPolarity ? HIGH : LOW; // clamp to 0/1

		applyEmergencyPinMode();
		emergencyRawLast = digitalRead(pinConfig.pinEmergencyExit);
		emergencyLastChangeMs = millis();
		emergencyActive = (emergencyRawLast == emergencyPolarity);
		emergencyInitialized = true;
		log_i("Emergency-STOP on GPIO%d: polarity=%s, raw=%d, initial=%s",
			  pinConfig.pinEmergencyExit,
			  (emergencyPolarity == HIGH) ? "active-HIGH" : "active-LOW",
			  emergencyRawLast, emergencyActive ? "ASSERTED" : "normal");
		// If the button is already held at boot, enter the safe state at once.
		if (emergencyActive)
			triggerEmergency(true);
	}

	// Polled every loop. The first call lazily configures the pin (so there is
	// no separate setup step); subsequent calls run the debounced edge detector.
	void checkEmergencyStop()
	{
		if (pinConfig.pinEmergencyExit < 0)
			return;

		if (!emergencyInitialized)
		{
			initEmergency();
			return;
		}

		int raw = digitalRead(pinConfig.pinEmergencyExit);
		uint32_t now = millis();
		if (raw != emergencyRawLast)
		{
			// Reading changed — restart the debounce window.
			emergencyRawLast = raw;
			emergencyLastChangeMs = now;
			return;
		}
		if ((now - emergencyLastChangeMs) < EMERGENCY_DEBOUNCE_MS)
			return; // not stable long enough yet
		bool pressed = (raw == emergencyPolarity);
		if (pressed != emergencyActive)
			triggerEmergency(pressed);
	}

	bool isEmergencyActive()
	{
		return emergencyActive;
	}

	int8_t getEmergencyPolarity()
	{
		// Before first poll, report the compile-time default.
		if (emergencyPolarity < 0)
			return pinConfig.pinEmergencyExitPolarity ? HIGH : LOW;
		return emergencyPolarity;
	}

	// Live raw reading of the E-stop pin (HIGH/LOW), or -1 if not configured.
	// Useful for picking the correct polarity from the host.
	int getEmergencyRaw()
	{
		if (pinConfig.pinEmergencyExit < 0)
			return -1;
		return digitalRead(pinConfig.pinEmergencyExit);
	}

	// Set + persist the asserted (emergency) logic level. Takes effect live.
	void setEmergencyPolarity(int8_t pol)
	{
		emergencyPolarity = pol ? HIGH : LOW;
		Preferences prefs;
		prefs.begin("UC2", false);
		prefs.putChar("estopPol", emergencyPolarity);
		prefs.end();

		if (pinConfig.pinEmergencyExit < 0)
			return;
		applyEmergencyPinMode();
		// Re-baseline against the current level; emit if the logical state
		// changed under the new polarity so the host stays in sync.
		emergencyRawLast = digitalRead(pinConfig.pinEmergencyExit);
		emergencyLastChangeMs = millis();
		emergencyInitialized = true;
		bool pressed = (emergencyRawLast == emergencyPolarity);
		log_i("E-stop polarity -> %s (raw=%d -> %s)",
			  (emergencyPolarity == HIGH) ? "active-HIGH" : "active-LOW",
			  emergencyRawLast, pressed ? "ASSERTED" : "normal");
		if (pressed != emergencyActive)
			triggerEmergency(pressed);
	}
} // namespace DigitalInController

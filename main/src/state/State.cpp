#include <PinConfig.h>
#include "State.h"
#include "esp_log.h"
#include "../config/ConfigController.h"
#include "../config/RuntimeConfig.h"
#include "Preferences.h"
#include "cJsonTool.h"
#include "Arduino.h"
#include "JsonKeys.h"
#include <WiFi.h>
#include <WiFiAP.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "../i2c/i2c_master.h"
#ifdef DIGITAL_IN_CONTROLLER
#include "../digitalin/DigitalInController.h"
#endif

namespace State
{

	// Tracks the logical CAN-bus power state (true = ON). Mirrors the
	// BUSPOWER_OFF_PIN gate so /state_get can report it without reading the pin.
	static bool busPowerOn = true;

	void setBusPower(bool on)
	{
		busPowerOn = on;
		if (pinConfig.BUSPOWER_OFF_PIN >= 0)
		{
			pinMode(pinConfig.BUSPOWER_OFF_PIN, OUTPUT);
			// Active-low-for-on gate: LOW enables bus power, HIGH cuts it.
			digitalWrite(pinConfig.BUSPOWER_OFF_PIN, on ? LOW : HIGH);
		}
		log_i("CAN-bus power %s", on ? "ON" : "OFF");
	}

	bool getBusPower()
	{
		return busPowerOn;
	}

	void setup()
	{
		log_d("Setup State");

		// CAN-bus power defaults to ON at boot (drives BUSPOWER_OFF_PIN LOW).
		// (The buzzer pin is owned by BuzzerController, which sets it up later.)
		if (pinConfig.BUSPOWER_OFF_PIN >= 0)
			setBusPower(true);
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
	int act(cJSON *doc)
	{
		// here you can do something
		if (isDEBUG)
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
					digitalWrite(pinConfig.BUZZER_PIN, HIGH);
				}
				else
				{
					digitalWrite(pinConfig.BUZZER_PIN, LOW);
				}
			}
		}
		// CAN-bus power control: {"task":"/state_act","power":0|1}
		cJSON *power = cJSON_GetObjectItemCaseSensitive(doc, "power");
		if (power != NULL)
		{
			setBusPower(power->valueint > 0);
		}

#ifdef DIGITAL_IN_CONTROLLER
		// Emergency-STOP polarity: {"task":"/state_act","estopPolarity":0|1}
		// 0 = asserted-LOW (idle HIGH), 1 = asserted-HIGH (idle LOW). Live + persisted.
		cJSON *estopPol = cJSON_GetObjectItemCaseSensitive(doc, "estopPolarity");
		if (estopPol != NULL)
		{
			DigitalInController::setEmergencyPolarity((int8_t)(estopPol->valueint ? 1 : 0));
		}
#endif

		cJSON *restart = cJSON_GetObjectItemCaseSensitive(doc, "restart");
		// assign default values to thhe variables
		if (restart != NULL)
		{ // {"task":"/state_act", "restart":1, "busrestart":1}
			// Cut CAN-bus power before rebooting so all slaves power-cycle with
			// the master; State::setup() restores power (pin LOW) after reboot.
			if (cJSON_GetObjectItemCaseSensitive(doc, "busrestart") != NULL && pinConfig.BUSPOWER_OFF_PIN >= 0)
			{
				log_i("Cutting CAN-bus power before restart");
				setBusPower(false);
				delay(200);
			}

			// {"task": "/state_act", "restart": 1}
			#ifdef I2C_MASTER
			i2c_master::reboot();
			#endif
			ESP.restart();
		}

		cJSON *ota = cJSON_GetObjectItemCaseSensitive(doc, "ota");
		if (ota != NULL)
		{
			// {"task": "/state_act", "ota": 1}
			startOTA();
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
			isDEBUG = debug->valueint;
			esp_log_level_set("MessageController", (esp_log_level_t)isDEBUG);
		}
		*/
		// assign default values to thhe variables
		cJSON *BUSY = cJSON_GetObjectItemCaseSensitive(doc, "isBusy");
		if (BUSY != NULL)
		{
			isBusy = BUSY->valueint;
		}
		cJSON *reset = cJSON_GetObjectItemCaseSensitive(doc, "resetPrefs");
		if (reset != NULL)
		{
			log_i("resetPreferences");
			Preferences preferences;
			const char *prefNamespace = "UC2";
			preferences.begin(prefNamespace, false);
			preferences.clear();
			preferences.end();
			ESP.restart();
			return true;
		}
		return 1;
	}

	cJSON *get(cJSON *docin)
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
		int qid = cJsonTool::getJsonInt(docin, "qid");
		cJSON *BUSY = cJSON_GetObjectItemCaseSensitive(docin, "isBusy");
		cJSON *HEAP = cJSON_GetObjectItemCaseSensitive(docin, "heap");
		cJSON *POWER = cJSON_GetObjectItemCaseSensitive(docin, "power");
		cJSON *ESTOP = cJSON_GetObjectItemCaseSensitive(docin, "estop");
		if (BUSY != NULL)
		{
			cJSON_AddItemToObject(st, "isBusy", cJSON_CreateNumber(((int)isBusy)));
		}
		else if (HEAP != NULL)
		{
			cJSON_AddItemToObject(st, "heap", cJSON_CreateNumber(ESP.getFreeHeap()));
		}
		else if (POWER != NULL)
		{
			// {"task":"/state_get","power":1} -> CAN-bus power state (1=ON)
			cJSON_AddItemToObject(st, "power", cJSON_CreateNumber((int)getBusPower()));
		}
		else if (ESTOP != NULL)
		{
#ifdef DIGITAL_IN_CONTROLLER
			// {"task":"/state_get","estop":1} -> E-stop diagnostics for tuning.
			// estopRaw is the live pin level; flip estopPolarity until pressing
			// the button makes estopActive read 1.
			cJSON_AddItemToObject(st, "estopPolarity", cJSON_CreateNumber(DigitalInController::getEmergencyPolarity()));
			cJSON_AddItemToObject(st, "estopRaw", cJSON_CreateNumber(DigitalInController::getEmergencyRaw()));
			cJSON_AddItemToObject(st, "estopActive", cJSON_CreateNumber(DigitalInController::isEmergencyActive() ? 1 : 0));
#endif
		}
		else
		{
			cJSON_AddItemToObject(st, "identifier_name", cJSON_CreateString(identifier_name));
			cJSON_AddItemToObject(st, "identifier_id", cJSON_CreateString(identifier_id));
			cJSON_AddItemToObject(st, "identifier_date", cJSON_CreateString(identifier_date));
			cJSON_AddItemToObject(st, "identifier_author", cJSON_CreateString(identifier_author));
			cJSON_AddItemToObject(st, "IDENTIFIER_NAME", cJSON_CreateString(IDENTIFIER_NAME));
			cJSON_AddItemToObject(st, "configIsSet", cJSON_CreateNumber(config_set));
			cJSON_AddItemToObject(st, "pindef", cJSON_CreateString(pinConfig.pindefName));
			cJSON_AddItemToObject(st, "I2C_SLAVE", cJSON_CreateNumber(pinConfig.I2C_CONTROLLER_TYPE));
			if (pinConfig.BUSPOWER_OFF_PIN >= 0)
				cJSON_AddItemToObject(st, "power", cJSON_CreateNumber((int)getBusPower()));
#ifdef DIGITAL_IN_CONTROLLER
			if (pinConfig.pinEmergencyExit >= 0)
			{
				cJSON_AddItemToObject(st, "estopPolarity", cJSON_CreateNumber(DigitalInController::getEmergencyPolarity()));
				cJSON_AddItemToObject(st, "estopRaw", cJSON_CreateNumber(DigitalInController::getEmergencyRaw()));
				cJSON_AddItemToObject(st, "estopActive", cJSON_CreateNumber(DigitalInController::isEmergencyActive() ? 1 : 0));
			}
#endif
			#ifdef CAN_CONTROLLER
			// Report the active node-id from runtimeConfig (may differ from
			// pinConfig.CAN_ID_CURRENT after LSS conflict resolution / NVS edits)
			cJSON_AddItemToObject(st, "CAN_SLAVE", cJSON_CreateNumber(runtimeConfig.canNodeId));
			cJSON_AddItemToObject(st, "CAN_SLAVE_DEFAULT", cJSON_CreateNumber(pinConfig.CAN_ID_CURRENT));
			#endif
			#ifdef CAN_CONTROLLER_CANOPEN
			cJSON_AddItemToObject(st, "CAN_SLAVE", cJSON_CreateNumber(runtimeConfig.canNodeId));
			cJSON_AddItemToObject(st, "CAN_SLAVE_DEFAULT", cJSON_CreateNumber(pinConfig.CAN_ID_CURRENT));
			#endif
			#ifdef GIT_COMMIT_HASH
				cJSON_AddItemToObject(st, "git_commit", cJSON_CreateString(GIT_COMMIT_HASH));
			#endif
			// cJSON_AddItemToObject(st, "heap", cJSON_CreateNumber(ESP.getFreeHeap()));
		}
		cJSON_AddItemToObject(doc, "qid", cJSON_CreateNumber(qid));

		return doc;
	}

	void setBusy(bool busy)
	{
		isBusy = busy;
	}

	bool getBusy()
	{
		return isBusy;
	}

	void printInfo()
	{
		if (isDEBUG)
			log_i("You can use this software by sending JSON strings, A full documentation can be found here:");
		if (isDEBUG)
			log_i("https://github.com/openUC2/UC2-REST/");
		// log_i("A first try can be: \{\"task\": \"/state_get\"");
	}

	void stopOTA()
	{
		ArduinoOTA.end();
		// turn off the wifi hotspot
		WiFi.softAPdisconnect(true);
		OTArunning = false;

	}

	void startOTA()
	{
		// Start a wifi hotspot using an SSID based on the ESPs MAC address with now password
		// and start an OTA server
		// This is a blocking function

		#if defined(I2C_MASTER)
		i2c_master::startOTA();
		#else

		// close any ongoing wifi connection
		WiFi.disconnect(true);

		// Generate SSID based on node ID or MAC address
		#ifdef CAN_BUS_ENABLED
		String ssid = "UC2-" + String(pinConfig.CAN_ID_CURRENT, HEX);
		ssid.toUpperCase();
		#else
		uint8_t mac[6];
		esp_read_mac(mac, ESP_MAC_WIFI_STA);
		String ssid = "ESP32-" + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
		ssid.toUpperCase();
		#endif

		// Start the Wi-Fi access point
		WiFi.softAP(ssid.c_str());
		IPAddress IP = WiFi.softAPIP();
		log_i("OTA server started on %s", IP.toString().c_str());

		// Set up mDNS responder for local hostname resolution
		if (!MDNS.begin("esp32"))
		{
			Serial.println("Error starting mDNS");
			return;
		}
		log_i("mDNS responder started");

		// Configure and start OTA server
		ArduinoOTA.onStart([]()
						   {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else
            type = "filesystem";
        Serial.println("Start updating " + type); });

		ArduinoOTA.onEnd([]()
						 { Serial.println("\nEnd"); });

		ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
							  { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });

		ArduinoOTA.onError([](ota_error_t error)
						   {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
            Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
            Serial.println("End Failed"); });

		ArduinoOTA.begin();
		log_i("OTA server started");

		// Blocking loop to handle OTA updates
		long cTime = millis();
		while (true)
		{
			ArduinoOTA.handle();
			delay(100);
			if (millis() - cTime > 30000)
			{
				log_i("Timeout, stopping OTA");
				break;
			}
		}
		log_i("No OTA for this device");
		#endif
		OTArunning = true;
	}

	cJSON *getModules()
	{
		cJSON *doc = cJSON_CreateObject();
		cJSON *mod = cJSON_CreateObject();
		cJSON_AddItemToObject(doc, key_modules, mod);
#ifdef LED_CONTROLLER
		cJSON_AddItemToObject(mod, keyLed, cJSON_CreateNumber(pinConfig.LED_PIN >= 0));
#endif
#ifdef MOTOR_CONTROLLER
		cJSON_AddItemToObject(mod, key_motor, cJSON_CreateNumber(pinConfig.MOTOR_ENABLE >= 0));
#endif
#ifdef GPIO_CONTROLLER
		cJSON_AddItemToObject(mod, key_gpio, cJSON_CreateNumber((pinConfig.GPIO_PIN_1 >= 0 || pinConfig.GPIO_PIN_2 >= 0 || pinConfig.GPIO_PIN_3 >= 0)));
#endif
#ifdef MESSAGE_CONTROLLER
		cJSON_AddItemToObject(mod, key_message, cJSON_CreateNumber(1));
#endif
#ifdef HOME_MOTOR
		cJSON_AddItemToObject(mod, key_home, cJSON_CreateNumber((pinConfig.DIGITAL_IN_1 >= 0 || pinConfig.DIGITAL_IN_2 >= 0 || pinConfig.DIGITAL_IN_3 >= 0)));
#endif
		cJSON_AddItemToObject(mod, key_analogin, cJSON_CreateNumber((pinConfig.analogin_PIN_0 >= 0 || pinConfig.analogin_PIN_1 >= 0 || pinConfig.analogin_PIN_2 >= 0 || pinConfig.analogin_PIN_3 >= 0)));
		cJSON_AddItemToObject(mod, key_pid, cJSON_CreateNumber((pinConfig.pid1 >= 0 || pinConfig.pid2 >= 0 || pinConfig.pid3 >= 0)));
		cJSON_AddItemToObject(mod, key_laser, cJSON_CreateNumber((pinConfig.LASER_1 >= 0 || pinConfig.LASER_2 >= 0 || pinConfig.LASER_3 >= 0)));
		cJSON_AddItemToObject(mod, key_dac, cJSON_CreateNumber((pinConfig.dac_fake_1 >= 0 || pinConfig.dac_fake_2 >= 0)));
		cJSON_AddItemToObject(mod, key_analogout, cJSON_CreateNumber((pinConfig.analogout_PIN_1 >= 0 || pinConfig.analogout_PIN_2 >= 0 || pinConfig.analogout_PIN_3 >= 0)));
		cJSON_AddItemToObject(mod, key_digitalout, cJSON_CreateNumber((pinConfig.DIGITAL_OUT_1 >= 0 || pinConfig.DIGITAL_OUT_2 >= 0 || pinConfig.DIGITAL_OUT_3 >= 0)));
		cJSON_AddItemToObject(mod, key_digitalin, cJSON_CreateNumber((pinConfig.DIGITAL_IN_1 >= 0 || pinConfig.DIGITAL_IN_2 >= 0 || pinConfig.DIGITAL_IN_3 >= 0)));
#ifdef SCANNER_CONTROLLER
		cJSON_AddItemToObject(mod, key_scanner, cJSON_CreateNumber(1));
#endif

		return doc;
	}
}

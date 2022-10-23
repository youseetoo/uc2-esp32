#include "ConfigController.h"

namespace Config
{

	Preferences preferences;

	const char *prefNamespace = "UC2";

	void setWifiConfig(WifiConfig *config)
	{
		preferences.begin(prefNamespace, false);
		preferences.putBytes(keyWifiSSID, config, sizeof(WifiConfig));
		preferences.end();
	}

	WifiConfig *getWifiConfig()
	{
		preferences.begin(prefNamespace, false);
		WifiConfig *conf = new WifiConfig();
		preferences.getBytes(keyWifiSSID, conf, sizeof(WifiConfig));
		preferences.end();
		return conf;
	}

	void setup()
	{
		// if we boot for the first time => reset the preferences! // TODO: Smart? If not, we may have the problem that a wrong pin will block bootup
		if (isFirstRun())
		{
			log_i("First Run, resetting config");
		}
		// check if setup went through after new config - avoid endless boot-loop
		// checkSetupCompleted();
	}

	void getMotorPins(MotorPins * pins[])
	{
		preferences.begin(prefNamespace, false);
		pins[Stepper::A] = new MotorPins();
		if (preferences.getBytesLength(keyMotorADirPin) > 0)
		{
			preferences.getBytes(keyMotorADirPin, pins[Stepper::A], sizeof(MotorPins));
			log_i("get stepper a");
		}
		pins[Stepper::X] = new MotorPins();
		if (preferences.getBytesLength(keyMotorXDirPin) > 0)
		{
			preferences.getBytes(keyMotorXDirPin, pins[Stepper::X], sizeof(MotorPins));
			log_i("get stepper x");
		}
		pins[Stepper::Y] = new MotorPins();
		if (preferences.getBytesLength(keyMotorYDirPin) > 0)
		{
			preferences.getBytes(keyMotorYDirPin, pins[Stepper::Y], sizeof(MotorPins));
			log_i("get stepper y");
		}
		pins[Stepper::Z] = new MotorPins();
		if (preferences.getBytesLength(keyMotorZDirPin) > 0)
		{
			preferences.getBytes(keyMotorZDirPin, pins[Stepper::Z], sizeof(MotorPins));
			log_i("get stepper z");
		}
		preferences.end();
	}

	void setMotorPinConfig(MotorPins * pins[])
	{
		preferences.begin(prefNamespace, false);
		preferences.putBytes(keyMotorADirPin, pins[Stepper::A], sizeof(MotorPins));
		preferences.putBytes(keyMotorXDirPin, pins[Stepper::X], sizeof(MotorPins));
		preferences.putBytes(keyMotorYDirPin, pins[Stepper::Y], sizeof(MotorPins));
		preferences.putBytes(keyMotorZDirPin, pins[Stepper::Z], sizeof(MotorPins));
		preferences.end();
	}


	LedConfig *getLedPins()
	{
		preferences.begin(prefNamespace, false);
		LedConfig *config = new LedConfig();
		preferences.getBytes(keyLed, config, sizeof(LedConfig));
		preferences.end();
		return config;
	}

	void setLedPins(LedConfig *config)
	{
		preferences.begin(prefNamespace, false);
		preferences.putBytes(keyLed, config, sizeof(LedConfig));
		preferences.end();
	}

	void setLaserPins(LaserPins pins)
	{
		preferences.begin(prefNamespace, false);
		preferences.putInt(keyLaser1Pin, pins.LASER_PIN_1);
		preferences.putInt(keyLaser2Pin, pins.LASER_PIN_2);
		preferences.putInt(keyLaser3Pin, pins.LASER_PIN_3);
		preferences.end();
	}
	void getLaserPins(LaserPins pins)
	{
		preferences.begin(prefNamespace, false);
		pins.LASER_PIN_1 = preferences.getInt(keyLaser1Pin, pins.LASER_PIN_1);
		pins.LASER_PIN_2 = preferences.getInt(keyLaser2Pin, pins.LASER_PIN_2);
		pins.LASER_PIN_3 = preferences.getInt(keyLaser3Pin, pins.LASER_PIN_3);
		preferences.end();
	}

	void setDacPins(DacPins pins)
	{
		preferences.begin(prefNamespace, false);
		preferences.putInt(keyDACfake1Pin, pins.dac_fake_1);
		preferences.putInt(keyDACfake2Pin, pins.dac_fake_2);
		preferences.end();
	}
	void getDacPins(DacPins pins)
	{
		preferences.begin(prefNamespace, false);
		pins.dac_fake_1 = preferences.getInt(keyDACfake1Pin, pins.dac_fake_1);
		pins.dac_fake_2 = preferences.getInt(keyDACfake2Pin, pins.dac_fake_2);
		preferences.end();
	}
	void setAnalogPins(AnalogPins pins)
	{
		preferences.begin(prefNamespace, false);
		preferences.putInt(keyAnalog1Pin, pins.analog_PIN_1);
		preferences.putInt(keyAnalog2Pin, pins.analog_PIN_2);
		preferences.putInt(keyAnalog3Pin, pins.analog_PIN_3);
		preferences.end();
	}
	void getAnalogPins(AnalogPins pins)
	{
		preferences.begin(prefNamespace, false);
		pins.analog_PIN_1 = preferences.getInt(keyAnalog1Pin, pins.analog_PIN_1);
		pins.analog_PIN_2 = preferences.getInt(keyAnalog2Pin, pins.analog_PIN_2);
		pins.analog_PIN_3 = preferences.getInt(keyAnalog3Pin, pins.analog_PIN_3);
		preferences.end();
	}
	void setDigitalPins(DigitalPins pins)
	{

		preferences.begin(prefNamespace, false);
		preferences.putInt(keyDigital1Pin, pins.digital_PIN_1);
		preferences.putInt(keyDigital2Pin, pins.digital_PIN_2);
		preferences.putInt(keyDigital3Pin, pins.digital_PIN_3);
		preferences.end();
	}
	void getDigitalPins(DigitalPins pins)
	{
		preferences.begin(prefNamespace, false);
		pins.digital_PIN_1 = preferences.getInt(keyDigital1Pin, pins.digital_PIN_1);
		pins.digital_PIN_2 = preferences.getInt(keyDigital2Pin, pins.digital_PIN_2);
		pins.digital_PIN_3 = preferences.getInt(keyDigital3Pin, pins.digital_PIN_3);
		preferences.end();
	}

	bool isFirstRun()
	{
		bool rdystate = preferences.begin(prefNamespace, false);
		log_i("isFirstRun Start preferences rdy %s", rdystate ? "true" : "false");
		// define preference name
		const char *compiled_date = __DATE__ " " __TIME__;
		String stored_date = preferences.getString(dateKey, ""); // FIXME

		log_i("Stored date: %s", stored_date.c_str());
		log_i("Compiled date: %s", compiled_date);

		log_i("First run? ");
		if (!stored_date.equals(compiled_date))
		{
			log_i("yes, resetSettings");
			resetPreferences();
			preferences.putString(dateKey, compiled_date); // FIXME?
		}
		else
		{
			log_i("no, loadSettings");
		}
		preferences.end();

		rdystate = preferences.begin(prefNamespace, false);
		log_i("datatest pref rdy %s", rdystate ? "true" : "false");
		String datetest = preferences.getString(dateKey, "");
		preferences.end();
		log_i("isFirstRun End datetest:%s", datetest.c_str());
		return !stored_date.equals(compiled_date);
	}

	bool resetPreferences()
	{
		log_i("resetPreferences");
		preferences.clear();
		return true;
	}

	void setModuleConfig(ModuleConfig *pins)
	{
		preferences.begin(prefNamespace, false);
		size_t s = preferences.putBytes("module", pins, sizeof(ModuleConfig));
		log_i("setModuleConfig size:%i", s);
		preferences.end();
	}
	ModuleConfig *getModuleConfig()
	{
		preferences.begin(prefNamespace, true);
		ModuleConfig *pin = new ModuleConfig();
		size_t s = preferences.getBytes("module", pin, sizeof(ModuleConfig));
		log_i("getModuleConfig size:%i", s);
		preferences.end();
		return pin;
	}
}

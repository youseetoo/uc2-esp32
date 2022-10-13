#include "ConfigController.h"

namespace Config
{

	Preferences preferences;

	const char *prefNamespace = "UC2";

	void setWifiConfig(WifiConfig config, bool prefopen)
	{
		bool open = prefopen;
		if (!prefopen)
			open = preferences.begin(prefNamespace, false);
		log_i("setWifiConfig ssid: %s, pw: %s, ap:%s prefopen:%s", config.mSSID, config.mPWD, boolToChar(config.mAP), boolToChar(open));
		preferences.putString(keyWifiSSID, config.mSSID);
		preferences.putString(keyWifiPW, config.mPWD);
		preferences.putInt(keyWifiAP, config.mAP);
		log_i("setWifiConfig pref ssid: %s pw:%s", preferences.getString(keyWifiSSID), preferences.getString(keyWifiPW));
		if (!prefopen)
			preferences.end();
	}

	void getWifiConfig(WifiConfig config)
	{
		config.mSSID = preferences.getString(keyWifiSSID);

		config.mPWD = preferences.getString(keyWifiPW);
		config.mAP = preferences.getInt(keyWifiAP);
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

	void getMotorPins(std::array<MotorPins, 4> pins)
	{

		preferences.begin(prefNamespace, false);
		pins[Stepper::X].STEP = preferences.getInt(keyMotorXStepPin);
		pins[Stepper::X].DIR = preferences.getInt(keyMotorXDirPin);
		pins[Stepper::X].ENABLE = preferences.getInt(keyMotorEnableX);
		pins[Stepper::X].enable_inverted = preferences.getInt(keyMotorEnableXinverted);
		pins[Stepper::X].step_inverted = preferences.getInt(keyMotorXStepPinInverted);
		pins[Stepper::X].direction_inverted = preferences.getInt(keyMotorXDirPinInverted);

		pins[Stepper::Y].STEP = preferences.getInt(keyMotorYStepPin);
		pins[Stepper::Y].DIR = preferences.getInt(keyMotorYDirPin);
		pins[Stepper::Y].ENABLE = preferences.getInt(keyMotorEnableY);
		pins[Stepper::Y].enable_inverted = preferences.getInt(keyMotorEnableYinverted);
		pins[Stepper::Y].step_inverted = preferences.getInt(keyMotorYStepPinInverted);
		pins[Stepper::Y].direction_inverted = preferences.getInt(keyMotorYDirPinInverted);

		pins[Stepper::Z].STEP = preferences.getInt(keyMotorZStepPin);
		pins[Stepper::Z].DIR = preferences.getInt(keyMotorZDirPin);
		pins[Stepper::Z].ENABLE = preferences.getInt(keyMotorEnableZ);
		pins[Stepper::Z].enable_inverted = preferences.getInt(keyMotorEnableZinverted);
		pins[Stepper::Z].step_inverted = preferences.getInt(keyMotorZStepPinInverted);
		pins[Stepper::Z].direction_inverted = preferences.getInt(keyMotorZDirPinInverted);

		pins[Stepper::A].STEP = preferences.getInt(keyMotorAStepPin);
		pins[Stepper::A].DIR = preferences.getInt(keyMotorADirPin);
		pins[Stepper::A].ENABLE = preferences.getInt(keyMotorEnableA);
		pins[Stepper::A].enable_inverted = preferences.getInt(keyMotorEnableAinverted);
		pins[Stepper::A].step_inverted = preferences.getInt(keyMotorAStepPinInverted);
		pins[Stepper::A].direction_inverted = preferences.getInt(keyMotorADirPinInverted);
		preferences.end();
	}

	void setMotorPinConfig(bool prefsOpen, std::array<MotorPins, 4> pins)
	{
		if (!prefsOpen)
			preferences.begin(prefNamespace, false);
		preferences.putInt(keyMotorAStepPin, pins[Stepper::A].STEP);
		preferences.putInt(keyMotorXStepPin, pins[Stepper::X].STEP);
		preferences.putInt(keyMotorYStepPin, pins[Stepper::Y].STEP);
		preferences.putInt(keyMotorZStepPin, pins[Stepper::Z].STEP);

		preferences.putInt(keyMotorADirPin, pins[Stepper::A].DIR);
		preferences.putInt(keyMotorXDirPin, pins[Stepper::X].DIR);
		preferences.putInt(keyMotorYDirPin, pins[Stepper::Y].DIR);
		preferences.putInt(keyMotorZDirPin, pins[Stepper::Z].DIR);

		preferences.putInt(keyMotorEnableA, pins[Stepper::A].ENABLE);
		preferences.putInt(keyMotorEnableX, pins[Stepper::X].ENABLE);
		preferences.putInt(keyMotorEnableY, pins[Stepper::Y].ENABLE);
		preferences.putInt(keyMotorEnableY, pins[Stepper::Z].ENABLE);

		preferences.putInt(keyMotorADirPinInverted, pins[Stepper::A].direction_inverted);
		preferences.putInt(keyMotorAStepPinInverted, pins[Stepper::A].step_inverted);
		preferences.putInt(keyMotorEnableAinverted, pins[Stepper::A].enable_inverted);

		preferences.putInt(keyMotorXDirPinInverted, pins[Stepper::X].direction_inverted);
		preferences.putInt(keyMotorXStepPinInverted, pins[Stepper::X].step_inverted);
		preferences.putInt(keyMotorEnableXinverted, pins[Stepper::X].enable_inverted);

		preferences.putInt(keyMotorYDirPinInverted, pins[Stepper::Y].direction_inverted);
		preferences.putInt(keyMotorYStepPinInverted, pins[Stepper::Y].step_inverted);
		preferences.putInt(keyMotorEnableYinverted, pins[Stepper::Y].enable_inverted);

		preferences.putInt(keyMotorZDirPinInverted, pins[Stepper::Z].direction_inverted);
		preferences.putInt(keyMotorZStepPinInverted, pins[Stepper::Z].step_inverted);
		preferences.putInt(keyMotorEnableZinverted, pins[Stepper::Z].enable_inverted);

		log_i("Stepper A step:%i dir:%i enablepin:%i", preferences.getInt(keyMotorAStepPin), preferences.getInt(keyMotorADirPin), preferences.getInt(keyMotorEnableA));
		log_i("Stepper X step:%i dir:%i enablepin:%i", preferences.getInt(keyMotorXStepPin), preferences.getInt(keyMotorXDirPin), preferences.getInt(keyMotorEnableX));
		log_i("Stepper Y step:%i dir:%i enablepin:%i", preferences.getInt(keyMotorYStepPin), preferences.getInt(keyMotorYDirPin), preferences.getInt(keyMotorEnableY));
		log_i("Stepper Z step:%i dir:%i enablepin:%i", preferences.getInt(keyMotorZStepPin), preferences.getInt(keyMotorZDirPin), preferences.getInt(keyMotorEnableZ));
		if (!prefsOpen)
			preferences.end();
	}

	void getLedPins(LedConfig config)
	{
		preferences.begin(prefNamespace, false);
		config.ledCount = preferences.getInt(keyLEDCount, 0);
		config.ledPin = preferences.getInt(keyLEDPin, 0);
		preferences.end();
	}

	void setLedPins(bool openPrefs, LedConfig config)
	{
		if (!openPrefs)
			preferences.begin(prefNamespace, false);
		preferences.putInt(keyLEDCount, config.ledCount);
		preferences.putInt(keyLEDPin, config.ledPin);
		log_i("pin:%i count:%i", preferences.getInt(keyLEDPin), preferences.getInt(keyLEDCount));
		if (!openPrefs)
			preferences.end();
	}

	void setLaserPins(bool openPrefs, LaserPins pins)
	{
		if (!openPrefs)
			preferences.begin(prefNamespace, false);
		preferences.putInt(keyLaser1Pin, pins.LASER_PIN_1);
		preferences.putInt(keyLaser2Pin, pins.LASER_PIN_2);
		preferences.putInt(keyLaser3Pin, pins.LASER_PIN_3);
		if (!openPrefs)
			preferences.end();
	}
	void getLaserPins(LaserPins pins)
	{

		pins.LASER_PIN_1 = preferences.getInt(keyLaser1Pin, pins.LASER_PIN_1);
		pins.LASER_PIN_2 = preferences.getInt(keyLaser2Pin, pins.LASER_PIN_2);
		pins.LASER_PIN_3 = preferences.getInt(keyLaser3Pin, pins.LASER_PIN_3);
	}

	void setDacPins(bool openPrefs, DacPins pins)
	{
		if (!openPrefs)
			preferences.begin(prefNamespace, false);
		preferences.putInt(keyDACfake1Pin, pins.dac_fake_1);
		preferences.putInt(keyDACfake2Pin, pins.dac_fake_2);
		if (!openPrefs)
			preferences.end();
	}
	void getDacPins(DacPins pins)
	{
		pins.dac_fake_1 = preferences.getInt(keyDACfake1Pin, pins.dac_fake_1);
		pins.dac_fake_2 = preferences.getInt(keyDACfake2Pin, pins.dac_fake_2);
	}
	void setAnalogPins(bool openPrefs, AnalogPins pins)
	{
		if (!openPrefs)
			preferences.begin(prefNamespace, false);
		preferences.putInt(keyAnalog1Pin, pins.analog_PIN_1);
		preferences.putInt(keyAnalog2Pin, pins.analog_PIN_2);
		preferences.putInt(keyAnalog3Pin, pins.analog_PIN_3);
		if (!openPrefs)
			preferences.end();
	}
	void getAnalogPins(AnalogPins pins)
	{
		pins.analog_PIN_1 = preferences.getInt(keyAnalog1Pin, pins.analog_PIN_1);
		pins.analog_PIN_2 = preferences.getInt(keyAnalog2Pin, pins.analog_PIN_2);
		pins.analog_PIN_3 = preferences.getInt(keyAnalog3Pin, pins.analog_PIN_3);
	}
	void setDigitalPins(bool openPrefs, DigitalPins pins)
	{
		if (!openPrefs)
			preferences.begin(prefNamespace, false);
		preferences.putInt(keyDigital1Pin, pins.digital_PIN_1);
		preferences.putInt(keyDigital2Pin, pins.digital_PIN_2);
		preferences.putInt(keyDigital3Pin, pins.digital_PIN_3);
		if (!openPrefs)
			preferences.end();
	}
	void getDigitalPins(DigitalPins pins)
	{
		pins.digital_PIN_1 = preferences.getInt(keyDigital1Pin, pins.digital_PIN_1);
		pins.digital_PIN_2 = preferences.getInt(keyDigital2Pin, pins.digital_PIN_2);
		pins.digital_PIN_3 = preferences.getInt(keyDigital3Pin, pins.digital_PIN_3);
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
}

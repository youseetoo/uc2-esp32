#include "config.h"
#include "ArduinoJson.h"
#include "esp_log.h"
#ifdef IS_ANALOG
#include "src/analog/AnalogController.h"
#endif
#include "src/state/State.h"
#ifdef IS_SCANNER
#include "src/scanner/ScannerController.h"
#endif
#ifdef IS_DIGITAL
#include "src/digital/DigitalController.h"
#endif
#if defined IS_DAC || defined IS_DAC_FAKE
#include "src/dac/DacController.h"
#endif
#if defined IS_PS4 || defined IS_PS3
#include "src/gamepads/ps_3_4_controller.h"
#endif
#include "src/wifi/WifiController.h"
#include "src/config/ConfigController.h"
#ifdef IS_SERIAL
#include "src/serial/SerialProcess.h"
#endif
#include "src/bt/BtController.h"
#include "ModuleController.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

PINDEF *pins;

void setup()
{
	// Start Serial
	Serial.begin(BAUDRATE);
	delay(500);
	log_i("Start setup");
	WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector

	// for any timing related puposes..
	state.startMillis = millis();
	pins = new PINDEF();
	log_i("getDefaultPins");
	state.getDefaultPinDef(pins);
	log_i("wifi.createJsonDoc()");
	WifiController::createJsonDoc();

	log_i("state.setup");
	state.setup(pins);
	state.printInfo();

	log_i("Config::setup");
	Config::setup(pins);

	WifiController::getJDoc()->clear();
	
	// connect to wifi if necessary
	log_i("wifi.setup");
	WifiController::setup();

	moduleController.setup();

	BtController::setup();

#if defined IS_PS4 || defined IS_PS3
	/*log_i("PsController Config::isFirstRun");
	if (Config::isFirstRun())
	{
	  log_i("clear bt devices and restart");
	  state.clearBlueetoothDevice();
	  ESP.restart();
	}*/
#ifdef DEBUG_GAMEPAD
	ps_c.DEBUG = true;
#endif
	//ps_c.start();
#endif

#if defined IS_DAC || defined IS_DAC_FAKE
#if defined IS_DAC
	log_i("IS_DAC");
#endif
#if defined IS_DAC_FAKE
	log_i("IS_DAC_FAKE");
#endif
	dac.setup(pins);
#endif

#ifdef IS_ANALOG
	log_i("IS_ANALOG");
#ifdef DEBUG_ANALOG
	analog->DEBUG = true;
#endif
	analog.setup(pins);
#endif

#ifdef IS_DIGITAL
	digital.setup(pins);
#endif

#ifdef IS_SCANNER
	log_i("IS_SCANNER");
	scanner.setup(pins);
#endif

	log_i("End setup");
}

void loop()
{
	// for any timing-related purposes
	state.currentMillis = millis();
#ifdef IS_SERIAL
	serial.loop(WifiController::getJDoc());
#endif
#ifdef IS_WIFI
	WifiController::handelMessages();
#endif

	moduleController.loop();

#if defined IS_PS4 || defined IS_PS3
	ps_c.control(); // if controller is operating motors, overheating protection is enabled
#endif
}

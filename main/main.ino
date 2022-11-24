#include "config.h"
#include "ArduinoJson.h"
#include "esp_log.h"


#if defined IS_PS4 || defined IS_PS3
#include "src/gamepads/ps_3_4_controller.h"
#endif
#include "src/wifi/WifiController.h"
#include "src/config/ConfigController.h"
#include "src/serial/SerialProcess.h"
#include "src/bt/BtController.h"
#include "ModuleController.h"

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"


void setup()
{
	// Start Serial
	Serial.begin(BAUDRATE);
	delay(500);
	log_i("Start setup");
	WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector

	// for any timing related puposes..

	log_i("Config::setup");
	Config::setup();
	
	// connect to wifi if necessary
	log_i("wifi.setup");
	WifiController::setup();

	moduleController.setup();

	BtController::setup();

#if defined IS_PS4 || defined IS_PS3
#ifdef DEBUG_GAMEPAD
	ps_c.DEBUG = true;
#endif
	//ps_c.start();
#endif
	WifiController::begin();
	log_i("End setup");
}

void loop()
{
	// for any timing-related purposes
	serial.loop();
	WifiController::handelMessages();
	moduleController.loop();

#if defined IS_PS4 || defined IS_PS3
	ps_c.control(); // if controller is operating motors, overheating protection is enabled
#endif
}

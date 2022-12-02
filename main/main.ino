#include "ArduinoJson.h"
#include "esp_log.h"
#include "src/wifi/WifiController.h"
#include "src/config/ConfigController.h"
#include "src/serial/SerialProcess.h"
#include "src/bt/BtController.h"
#include "ModuleController.h"


#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define BAUDRATE 115200

void setup()
{
	// Start Serial
	Serial.begin(BAUDRATE); // default is 115200
	delay(500);

	// Disable brownout detector
	log_i("Start setup");
	WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 

	// initialize the pin/settings configurator
	log_i("Config::setup");
	Config::setup();

	// connect to wifi if necessary
	log_i("wifi.setup");
	WifiController::setup();

	// initialize the module controller
	moduleController.setup();

	// initialize the bluetooth controller
	BtController::setup();

	// start with the wifi (either AP or connecting to wifi)
	WifiController::begin();
	log_i("End setup");

	// check if boot process went through
	Config::checkifBootWentThrough();
}

void loop()
{
	// for any timing-related purposes
	serial.loop();
	WifiController::handelMessages();
	BtController::loop();
	moduleController.loop();
}

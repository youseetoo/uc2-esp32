#include "ArduinoJson.h"
#include "esp_log.h"
#include "src/wifi/WifiController.h"
#include "src/config/ConfigController.h"
#include "src/serial/SerialProcess.h"
#include "src/bt/BtController.h"
#include "ModuleController.h"


#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

int BAUDRATE = 115200;

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

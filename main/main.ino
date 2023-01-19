#include "ArduinoJson.h"
#include "esp_log.h"
#include "src/config/ConfigController.h"
#include "src/serial/SerialProcess.h"
#include "ModuleController.h"


#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define BAUDRATE 115200

void setup()
{
	// Start Serial
	Serial.begin(BAUDRATE); // default is 115200
	delay(500);
	Serial.setTimeout(50);

	// Disable brownout detector
	log_i("Start setup");
	WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 

	// initialize the pin/settings configurator
	log_i("Config::setup");
	Config::setup();

	// initialize the module controller
	moduleController.setup();

	log_i("End setup");
}

void loop()
{
	// receive and process serial messages
	serial.loop();
	// process all commands in their modules
	moduleController.loop();
}

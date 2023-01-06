#include "ArduinoJson.h"
#include "esp_log.h"
#include "src/wifi/WifiController.h"
#include "src/config/ConfigController.h"
#include "src/serial/SerialProcess.h"
#include "src/bt/BtController.h"
#include "ModuleController.h"
#include "src/motor/FocusMotor.h"


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

long tProcessServer = 100; // process server every 100ms
long oldTime = 0;

void loop()
{

	// receive and process serial messages
	serial.loop();

	// process the server every 100ms
	// 	if (true){//motor->motorsBusy() or millis()-oldTime > tProcessServer) {
	if (moduleController.get(AvailableModules::wifi) != nullptr and moduleController.get(AvailableModules::wifi)){
		oldTime = millis();
		WifiController::handelMessages();
	}
	
	// handle PS-controller inputs
	BtController::loop();

	// process all commands in their modules
	moduleController.loop();
	unsigned long end = micros();
	//log_i("loop took %i ms", end - now);
}

#include "esp_log.h"
#include "src/config/ConfigController.h"
#include "src/serial/SerialProcess.h"
#include <PinConfig.h>

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#ifdef ANALOG_IN_CONTROLLER
#include "src/analogin/AnalogInController.h"
#endif
#ifdef ANALOG_JOYSTICK
#include "src/analogin/AnalogJoyStick.h"
#endif
#ifdef ANALOG_OUT_CONTROLLER
#include "src/analogout/AnalogOutController.h"
#endif
#ifdef BLUETOOTH
#include "src/bt/BtController.h"
#endif
#ifdef DAC_CONTROLLER
#include "src/dac/DacController.h"
#endif
#ifdef DIGITAL_IN_CONTROLLER
#include "src/digitalin/DigitalInController.h"
#endif
#ifdef DIGITAL_OUT_CONTROLLER
#include "src/digitalout/DigitalOutController.h"
#endif
#ifdef ENCODER_CONTROLLER
#include "src/encoder/EncoderController.h"
#endif
#ifdef LASER_CONTROLLER
#include "src/laser/LaserController.h"
#endif
#ifdef LED_CONTROLLER
#include "src/led/LedController.h"
#endif
#ifdef FOCUS_MOTOR
#include "src/motor/FocusMotor.h"
#endif
#ifdef PID_CONTROLLER
#include "src/pid/PidController.h"
#endif
#ifdef SCANNER_CONTROLLER
#include "src/scanner/ScannerController.h"
#endif
#ifdef HOME_MOTOR
#include "src/home/HomeMotor.h"
#endif
#ifdef WIFI
#include "src/wifi/WifiController.h"
#endif
#ifdef USE_TCA9535
#include "src/i2c/tca_controller.h"
#endif


#define BAUDRATE 115200
long lastHeapUpdateTime;

extern "C" void looper(void *p)
{
	for (;;)
	{
		// receive and process serial messages
		serial.loop();
#ifdef ENCODER_CONTROLLER
		EncoderController::loop();
		vTaskDelay(1);
#endif
#ifdef HOME_MOTOR
		HomeMotor::loop();
		vTaskDelay(1);
#endif
#ifdef LASER_CONTROLLER
		LaserController::loop();
		vTaskDelay(1);
#endif
#ifdef FOCUS_MOTOR
		FocusMotor::loop();
		vTaskDelay(1);
#endif
#ifdef PID_CONTROLLER
		PidController::loop();
		vTaskDelay(1);
#endif
#ifdef SCANNER_CONTROLLER
		ScannerController::loop();
		vTaskDelay(1);
#endif
		

		// process all commands in their modules

		if (pinConfig.dumpHeap && lastHeapUpdateTime + 1000000 < esp_timer_get_time())
		{
			/* code */
			log_i("free heap:%d", ESP.getFreeHeap());
			lastHeapUpdateTime = esp_timer_get_time();
		}
	}
	vTaskDelete(NULL);
}

extern "C" void setupApp(void)
{

#ifdef USE_TCA9535
	tca_controller::init_tca();
#endif
#ifdef FOCUS_MOTOR
	FocusMotor::setup();
#endif
#ifdef ANALOG_IN_CONTROLLER
	AnalogInController::setup();
#endif
#ifdef ANALOG_JOYSTICK
	AnalogJoystick::setup();
#endif
#ifdef ANALOG_OUT_CONTROLLER
	AnalogOutController::setup();
#endif
#ifdef BLUETOOTH
	BtController::setup();
#endif
#ifdef DAC_CONTROLLER
	DacController::setup();
#endif
#ifdef DIGITAL_IN_CONTROLLER
	DigitalInController::setup();
#endif
#ifdef DIGITAL_OUT_CONTROLLER
	DigitalOutController::setup();
#endif
#ifdef ENCODER_CONTROLLER
	EncoderController::setup();
#endif
#ifdef HOME_MOTOR
	HomeMotor::setup();
#endif
#ifdef LASER_CONTROLLER
	LaserController::setup();
#endif
#ifdef LED_CONTROLLER
	LedController::setup();
#endif

#ifdef PID_CONTROLLER
	PidController::setup();
#endif
#ifdef SCANNER_CONTROLLER
	ScannerController::setup();
#endif
#ifdef WIFI
	WifiController::setup();
#endif
}

extern "C" void app_main(void)
{
	// Start Serial
	Serial.begin(BAUDRATE); // default is 115200
	// delay(500);
	Serial.setTimeout(50);

	// Disable brownout detector
	log_i("Start setup");
	WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

	// initialize the pin/settings configurator
	log_i("Config::setup");
	Config::setup();

	// initialize the module controller
	setupApp();

	log_i("End setup");
	xTaskCreatePinnedToCore(&looper, "loop", pinConfig.MAIN_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, NULL, 1);
	// xTaskCreate(&looper, "loop", 8128, NULL, 5, NULL);
}

#include "esp_log.h"
#include <PinConfig.h>
#include "src/config/ConfigController.h"
#include "src/serial/SerialProcess.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_task_wdt.h"
#include "Wire.h"


#define WDTIMEOUT 2 // ensure that the watchdog timer is reset every 2 seconds, otherwise the ESP32 will reset

// TODO: Just for testing
#ifdef ESP32S3_MODEL_XIAO
#define GPIO_NUM_22 GPIO_NUM_17
#define GPIO_NUM_23 GPIO_NUM_17
#endif

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
#ifdef DIAL_CONTROLLER
#include "src/dial/DialController.h"
#endif
#ifdef DIGITAL_OUT_CONTROLLER
#include "src/digitalout/DigitalOutController.h"
#endif
#ifdef ENCODER_CONTROLLER
#include "src/encoder/EncoderController.h"
#endif
#ifdef LINEAR_ENCODER_CONTROLLER
#include "src/encoder/LinearEncoderController.h"
#endif
#ifdef LASER_CONTROLLER
#include "src/laser/LaserController.h"
#endif
#ifdef LED_CONTROLLER
#include "src/led/LedController.h"
#endif
#ifdef MESSAGE_CONTROLLER
#include "src/message/MessageController.h"
#endif
#ifdef MOTOR_CONTROLLER
#include "src/motor/FocusMotor.h"
#endif
#ifdef PID_CONTROLLER
#include "src/pid/PidController.h"
#endif
#ifdef SCANNER_CONTROLLER
#include "src/scanner/ScannerController.h"
#endif
#ifdef GALVO_CONTROLLER
#include "src/scanner/GalvoController.h"
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
#ifdef USE_I2C
#include "src/i2c/i2c_controller.h"
#endif
#ifdef HEAT_CONTROLLER
#include "src/heat/DS18b20Controller.h"
#include "src/heat/HeatController.h"
#endif

long lastHeapUpdateTime = 0;

extern "C" void looper(void *p)
{
	log_i("Starting loop");
	// Enable the watchdog timer to detect and recover from system crashes
	esp_task_wdt_init(WDTIMEOUT, true); // Enable panic so ESP32 restarts
	esp_task_wdt_add(NULL);				// Add current thread to WDT watch

	for (;;)
	{
		esp_task_wdt_reset(); // Reset (feed) the watchdog timer
		// receive and process serial messages
		SerialProcess::loop();
#ifdef ENCODER_CONTROLLER
		EncoderController::loop();
		vTaskDelay(1);
#endif

#ifdef LINEAR_ENCODER_CONTROLLER
		LinearEncoderController::loop();
		vTaskDelay(1);
#endif
#ifdef HOME_MOTOR
		HomeMotor::loop();
		vTaskDelay(1);
#endif
#ifdef DIGITAL_IN_CONTROLLER
		DigitalInController::loop();
		vTaskDelay(1);
#endif
#ifdef DIAL_CONTROLLER
		DialController::loop();
		vTaskDelay(1);
#endif
#ifdef LASER_CONTROLLER
		LaserController::loop();
		vTaskDelay(1);
#endif
#ifdef MOTOR_CONTROLLER
		FocusMotor::loop();
		vTaskDelay(1);
#endif
#ifdef USE_I2C
		i2c_controller::loop();
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
#ifdef GALVO_CONTROLLER
		GalvoController::loop();
		vTaskDelay(1);
#endif
#ifdef HEAT_CONTROLLER
		HeatController::loop();
		vTaskDelay(1);
#endif

		// process all commands in their modules
		if (pinConfig.dumpHeap && lastHeapUpdateTime + 500000 < esp_timer_get_time())
		{ //
			/* code */
			Serial.print("free heap:");
			Serial.println(ESP.getFreeHeap());
			lastHeapUpdateTime = esp_timer_get_time();
		}
		// Allow other tasks to run and reset the WDT
		vTaskDelay(pdMS_TO_TICKS(1));
	}
	vTaskDelete(NULL);
}

extern "C" void setupApp(void)
{

	log_i("SetupApp");
	// setup debugging level
	// esp_log_level_set("*", ESP_LOG_DEBUG);
	esp_log_level_set("*", ESP_LOG_NONE);
	SerialProcess::setup();
#ifdef USE_I2C
	i2c_controller::setup();
#endif
#ifdef USE_TCA9535
	tca_controller::init_tca();
#endif
#ifdef DIAL_CONTROLLER
	DialController::setup();
#endif
#ifdef MOTOR_CONTROLLER
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
#ifdef LINEAR_ENCODER_CONTROLLER
	LinearEncoderController::setup();
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
#ifdef MESSAGE_CONTROLLER
	MessageController::setup();
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
#ifdef HEAT_CONTROLLER
	DS18b20Controller::setup();
	HeatController::setup();
#endif
#ifdef GALVO_CONTROLLER
	GalvoController::setup();
#endif
}
extern "C" void app_main(void)
{
	// Setzt das Log-Level für alle Tags auf WARNING, um INFO-Nachrichten zu unterdrücken
	// esp_log_level_set("*", ESP_LOG_WARN);

	// Oder, wenn der Tag bekannt ist, z.B. "gpio", nur für diesen Tag setzen
	// esp_log_level_set("gpio", ESP_LOG_WARN);

	// Start Serial
	
	Serial.begin(pinConfig.BAUDRATE); // default is 115200
	// delay(500);
	Serial.setTimeout(50);

	// Start Serial 2
	/*
	Serial2.begin(BAUDRATE, SERIAL_8N1, pinConfig.SERIAL2_RX, pinConfig.SERIAL2_TX);
	Serial2.setTimeout(50);
	Serial2.println("Serial2 started");
	*/
	// Disable brownout detector
	log_i("Start setup");
	WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

	// initialize the pin/settings configurator
	log_i("Config::setup");
	Config::setup();

	// initialize the module controller
	setupApp();

	Serial.println("{'setup':'done'}");

	xTaskCreatePinnedToCore(&looper, "loop", pinConfig.MAIN_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, NULL, 1);
	// xTaskCreate(&looper, "loop", 8128, NULL, 5, NULL);
}

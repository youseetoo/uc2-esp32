#define CORE_isDEBUG_LEVEL 0
#include "esp_log.h"
#include "PinConfig.h"
#include "src/config/ConfigController.h"
#include "src/serial/SerialProcess.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include "Wire.h"
#include <Preferences.h>
#include "nvs_flash.h"
#include "hal/usb_hal.h"

Preferences preferences;

#define WDTIMEOUT 5 // ensure that the watchdog timer is reset every 2 seconds, otherwise the ESP32 will reset

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
#ifdef TMC_CONTROLLER
#include "src/tmc/TMCController.h"
#endif
#ifdef LED_CONTROLLER
#include "src/led/LedController.h"
#endif
#ifdef MESSAGE_CONTROLLER
#include "src/message/MessageController.h"
#endif
#ifdef MOTOR_CONTROLLER
#include "src/motor/FocusMotor.h"
#include "src/motor/MotorGamePad.h"
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
#ifdef OBJECTIVE_CONTROLLER
#include "src/objective/ObjectiveController.h"
#endif
#ifdef WIFI
#include "src/wifi/WifiController.h"
#endif
#ifdef USE_TCA9535
#include "src/i2c/tca_controller.h"
#endif
#ifdef CAN_CONTROLLER
#include "src/can/can_controller.h"
#endif
#ifdef I2C_MASTER
#include "src/i2c/i2c_master.h"
#endif
#ifdef I2C_SLAVE_MOTOR
#include "src/i2c/i2c_slave_motor.h"
#endif
#ifdef I2C_SLAVE_LASER
#include "src/i2c/i2c_slave_laser.h"
#endif
#ifdef I2C_SLAVE_DIAL
#include "src/i2c/i2c_slave_dial.h"
#endif
#ifdef HEAT_CONTROLLER
#include "src/heat/DS18b20Controller.h"
#include "src/heat/HeatController.h"
#endif
#ifdef ESPNOW_SLAVE_MOTOR
#include "src/espnow/espnow_slave_motor.h"
#endif
#include "./src/state/State.h"

long lastHeapUpdateTime = 0;
bool state_led = false;
long time_led = 0;
long period_led = 1000;
extern "C" void looper(void *p)
{
	log_i("Starting loop");
	// Enable the watchdog timer to detect and recover from system crashes
	esp_task_wdt_init(WDTIMEOUT, true); // Enable panic so ESP32 restarts
	esp_task_wdt_add(NULL);				// Add current thread to WDT watch

	for (;;)
	{

		// measure time since boot and turn off OTA on startup 
		#ifdef OTA_ON_STARTUP
		if (esp_timer_get_time()>pinConfig.OTA_TIME_FROM_STARTUP and State::OTArunning)
		{
			State::stopOTA();
		}
		#endif

		esp_task_wdt_reset(); // Reset (feed) the watchdog timer
		// receive and process serial messages
		#ifdef ESP32S3_MODEL_XIAO
		// heartbeat for the LED
		if (time_led + period_led < esp_timer_get_time())
		{
			time_led = esp_timer_get_time();
			digitalWrite(LED_BUILTIN, state_led);
			state_led = !state_led;
		}
		#endif
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
#ifdef OBJECTIVE_CONTROLLER
		ObjectiveController::loop();
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
#ifdef TMC_CONTROLLER
		TMCController::loop();
		vTaskDelay(1);
#endif
#ifdef LASER_CONTROLLER
		LaserController::loop();
		vTaskDelay(1);
#endif
#ifdef LED_CONTROLLER
		LedController::loop();
		vTaskDelay(1);
#endif
#ifdef MOTOR_CONTROLLER
		FocusMotor::loop();
		vTaskDelay(1);
#endif
#ifdef I2C_MASTER
		i2c_master::loop();
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
	// esp_log_level_set("*", ESP_LOG_isDEBUG);
#ifdef DESP32S3_MODEL_XIAO
  pinMode(LED_BUILTIN, OUTPUT);
#endif	
	SerialProcess::setup();
#ifdef DIAL_CONTROLLER
	// need to initialize the dial controller before the i2c controller
	DialController::setup();
#endif
#ifdef I2C_MASTER
	i2c_master::setup();
#endif
#ifdef CAN_CONTROLLER
	can_controller::setup();
#endif
#ifdef I2C_SLAVE_MOTOR
	i2c_slave_motor::setup();
#endif
#ifdef I2C_SLAVE_LASER
	i2c_slave_laser::setup();
#endif
#ifdef I2C_SLAVE_DIAL
	i2c_slave_dial::setup();
#endif
#ifdef USE_TCA9535
	tca_controller::init_tca();
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
	log_i("BtController setup");
	BtController::setup();
	#ifdef LED_CONTROLLER
		BtController::setCircleChangedEvent(LedController::circle_changed_event);
		BtController::setCrossChangedEvent(LedController::cross_changed_event);
	#endif
	#ifdef MESSAGE_CONTROLLER
		BtController::setTriangleChangedEvent(MessageController::triangle_changed_event);
		BtController::setSquareChangedEvent(MessageController ::square_changed_event);
	#endif
	#ifdef LASER_CONTROLLER
		BtController::setDpadChangedEvent(LaserController::dpad_changed_event);
	#endif
	#ifdef MOTOR_CONTROLLER
		BtController::setXYZAChangedEvent(MotorGamePad::xyza_changed_event);
		//log_i("BtController xyza_changed_event nullptr %d", BtController::xyza_changed_event == nullptr);
	#endif
	#ifdef ANALOG_OUT_CONTROLLER
		BtController::setAnalogControllerChangedEvent(AnalogOutController::btcontroller_event);
	#endif
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
#ifdef OBJECTIVE_CONTROLLER
	ObjectiveController::setup();
#endif
#ifdef LASER_CONTROLLER
	LaserController::setup();
#endif
#ifdef TMC_CONTROLLER
	TMCController::setup();
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
#ifdef ESPNOW_MASTER
	espnow_master::setup();
#endif
#ifdef ESPNOW_SLAVE_MOTOR
	espnow_slave_motor::setup();
#endif

#ifdef OTA_ON_STARTUP
State::startOTA();
#endif

	Serial.println("{'setup':'done'}");
}

extern "C" void app_main(void)
{
	// Disable brownout detector
	// WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
	// esp_log_level_set("*", ESP_LOG_NONE);
	log_i("Start setup");

	// Initialisieren Sie den NVS-Speicher
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		// NVS-Partition ist beschädigt oder eine neue Version wurde gefunden
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	// read if boot went well from preferences // TODO: Some ESPs have this problem apparently... not sure why
	preferences.begin("boot_prefs", false);
	bool hasBooted = preferences.getBool("hasBooted", false); // Check if the ESP32 has already booted successfully before

	// If this is the first boot, set the flag and restart
	if (false and !hasBooted)
	{ // some ESPs are freaking out on start, but this is not a good solution
		// Set the flag to indicate that the ESP32 has booted once
		Serial.println("First boot");
		preferences.putBool("hasBooted", true);
		preferences.end();
		ESP.restart(); // Restart the ESP32 immediately
	}

	preferences.putBool("hasBooted", false); // reset boot flag so that the ESP32 will restart on the next boot
	preferences.end();

	// Start Serial
	Serial.begin(pinConfig.BAUDRATE); // default is 115200
	// delay(500);
	Serial.setTimeout(pinConfig.serialTimeout);
	#ifdef ESP32S3_MODEL_XIAO
	
	
	// Weitere USB-CDC-Konfiguration
	Serial.setTxTimeoutMs(0);
	Serial.setRxBufferSize(2048);
	Serial.setTxBufferSize(2048);

	delay(500); // Kurze Pause für USB-Initialisierung
	#endif

	// initialize the pin/settings configurator
	log_i("Config::setup");
	Config::setup();

	// initialize the module controller
	setupApp();

	xTaskCreatePinnedToCore(&looper, "loop", pinConfig.MAIN_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, NULL, 1);
	// xTaskCreate(&looper, "loop", 8128, NULL, 5, NULL);
}

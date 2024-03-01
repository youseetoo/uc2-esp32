#include "esp_log.h"
#include "src/config/ConfigController.h"
#include "src/serial/SerialProcess.h"
#include "ModuleController.h"
#include "PinConfig.h"


#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define BAUDRATE 500000 // 115200
long lastHeapUpdateTime;

extern "C" void looper(void *p)
{
	for(;;)
	{
		// receive and process serial messages
		serial.loop();
		vTaskDelay(1);
		// process all commands in their modules
		moduleController.loop();
		
		if (pinConfig.dumpHeap && lastHeapUpdateTime +pinConfig.dumpHeapUpdatePeriod < esp_timer_get_time())
		{
			/* code */
			//log_i("free heap:%d", ESP.getFreeHeap());
			//Serial2.println("free heap"+String(ESP.getFreeHeap()));
			lastHeapUpdateTime = esp_timer_get_time();
		}
		
	}
	vTaskDelete(NULL);
}

extern "C" void app_main(void)
{	
	// adjust logging programmatically
	//esp_log_level_set("*", ESP_LOG_DEBUG); //ESP_LOG_NONE);
	// Start Serial
	Serial.begin(BAUDRATE); // default is 115200
	//delay(500);
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
	moduleController.setup();

	log_i("End setup");
	xTaskCreatePinnedToCore(&looper, "loop", pinConfig.MAIN_TASK_STACKSIZE, NULL, 5, NULL,1);
	
	
}



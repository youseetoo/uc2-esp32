#include "tca_controller.h"
#include "tca9535.h"
#include "PinConfig.h"
#include "../motor/FocusMotor.h"
#include "Wire.h"
#define SLAVE_ADDRESS 0x40  // I2C address of the ESP32

namespace tca_controller
{
#define PIN_EXTERNAL_FLAG 128
	TCA9535_Register outRegister;
	bool setExternalPin(uint8_t pin, uint8_t value)
	{
		// This example returns the previous value of the output.
		// Consequently, FastAccelStepper needs to call setExternalPin twice
		// in order to successfully change the output value.
		pin = pin & ~PIN_EXTERNAL_FLAG;
#ifdef USE_TCA9535
		if (pin == 100) // enable
			outRegister.Port.P0.bit.Bit0 = value;
		if (pin == 101) // x
			outRegister.Port.P0.bit.Bit1 = value;
		if (pin == 102) // y
			outRegister.Port.P0.bit.Bit2 = value;
		if (pin == 103) // z
			outRegister.Port.P0.bit.Bit3 = value;
		if (pin == 104) // a
			outRegister.Port.P0.bit.Bit4 = value;
		// log_i("external pin cb for pin:%d value:%d", pin, value);
		if (ESP_OK != TCA9535WriteOutput(&outRegister))
			log_e("i2c write failed");
#endif
		return value;
	}

#ifdef USE_TCA9535
	void dumpRegister(const char *name, TCA9535_Register configRegister)
	{
		log_i("%s port0 b0:%i, b1:%i, b2:%i, b3:%i, b4:%i, b5:%i, b6:%i, b7:%i",
			  name,
			  configRegister.Port.P0.bit.Bit0,
			  configRegister.Port.P0.bit.Bit1,
			  configRegister.Port.P0.bit.Bit2,
			  configRegister.Port.P0.bit.Bit3,
			  configRegister.Port.P0.bit.Bit4,
			  configRegister.Port.P0.bit.Bit5,
			  configRegister.Port.P0.bit.Bit6,
			  configRegister.Port.P0.bit.Bit7);
		/*log_i("%s port1 b0:%i, b1:%i, b2:%i, b3:%i, b4:%i, b5:%i, b6:%i, b7:%i",
			  name,
			  configRegister.Port.P1.bit.Bit0,
			  configRegister.Port.P1.bit.Bit1,
			  configRegister.Port.P1.bit.Bit2,
			  configRegister.Port.P1.bit.Bit3,
			  configRegister.Port.P1.bit.Bit4,
			  configRegister.Port.P1.bit.Bit5,
			  configRegister.Port.P1.bit.Bit6,
			  configRegister.Port.P1.bit.Bit7);*/
	}

	void tca_read_endstopTask(void *p)
	{
		TCA9535_Register inputFromTcaRegister;
		for (;;)
		{
			if (not tca_initiated)
			{
				vTaskDelay(1000);
				continue;
			}
			TCA9535ReadInput(&inputFromTcaRegister);

			FocusMotor::getData()[Stepper::X]->endstop_hit = !inputFromTcaRegister.Port.P0.bit.Bit5;
			FocusMotor::getData()[Stepper::Y]->endstop_hit = !inputFromTcaRegister.Port.P0.bit.Bit6;
			FocusMotor::getData()[Stepper::Z]->endstop_hit = !inputFromTcaRegister.Port.P0.bit.Bit7;
			vTaskDelay(2);
		}
	}

	void init_tca()
	{
		if (ESP_OK != TCA9535Init(pinConfig.I2C_SCL, pinConfig.I2C_SDA, pinConfig.I2C_ADD_TCA)){
			log_e("failed to init tca9535");
			tca_initiated = false;
		}
		else
		{	
			log_i("tca9535 init!");
			tca_initiated = true;
		}
		TCA9535_Register configRegister;
		TCA9535ReadInput(&configRegister);
		dumpRegister("Input", configRegister);
		TCA9535ReadOutput(&configRegister);
		dumpRegister("Output", configRegister);
		TCA9535ReadPolarity(&configRegister);
		dumpRegister("Polarity", configRegister);
		TCA9535ReadConfig(&configRegister);
		dumpRegister("Config", configRegister);

		configRegister.Port.P0.bit.Bit0 = 0; // motor enable
		configRegister.Port.P0.bit.Bit1 = 0; // x
		configRegister.Port.P0.bit.Bit2 = 0; // y
		configRegister.Port.P0.bit.Bit3 = 0; // z
		configRegister.Port.P0.bit.Bit4 = 0; // a
		configRegister.Port.P0.bit.Bit5 = 1;
		configRegister.Port.P0.bit.Bit6 = 1;
		configRegister.Port.P0.bit.Bit7 = 1;

		/*configRegister.Port.P1.bit.Bit0 = 1; // motor enable
		configRegister.Port.P1.bit.Bit1 = 1; // x
		configRegister.Port.P1.bit.Bit2 = 1; // y
		configRegister.Port.P1.bit.Bit3 = 1; // z
		configRegister.Port.P1.bit.Bit4 = 1; // a
		configRegister.Port.P1.bit.Bit5 = 0;
		configRegister.Port.P1.bit.Bit6 = 0;
		configRegister.Port.P1.bit.Bit7 = 0;*/
		if (ESP_OK != TCA9535WriteConfig(&configRegister))
			log_e("failed to write config to tca9535");
		else
			log_i("tca9535 config written!");

		TCA9535ReadConfig(&configRegister);
		dumpRegister("Config", configRegister);
		xTaskCreate(&tca_read_endstopTask, "tca_read_endstopTask", pinConfig.TCA_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, NULL);
	}
#endif
} // namespace tca_controller

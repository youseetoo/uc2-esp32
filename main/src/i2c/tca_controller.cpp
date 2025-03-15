#include "tca_controller.h"
#include "PinConfig.h"
#include "../motor/FocusMotor.h"
#include "Wire.h"

namespace tca_controller
{

	bool setExternalPin(uint8_t pin, uint8_t value)
	{
#ifdef USE_TCA9535
		pin -= 128;
		TCA.write1(pin, value);  
		log_i("Setting TCA9535 pin %i to %i", pin, value);
#endif
		return value;
	}


	bool tca_read_endstop(uint8_t pin)
	{
		#ifdef USE_TCA9535
		if (not tca_initiated)
			return false;
		return !TCA.read1(pin);
		#endif
		return false;
	}

	void init_tca()
	{
		#ifdef USE_TCA9535
		if (tca_initiated)
			return;
		// check if I2C has been initiated - if not, do it
		if (!pinConfig.isI2Cinitiated)
		{
			Wire.begin(pinConfig.I2C_SDA, pinConfig.I2C_SCL, 100000); // 400 Khz is necessary for the M5Dial
			log_i("Wire initiated for I2C on TCA9535");
		}
		TCA.begin();
		tca_initiated = true;

		// Set all pins to output ?		
		TCA.pinMode1(pinConfig.MOTOR_ENABLE, OUTPUT);
		TCA.pinMode1(pinConfig.MOTOR_X_DIR, OUTPUT);
		TCA.pinMode1(pinConfig.MOTOR_Y_DIR, OUTPUT);
		TCA.pinMode1(pinConfig.MOTOR_Z_DIR, OUTPUT);
		TCA.pinMode1(pinConfig.MOTOR_A_DIR, OUTPUT);
		TCA.pinMode1(pinConfig.DIGITAL_IN_1, INPUT);
		TCA.pinMode1(pinConfig.DIGITAL_IN_2, INPUT);
		TCA.pinMode1(pinConfig.DIGITAL_IN_3, INPUT);

		log_i("TCA9535 initiated");
		#endif
	}
} // namespace tca_controller

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
#ifdef USE_TCA9535
		if (pin == 100) // enable
			TCA.write1(pinEnable, value);  
		if (pin == 101) // x
			TCA.write1(pinDirX, value);  
		if (pin == 102) // y
			TCA.write1(pinDirY, value);  
		if (pin == 103) // z
			TCA.write1(pinDirZ, value);  
		if (pin == 104) // a
			TCA.write1(pinDirA, value);  
#endif
		return value;
	}


	bool tca_read_endstop(uint8_t pin)
	{
		#ifdef USE_TCA9535
		if (not tca_initiated)
			return false;
		if (pin == 105){
			return !TCA.read1(pinEndstopX);
		} // endstop X
		if (pin == 106){
			return !TCA.read1(pinEndstopY);
		} // endstop Y
		if (pin == 107) // endstop Z
			return !TCA.read1(pinEndstopZ);
		#endif
		return false;
	}

	void init_tca()
	{
		#ifdef USE_TCA9535
		if (tca_initiated)
			return;
		TCA.begin();
		tca_initiated = true;

		// Set all pins to output ?		
		TCA.pinMode1(pinEnable, OUTPUT);
		TCA.pinMode1(pinDirX, OUTPUT);
		TCA.pinMode1(pinDirY, OUTPUT);
		TCA.pinMode1(pinDirZ, OUTPUT);
		TCA.pinMode1(pinDirA, OUTPUT);
		TCA.pinMode1(pinEndstopX, INPUT);
		TCA.pinMode1(pinEndstopY, INPUT);
		TCA.pinMode1(pinEndstopZ, INPUT);

		#endif
	}
} // namespace tca_controller

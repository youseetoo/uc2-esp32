#pragma once
#include <PinConfig.h>
#include "Arduino.h"
#ifdef USE_TCA9535
#include "TCA9555.h"
#endif

namespace tca_controller
{

/*
I/O-exp. GPIO	target	Input/Output	comment
00	Step_Enable	O	to all stepper drivers EN pin
01	X_Dir	O	Stepper driver DIR
02	Y_Dir	O	Stepper driver DIR
03	Z_Dir	O	Stepper driver DIR
04	A_Dir	O	Stepper driver DIR and to A-STEP+DIR header (J118)
05	X_LIMIT	I	Limit switch input. Output LOW to light LED
06	Y_LIMIT	I	Limit switch input. Output LOW to light LED
07	Z_LIMIT	I	Limit switch input. Output LOW to light LED
*/

    #ifdef USE_TCA9535
	static TCA9535 TCA(pinConfig.I2C_ADD_TCA);
    #endif
    bool setExternalPin(uint8_t pin, uint8_t value);
    void init_tca();
    static bool tca_initiated = false;
    bool tca_read_endstop(uint8_t pin);

};
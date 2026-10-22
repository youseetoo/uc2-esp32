#pragma once

// The uc2 shim (components/tmcstepper/include/Arduino.h) resolves to the
// uc2_hal umbrella, which does not pull in stdint before this header is
// parsed — SW_SPI.h itself first uses uint16_t below. The Arduino core's
// real Arduino.h provided it; add it here so the header is self-contained.
#include <stdint.h>

#if defined(ARDUINO) && ARDUINO >= 100
	#include <Arduino.h>
#elif defined(bcm2835)
	#include "source/rpi_bcm2835.h"
	#include "source/bcm2835_spi.h"
	#include "source/bcm2835_stream.h"
#elif __cplusplus >= 201703L
	#if __has_include(<Arduino.h>)
		#include <Arduino.h>
	#endif
#endif

#include "TMC_platforms.h"

class SW_SPIClass {
	public:
		SW_SPIClass(uint16_t sw_mosi_pin, uint16_t sw_miso_pin, uint16_t sw_sck_pin);
		void init();
		void begin() {};
		uint8_t transfer(uint8_t ulVal);
		uint16_t transfer16(uint16_t data);
		void endTransaction() {};
	private:
		const uint16_t	mosi_pin,
						miso_pin,
						sck_pin;

		#if defined(ARDUINO_ARCH_AVR)
			fastio_bm mosi_bm,
					miso_bm,
					sck_bm;
			fastio_reg mosi_register,
							 miso_register,
							 sck_register;
		#endif
};

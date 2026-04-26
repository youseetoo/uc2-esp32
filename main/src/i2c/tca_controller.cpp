#include "tca_controller.h"
#include "PinConfig.h"
#include "../motor/FocusMotor.h"
#include "Wire.h"
#include <cstring>

namespace tca_controller
{

#ifdef USE_TCA9535
	namespace {
		// Tracks per-TCA-pin (0..15, after stripping the 128 flag) state so the
		// FastAccelStepper external-pin callback can report the *actual* pin
		// state, not the *requested* one. This is required so FAS inserts its
		// 2 ms direction-change pause; otherwise step pulses are enqueued while
		// the I2C write to the TCA is still in flight, causing extra pulses on
		// direction reversal.
		uint8_t  s_requested[16];
		uint8_t  s_acked[16];
		uint32_t s_request_us[16];
		bool     s_inited = false;

		// A single-byte TCA write at 100 kHz I2C is ~250-500 us. Be generous
		// to cover bus contention from other I2C devices.
		constexpr uint32_t TCA_SETTLE_US = 800;

		inline void ensure_init() {
			if (s_inited) return;
			std::memset(s_requested, 0xFF, sizeof(s_requested));
			std::memset(s_acked,     0xFF, sizeof(s_acked));
			std::memset(s_request_us, 0, sizeof(s_request_us));
			s_inited = true;
		}
	}
#endif

	bool setExternalPin(uint8_t pin, uint8_t value)
	{
#ifdef USE_TCA9535
		ensure_init();
		const uint8_t p = (uint8_t)(pin - 128);
		if (p >= 16) {
			// Out-of-range: best-effort write, no tracking possible.
			TCA.write1(p, value);
			return value;
		}

		if (s_requested[p] != value) {
			// New request: kick off the I2C write, but do NOT yet claim
			// success. Return the last acknowledged state so FAS sees
			// "not yet" and inserts its 2ms pause; on the next fill cycle
			// we'll be polled again.
			TCA.write1(p, value);
			s_requested[p]  = value;
			s_request_us[p] = micros();
			return s_acked[p];
		}

		// Same value re-requested (FAS is polling us): only ack once the
		// write has had time to land on the TCA.
		if ((uint32_t)(micros() - s_request_us[p]) >= TCA_SETTLE_US) {
			s_acked[p] = value;
		}
		return s_acked[p];
#else
		return value;
#endif
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

#include "tca_controller.h"
#include "PinConfig.h"
#include "../motor/FocusMotor.h"
#include "Wire.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
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
		uint32_t s_settle_us[16];
		bool     s_inited = false;

		// Margin added on top of the *measured* I2C write duration. Covers:
		//   - any RMT/MCPWM step pulses already queued in hardware that may
		//     still drain after TCA.write1() returns,
		//   - the TCA9535 chip's own input-to-output propagation delay,
		//   - drift between micros() captured around TCA.write1() and the
		//     real I2C bus state when other tasks (DigitalInController
		//     reading endstops, dial display, etc.) contend for the bus.
		// At 15 kHz step rate, 67 us = 1 step. The standalone reproducer
		// uses 500 us with no other I2C traffic and works perfectly. The
		// firmware has concurrent endstop reads, so we add headroom.
		constexpr uint32_t TCA_SETTLE_MARGIN_US = 800;

		// Mutex serializing every Wire transaction the TCA driver performs.
		// FAS runs its queue-filler in a dedicated FreeRTOS task and calls
		// setExternalPin() from there; the main loop task calls
		// tca_read_endstop() every iteration. Without explicit serialization
		// the order in which the two tasks acquire the Wire bus is racy and
		// can let a read interleave with the read-modify-write that
		// RobTillaart's TCA9555 library performs internally for write1().
		SemaphoreHandle_t s_tcaMutex = nullptr;

		inline void ensure_init() {
			if (s_inited) return;
			// Initialize to LOW (0). seed() in init_tca() overwrites the pins
			// we actually drive; pins we never touch stay LOW so the FAS
			// callback's first poll on an unseeded pin does not falsely
			// report HIGH (which 0xFF would, since any non-zero is true).
			std::memset(s_requested, 0, sizeof(s_requested));
			std::memset(s_acked,     0, sizeof(s_acked));
			std::memset(s_request_us, 0, sizeof(s_request_us));
			std::memset(s_settle_us,  0, sizeof(s_settle_us));
			if (s_tcaMutex == nullptr) {
				s_tcaMutex = xSemaphoreCreateMutex();
			}
			s_inited = true;
		}

		inline void tca_lock() {
			if (s_tcaMutex != nullptr) {
				xSemaphoreTake(s_tcaMutex, portMAX_DELAY);
			}
		}
		inline void tca_unlock() {
			if (s_tcaMutex != nullptr) {
				xSemaphoreGive(s_tcaMutex);
			}
		}
	}
#endif

bool setExternalPin(uint8_t pin, uint8_t value)
{
#ifdef USE_TCA9535
    // Defensive: must run before we touch s_requested/s_acked. init_tca()
    // normally calls ensure_init() first, but if the FAS engine queue-fill
    // task fires before init_tca() has completed (boot ordering surprise
    // on a different config) we would otherwise read uninitialized memory
    // and lie to FAS about the current pin state.
    ensure_init();
    const uint8_t p = (uint8_t)(pin - 128);
    if (p >= 16) {
        // No FAS_PIN_EXTERNAL_FLAG set -> caller bypassed the handshake.
        // Best effort: do the write under the mutex and report success.
        tca_lock();
        TCA.write1(p, value);
        tca_unlock();
        return value;
    }

    if (s_requested[p] != value) {
        // New request: do the write, MEASURE its duration, and only ack
        // once at least that long has elapsed since it started. This
        // accounts for the read-modify-write the underlying library
        // performs (~1-1.5 ms at 100 kHz I2C) and any bus contention.
        tca_lock();
        const uint32_t t0 = micros();
        TCA.write1(p, value);
        const uint32_t write_us = micros() - t0;
        tca_unlock();

        s_requested[p]  = value;
        s_request_us[p] = t0;
        // Settle = the actual write duration, plus a margin large enough
        // to cover already-queued RMT/MCPWM pulses and bus contention
        // (see TCA_SETTLE_MARGIN_US comment above).
        s_settle_us[p]  = write_us + TCA_SETTLE_MARGIN_US;

        // Return the *previous* state so FAS knows the new value has not
        // yet landed; FAS will insert its 2 ms direction-change pause and
        // poll us again on the next queue-fill cycle.
        return s_acked[p];
    }

    if ((uint32_t)(micros() - s_request_us[p]) >= s_settle_us[p]) {
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
		// Serialize with FAS-task writes to the same TCA so the read does
		// not interleave with a read-modify-write that write1() performs.
		tca_lock();
		const bool v = !TCA.read1(pin);
		tca_unlock();
		return v;
		#endif
		return false;
	}

	void init_tca()
	{
		#ifdef USE_TCA9535
		if (tca_initiated)
			return;
		// Create the wrapper state (and the I2C mutex) before we touch the
		// bus, so subsequent locked sections work correctly.
		ensure_init();
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

		auto seed = [](uint8_t pin, uint8_t v) {
			TCA.write1(pin, v);
			s_requested[pin] = v;
			s_acked[pin]     = v;
			s_request_us[pin] = micros();
			s_settle_us[pin]  = 0;     // already settled — was just blocking-written
		};
		// MOTOR_ENABLE seed: idle = motor disabled. With active-low drivers
		// (MOTOR_ENABLE_INVERTED == true) the disabled state is HIGH, not
		// LOW. The previous code had this inverted, which seeded the TCA
		// pin to "motor enabled" at boot — harmless to FAS handshake but
		// unintentionally energizes the motor before FAS owns the pin.
		seed(pinConfig.MOTOR_ENABLE,  pinConfig.MOTOR_ENABLE_INVERTED ? HIGH : LOW);
		seed(pinConfig.MOTOR_X_DIR,   LOW);
		seed(pinConfig.MOTOR_Y_DIR,   LOW);
		seed(pinConfig.MOTOR_Z_DIR,   LOW);
		seed(pinConfig.MOTOR_A_DIR,   LOW);
		log_i("TCA9535 initiated");
		#endif
	}
} // namespace tca_controller

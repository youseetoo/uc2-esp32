#pragma once
#include <driver/gpio.h>
// IDF 6.1: the legacy driver/pcnt.h is gone; PCNT is esp_driver_pcnt
// (driver/pulse_cnt.h, handle-based + watch-point callbacks).
#include <driver/pulse_cnt.h>
#ifndef ARDUINO
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/portable.h>
#include <freertos/semphr.h>
#endif

// ESP32-S3 has 4 PCNT units (PCNT_LL_UNITS_PER_INST == 4); each has 2 channels.
#define MAX_ESP32_ENCODERS 4
#define 	_INT16_MAX 32766
#define  	_INT16_MIN -32766
#define ISR_CORE_USE_DEFAULT (0xffffffff)

enum class encType {
	single,
	half,
	full
};

enum class puType {
	up,
	down,
	none
};

class ESP32Encoder;

typedef void (*enc_isr_cb_t)(void*);

class ESP32Encoder {
public:
	/**
	 * @brief Construct a new ESP32Encoder object
	 *
	 * @param always_interrupt set to true to enable interrupt on every encoder pulse, otherwise false
	 * @param enc_isr_cb callback executed on every encoder ISR, gets a pointer to
	 * 	the ESP32Encoder instance as an argument, no effect if always_interrupt is
	 * 	false
	 */
	ESP32Encoder(bool always_interrupt=false, enc_isr_cb_t enc_isr_cb=nullptr, void* enc_isr_cb_data=nullptr);
	~ESP32Encoder();
	void attachHalfQuad(int aPintNumber, int bPinNumber);
	void attachFullQuad(int aPintNumber, int bPinNumber);
	void attachSingleEdge(int aPintNumber, int bPinNumber);
	int64_t getCount();
	int64_t clearCount();
	int64_t pauseCount();
	int64_t resumeCount();
	void detach();
	[[deprecated("Replaced by detach")]] void detatch();
	bool isAttached(){return attached;}
	void setCount(int64_t value);
	void setFilter(uint16_t value);
	static ESP32Encoder *encoders[MAX_ESP32_ENCODERS];
	bool always_interrupt;
	gpio_num_t aPinNumber;
	gpio_num_t bPinNumber;
	// Logical PCNT unit index (== the slot this encoder reserved in encoders[]).
	// Kept as an int so callers (EncoderBackend) can pin/compare it against a
	// known unit (e.g. FastAccelStepper's FAS_PCNT_UNIT).
	int unit;
	// New-API handle for the actually-allocated PCNT unit (null until attach()).
	pcnt_unit_handle_t unit_handle;
	int countsMode = 2;
	volatile int64_t count=0;
	static puType useInternalWeakPullResistors;
	static uint32_t isrServiceCpuCore;
	enc_isr_cb_t _enc_isr_cb;
	void* _enc_isr_cb_data;

private:
	static bool attachedInterrupt;
	void attach(int aPintNumber, int bPinNumber, encType et);
	int64_t getCountRaw();
	bool attached;
  bool direction;
  bool working;
};

//Added by Sloeber
#pragma once

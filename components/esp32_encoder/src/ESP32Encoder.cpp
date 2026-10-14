/*
 * ESP32Encoder.cpp — raw-IDF port (ESP32-S3, IDF 6.1, esp_driver_pcnt /
 * driver/pulse_cnt.h, handle-based + watch-point callbacks).
 *
 * Ported from madhephaestus/ESP32Encoder (legacy driver/pcnt.h, register-level
 * ISR). Same public surface and behavior: the 16-bit PCNT counter saturates at
 * ±_INT16_MAX/_INT16_MIN; each wrap is caught by a watch-point callback that
 * accumulates the limit into the software `count`, so getCount() is unbounded
 * (int64). Quadrature: channel 0 = A edge + B level (reverse); channel 1
 * (full) = B edge + A level.
 */
#include <ESP32Encoder.h>
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG_ENCODER = "ESP32Encoder";

// Spinlock guarding the software `count` (shared between the main task and the
// PCNT watch-point ISR). _SAFE variant tolerates the legacy nested-lock pattern.
static portMUX_TYPE encoder_spinlock = portMUX_INITIALIZER_UNLOCKED;
#define _ENTER_CRITICAL() portENTER_CRITICAL_SAFE(&encoder_spinlock)
#define _EXIT_CRITICAL() portEXIT_CRITICAL_SAFE(&encoder_spinlock)

puType ESP32Encoder::useInternalWeakPullResistors = puType::down;
uint32_t ESP32Encoder::isrServiceCpuCore = ISR_CORE_USE_DEFAULT;
ESP32Encoder *ESP32Encoder::encoders[MAX_ESP32_ENCODERS] = { NULL, };
bool ESP32Encoder::attachedInterrupt = false;

ESP32Encoder::ESP32Encoder(bool always_interrupt_, enc_isr_cb_t enc_isr_cb, void *enc_isr_cb_data)
	: always_interrupt{always_interrupt_},
	  aPinNumber{(gpio_num_t) 0},
	  bPinNumber{(gpio_num_t) 0},
	  unit{-1},
	  unit_handle{nullptr},
	  countsMode{2},
	  count{0},
	  _enc_isr_cb(enc_isr_cb),
	  _enc_isr_cb_data(enc_isr_cb_data),
	  attached{false},
	  direction{false},
	  working{false}
{
	if (enc_isr_cb_data == nullptr) _enc_isr_cb_data = this;
}

ESP32Encoder::~ESP32Encoder()
{
	detach();
}

// PCNT watch-point callback (ISR context): fires when the 16-bit counter reaches
// a configured watch point (±_INT16_MAX/_INT16_MIN). Accumulate the limit into
// the software counter and reset the hardware counter to 0.
static bool esp32encoder_pcnt_on_reach(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx)
{
	ESP32Encoder *esp32enc = static_cast<ESP32Encoder *>(user_ctx);
	_ENTER_CRITICAL();
	if (edata->watch_point_value >= _INT16_MAX) {
		esp32enc->count += _INT16_MAX;
	} else if (edata->watch_point_value <= _INT16_MIN) {
		esp32enc->count += _INT16_MIN;
	}
	pcnt_unit_clear_count(unit);
	if (esp32enc->_enc_isr_cb) {
		esp32enc->_enc_isr_cb(esp32enc->_enc_isr_cb_data);
	}
	_EXIT_CRITICAL();
	return false; // keep the PCNT unit running.
}

void ESP32Encoder::detach()
{
	if (unit_handle) {
		pcnt_unit_stop(unit_handle);
		pcnt_unit_disable(unit_handle);
		pcnt_del_unit(unit_handle);
		unit_handle = nullptr;
	}
	if (unit >= 0 && unit < MAX_ESP32_ENCODERS) {
		ESP32Encoder::encoders[unit] = nullptr;
	}
	unit = -1;
	attached = false;
}

void ESP32Encoder::detatch()
{
	detach();
}

void ESP32Encoder::attach(int a, int b, encType et)
{
	if (attached) {
		ESP_LOGE(TAG_ENCODER, "attach: already attached");
		return;
	}

	// Reserve a logical unit slot (encoders[]); the caller pins this via
	// pre-reserved slots so FAS_PCNT_UNIT (stepper) is never taken by an encoder.
	int index = 0;
	for (; index < MAX_ESP32_ENCODERS; index++) {
		if (ESP32Encoder::encoders[index] == nullptr) {
			encoders[index] = this;
			break;
		}
	}
	if (index == MAX_ESP32_ENCODERS) {
		while (1) {
			ESP_LOGE(TAG_ENCODER, "Too many encoders, FAIL!");
			vTaskDelay(pdMS_TO_TICKS(100));
		}
	}
	unit = index;
	this->aPinNumber = (gpio_num_t) a;
	this->bPinNumber = (gpio_num_t) b;

	// GPIO: input, weak pull-down (matches legacy default).
	// IDF 6.1: gpio_pad_select_gpio is the ROM function esp_rom_gpio_pad_select_gpio.
	esp_rom_gpio_pad_select_gpio(aPinNumber);
	esp_rom_gpio_pad_select_gpio(bPinNumber);
	gpio_set_direction(aPinNumber, GPIO_MODE_INPUT);
	gpio_set_direction(bPinNumber, GPIO_MODE_INPUT);
	if (useInternalWeakPullResistors == puType::down) {
		gpio_pulldown_en(aPinNumber);
		gpio_pulldown_en(bPinNumber);
	}
	if (useInternalWeakPullResistors == puType::up) {
		gpio_pullup_en(aPinNumber);
		gpio_pullup_en(bPinNumber);
	}

	// Create the PCNT unit (16-bit, saturating at the limits).
	pcnt_unit_config_t unit_cfg = {};
	unit_cfg.group_id = 0;
	unit_cfg.clk_src = PCNT_CLK_SRC_DEFAULT;
	unit_cfg.low_limit = _INT16_MIN;
	unit_cfg.high_limit = _INT16_MAX;
	unit_cfg.intr_priority = 1;
	unit_cfg.flags.accum_count = false;
	if (pcnt_new_unit(&unit_cfg, &unit_handle) != ESP_OK) {
		ESP_LOGE(TAG_ENCODER, "attach: pcnt_new_unit failed");
		unit_handle = nullptr;
		return;
	}

	// Channel 0: A = edge signal, B = level signal.
	pcnt_chan_config_t ch0_cfg = {};
	ch0_cfg.edge_gpio_num = a;
	ch0_cfg.level_gpio_num = b;
	pcnt_channel_handle_t ch0 = nullptr;
	if (pcnt_new_channel(unit_handle, &ch0_cfg, &ch0) != ESP_OK) {
		ESP_LOGE(TAG_ENCODER, "attach: pcnt_new_channel(ch0) failed");
		return;
	}
	if (et == encType::single) {
		// Count only on A falling edges (legacy: pos=HOLD, neg=INCREASE).
		pcnt_channel_set_edge_action(ch0, PCNT_CHANNEL_EDGE_ACTION_HOLD, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
		pcnt_channel_set_level_action(ch0, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
	} else {
		// Quadrature: A rising -> DEC, A falling -> INC, B high -> reverse.
		pcnt_channel_set_edge_action(ch0, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
		pcnt_channel_set_level_action(ch0, PCNT_CHANNEL_LEVEL_ACTION_INVERSE, PCNT_CHANNEL_LEVEL_ACTION_KEEP);
	}

	// Channel 1 (full quadrature only): B = edge signal, A = level signal.
	if (et == encType::full) {
		pcnt_chan_config_t ch1_cfg = {};
		ch1_cfg.edge_gpio_num = b;
		ch1_cfg.level_gpio_num = a;
		pcnt_channel_handle_t ch1 = nullptr;
		if (pcnt_new_channel(unit_handle, &ch1_cfg, &ch1) == ESP_OK) {
			pcnt_channel_set_edge_action(ch1, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
			pcnt_channel_set_level_action(ch1, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
		} else {
			ESP_LOGE(TAG_ENCODER, "attach: pcnt_new_channel(ch1) failed");
		}
	}

	// Wrap the 16-bit counter at the limits via watch points.
	if (pcnt_unit_add_watch_point(unit_handle, _INT16_MAX) != ESP_OK ||
	    pcnt_unit_add_watch_point(unit_handle, _INT16_MIN) != ESP_OK) {
		ESP_LOGE(TAG_ENCODER, "attach: add_watch_point failed");
	}

	// Register the watch-point callback (installs the ISR service lazily on enable).
	pcnt_event_callbacks_t cbs = {};
	cbs.on_reach = esp32encoder_pcnt_on_reach;
	if (pcnt_unit_register_event_callbacks(unit_handle, &cbs, this) != ESP_OK) {
		ESP_LOGE(TAG_ENCODER, "attach: register_event_callbacks failed");
	}

	// Default glitch filter (legacy default: 250) — must be set in init state.
	pcnt_glitch_filter_config_t filter_cfg = {};
	filter_cfg.max_glitch_ns = 250u * 100u;
	pcnt_unit_set_glitch_filter(unit_handle, &filter_cfg);

	pcnt_unit_enable(unit_handle);
	pcnt_unit_start(unit_handle);
	pcnt_unit_clear_count(unit_handle);
	count = 0;
	attached = true;
}

void ESP32Encoder::attachHalfQuad(int a, int b) { attach(a, b, encType::half); }
void ESP32Encoder::attachSingleEdge(int a, int b) { attach(a, b, encType::single); }
void ESP32Encoder::attachFullQuad(int a, int b) { attach(a, b, encType::full); }

void ESP32Encoder::setCount(int64_t value)
{
	_ENTER_CRITICAL();
	count = value - getCountRaw();
	_EXIT_CRITICAL();
}

int64_t ESP32Encoder::getCountRaw()
{
	int c = 0;
	if (unit_handle) {
		_ENTER_CRITICAL();
		pcnt_unit_get_count(unit_handle, &c);
		_EXIT_CRITICAL();
	}
	return c;
}

int64_t ESP32Encoder::getCount()
{
	_ENTER_CRITICAL();
	int64_t result = count + getCountRaw();
	_EXIT_CRITICAL();
	return result;
}

int64_t ESP32Encoder::clearCount()
{
	int prev = 0;
	_ENTER_CRITICAL();
	count = 0;
	if (unit_handle) {
		pcnt_unit_get_count(unit_handle, &prev);
		pcnt_unit_clear_count(unit_handle);
	}
	_EXIT_CRITICAL();
	return prev;
}

int64_t ESP32Encoder::pauseCount()
{
	int c = 0;
	if (unit_handle) {
		_ENTER_CRITICAL();
		pcnt_unit_get_count(unit_handle, &c);
		_EXIT_CRITICAL();
		pcnt_unit_stop(unit_handle);
	}
	return c;
}

int64_t ESP32Encoder::resumeCount()
{
	int c = 0;
	if (unit_handle) {
		_ENTER_CRITICAL();
		pcnt_unit_get_count(unit_handle, &c);
		_EXIT_CRITICAL();
		pcnt_unit_start(unit_handle);
	}
	return c;
}

void ESP32Encoder::setFilter(uint16_t value)
{
	// The new API sets the glitch filter only in the init (disabled) state; a
	// post-enable call is best-effort (ignored). The default (250) is applied in
	// attach(), so a caller's late setFilter() is a no-op on IDF 6.1.
	if (!unit_handle) {
		return;
	}
	if (value == 0) {
		pcnt_unit_set_glitch_filter(unit_handle, nullptr);
	} else {
		pcnt_glitch_filter_config_t filter_cfg = {};
		filter_cfg.max_glitch_ns = (uint32_t) value * 100u;
		pcnt_unit_set_glitch_filter(unit_handle, &filter_cfg);
	}
}

// LedController.c — T4.1: converted from C++ (namespace LedController) to C.
// The Adafruit library objects live behind the led_strip_c.h shim
// (led/led_strip_c.cpp); this file is pure C.
#include "LedController.h"
#include "led_strip_c.h"
#include "JsonKeys.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "uc2_hal/uc2_misc.h" // uc2_random
#include "uc2_hal/uc2_time.h" // millis / delay (no-Arduino drop-in; do NOT redefine here)
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>
// POSIX case-insensitive compare (newlib provides strcasecmp; MSVC does not).
#ifdef _MSC_VER
#define strcasecmp(a, b) _stricmp(a, b)
#else
#include <strings.h>
#endif
#include "cJSON.h"
#include "PinConfig.h" // user-provided config, if needed
#include "../qid/QidRegistry.h"
#include "../canopen/DeviceRouter.h"
#ifdef THERMAL_CONTROLLER
#include "ThermalController.h"
#endif
#ifdef WAVESHARE_ESP32S3_LEDARRAY
#include "uc2_hal/uc2_gpio.h" // pinMode (C-linked Arduino compat)
#include "soc/gpio_sig_map.h" // SIG_IN_FUNC212_IDX
#include "soc/io_mux_reg.h"
#endif

// --------------------------------------------------------------------------------
// FastAccelStepper / NeoPixel RMT coexistence — see Ws2812Rmt.cpp for context.
//
// Adafruit_NeoPixel's built-in espShow() calls rmt_driver_install(ch, 0, 0),
// i.e. WITHOUT ESP_INTR_FLAG_SHARED. On the ESP32-classic the whole RMT
// peripheral has a single IRQ line, and FastAccelStepper has already grabbed
// it via rmt_isr_register(..., ESP_INTR_FLAG_SHARED, ...). Adafruit's install
// then fails silently and ledShow() hangs in rmt_wait_tx_done().
//
// We bypass Adafruit's espShow() entirely. Adafruit_NeoPixel is kept only as a
// pixel-buffer / colour-utility object (setPixelColor, getPixels, Color, ...).
// All TX is done through ws2812_rmt_show() on a free RMT channel
// (routing lives in led_strip_c.cpp).
#if !defined(DOTSTAR) && !defined(HUB75)
#ifndef UC2_WS2812_RMT_CHANNEL
#  if defined(CONFIG_IDF_TARGET_ESP32S3)
     // S3 has 4 TX channels (0..3); default to ch0. If motors are present the
     // caller should override via -DUC2_WS2812_RMT_CHANNEL=<free channel>.
#    define UC2_WS2812_RMT_CHANNEL 0
#  else
     // Classic ESP32: FAS owns channels 0..3 (one per stepper on UC2_4).
#    define UC2_WS2812_RMT_CHANNEL 4
#  endif
#endif
#endif

// --------------------------------------------------------------------------------
// 3) The LedController module (was: namespace LedController)
// --------------------------------------------------------------------------------
static const char *TAG = "LedController";

// ------------------------------------------------------------------
// T5.3 Arduino->IDF shims (so the bulk of this file needs no per-call
// edits): log_* short macros, Serial (UART0/printf), and a minimal Arduino
// String stand-in (str_lower_into: toLowerCase + strcmp). millis()/delay()
// come from uc2_hal/uc2_time.h (no-Arduino drop-in) — NOT redefined here:
// a local copy clashes with it in the host-test include chain, where this
// file is #included into a .cpp.
// ------------------------------------------------------------------
#define log_i(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)
#define log_e(fmt, ...) ESP_LOGE(TAG, fmt, ##__VA_ARGS__)
#define log_w(fmt, ...) ESP_LOGW(TAG, fmt, ##__VA_ARGS__)
static inline void Serial_println(const char *s) { printf("%s\n", s); }
// Arduino String stand-in: lowercase a string into a fixed buffer.
static void str_lower_into(char *dst, size_t dst_size, const char *src)
{
	size_t i = 0;
	if (!src) src = "";
	for (; src[i] && i < dst_size - 1; i++)
		dst[i] = (char)tolower((unsigned char)src[i]);
	dst[i] = '\0';
}

// The Adafruit object (opaque handle from led_strip_c.h).
static void *matrix = NULL;
static bool isOn = false;

// for in-loop interaction
static int currentLedForStatus = idle; // LedForStatus::idle

// Pattern animation state
static uint8_t activePatternId = 0;     // 0=none, 1=rainbow, 2=breathe, 3=chase, ...
static uint16_t activePatternSpeed = 100; // ms per frame
static uint32_t lastPatternUpdateMs = 0;
static uint8_t patternFrame = 0;

// LED safety auto-off tracking
static uint16_t currentTotalIntensity = 0;  // Sum of R + G + B channels (0-765)
static unsigned long highIntensityStartTime = 0;  // When high intensity was first detected
static bool highIntensityWarningShown = false;    // Track if warning has been shown
static bool ledAutoOffTriggered = false;          // Track if auto-off has been triggered

// Toggle functionality for Circle button
static unsigned long circleLastEventTime = 0;
static const unsigned long BUTTON_DEBOUNCE_TIME = 300; // ms to prevent multiple toggles
static bool ledToggleState = false; // false = off, true = on

#ifdef HUB75
static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
#endif

// ------------------------------------------------
// Internal show() helper — routes through our Ws2812Rmt driver for the
// NeoPixel build path (routing lives in led_strip_c.cpp). DOTSTAR / HUB75
// keep using the Adafruit show().
// ------------------------------------------------
static inline void ledShow(void)
{
	if (matrix) led_strip_show(matrix);
}

// ------------------------------------------------
// HELPER: Convert (x, y) -> single index
// ------------------------------------------------
uint16_t led_xy_to_index(int x, int y)
{
	// For top-left → bottom-right wiring
	return (uint16_t)(y * pinConfig.MATRIX_W + x);
}

// ------------------------------------------------
// HELPER: Set one pixel in x/y coords
// ------------------------------------------------
void led_set_pixel_xy(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{

	if (x < 0 || x >= pinConfig.MATRIX_W || y < 0 || y >= pinConfig.MATRIX_H)
		return;
#ifdef HUB75
	led_strip_draw_pixel(matrix, x, y, rgb565(r, g, b));
#else
	uint16_t idx = led_xy_to_index(x, y);
	log_i("Setting LED at (x=%d, y=%d, index=%d) to color: R=%d, G=%d, B=%d", x, y, idx, r, g, b);
	led_strip_set_pixel(matrix, idx, led_strip_color(r, g, b));
#endif
}

// ------------------------------------------------
// 4) Setup: Initialize the matrix
// ------------------------------------------------
void led_setup(void)
{
	// On boards where the on-board NeoPixel is only used as a status
	// indicator (e.g. the CAN HAT master with IS_STATUS_LED=true), the
	// pixel is owned by SignalController via /signal_act. Skip the heavy
	// LED-array setup here so we don't double-init the same data pin.
	if (pinConfig.IS_STATUS_LED) {
		// Allocate the pixel buffer so matrix is non-null (the matrix-deref
		// helpers must not be provably-null under -Werror=nonnull), but skip
		// begin()/show(): the status pixel is owned by SignalController via
		// /signal_act, so we must not double-init the same data pin.
		matrix = led_strip_create();
		log_i("LedController: IS_STATUS_LED=true, status pixel handled by SignalController — skipping setup");
		return;
	}

	log_i("Initializing LED matrix");
#ifdef HUB75
	matrix = led_strip_create();
	if (!matrix || !led_strip_begin(matrix))
	{
#ifndef UNIT_TEST
		Serial_println("Protomatter init failed");
#endif
		while (1)
			delay(10);
	}

#else
	log_i("LedController: LED_COUNT=%u pin=%d — using Ws2812Rmt on RMT ch %d",
		  (unsigned)LED_COUNT, (int)pinConfig.LED_PIN, UC2_WS2812_RMT_CHANNEL);
	// Pixel buffer + colour helpers only — NO RMT install via Adafruit.
	// We deliberately do NOT call begin() (which would try to set up
	// the RX/TX peripheral via the IDF driver and conflict with FAS).
	matrix = led_strip_create();
#if !defined(USE_FASTACCEL)
	log_i("LedController: calling begin() to set up GPIO (no FastAccelStepper conflict)");
	led_strip_begin(matrix); // Only call begin() if we're not sharing the RMT with FastAccelStepper, to avoid conflicts. If FastAccelStepper is used, we'll do the necessary GPIO setup ourselves below.
#else
	log_i("LedController: skipping begin() to avoid RMT conflict with FastAccelStepper; configuring GPIO ourselves");
	// Bring up our own RMT-based driver on a channel above FAS's allocation.
	if (!led_strip_begin(matrix)) {
		log_e("LedController: ws2812_rmt_begin() failed — strip will stay dark");
	}
#endif
	led_strip_set_brightness(matrix, 255);
	led_strip_clear(matrix);

	log_i("LedController: about to show() — first RMT TX");
	ledShow();
	log_i("LedController: first show() returned");

#endif

	// test led array
	int initIntensity = 100;


	led_fill_all(initIntensity, initIntensity, initIntensity);
	delay(10);
	led_fill_all(0, 0, 0);
	ledShow(); //  Update strip to match

	isOn = false;

#ifdef WAVESHARE_ESP32S3_LEDARRAY
	// This will enable the masterboard to take over control of the LED array, too (alongside the firmware)
#ifndef UNIT_TEST
	Serial_println("WAVESHARE_ESP32S3_LEDARRAY");
#endif
	pinMode(14, OUTPUT);
	pinMode(6, INPUT);
	esp_rom_gpio_connect_in_signal(6, SIG_IN_FUNC212_IDX, false);
	esp_rom_gpio_connect_out_signal(14, SIG_IN_FUNC212_IDX, false, false);
#endif
	log_i("LED matrix initialization complete");
}

// ------------------------------------------------
// 5) "Off" all LEDs
// ------------------------------------------------
void led_turn_off(void)
{
#ifdef HUB75
	led_strip_clear(matrix); // fillScreen(0)
	ledShow();
#else
	led_strip_clear(matrix);
	ledShow();
#endif
	isOn = false;
}

// ------------------------------------------------
// 6) Fill entire matrix with a single color
// ------------------------------------------------
void led_fill_all(uint8_t r, uint8_t g, uint8_t b)
{
	log_i("Filling all LEDs with color: R=%d, G=%d, B=%d", r, g, b);
#ifdef HUB75
	uint16_t c = rgb565(r, g, b);
	led_strip_fill(matrix, (uint32_t)c);
#else
	for (uint16_t i = 0; i < LED_COUNT; i++)
		led_strip_set_pixel(matrix, i, led_strip_color(r, g, b));
#endif
	ledShow();
	isOn = (r || g || b);
}

// ------------------------------------------------
// 7) Light a SINGLE LED by index
// ------------------------------------------------
void led_set_single(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
	log_i("Setting single LED at index %d to color: R=%d, G=%d, B=%d", index, r, g, b);
#ifdef HUB75
	led_strip_clear(matrix);
	int x = index % pinConfig.MATRIX_W;
	int y = index / pinConfig.MATRIX_W;
	led_strip_draw_pixel(matrix, x, y, rgb565(r, g, b));
	ledShow();
#else
	if (index < LED_COUNT)
	{
		// clear intentionally omitted (as before)
		led_strip_set_pixel(matrix, index, led_strip_color(r, g, b));
		ledShow();
	}
#endif
	isOn = (r || g || b);
}

// ------------------------------------------------
// 8) Halves: fill left/right/top/bottom with color, rest dark
//    For illumination board with concentric rings, uses segment indices from PinConfig
// ------------------------------------------------
void led_fill_halves(const char *region, uint8_t r, uint8_t g, uint8_t b)
{
	log_i("fillHalves: region=%s, r=%d, g=%d, b=%d", region, r, g, b);
#ifndef HUB75

	led_strip_clear(matrix);

	// Check if this is an illumination board with ring segments defined
	#ifdef LED_CONTROLLER
	if (pinConfig.SEGMENT_INNER_COUNT > 0)
	{
		// Use ring segment addressing for illumination board
		led_fill_halves_ring_segments(region, r, g, b);
		return;
	}
	#endif

	// Default matrix-based halves for other configurations
	if (strcasecmp(region, "left") == 0)
	{
		for (int y = 0; y < pinConfig.MATRIX_H; y++)
		{
			for (int x = 0; x < pinConfig.MATRIX_W / 2; x++)
			{
				led_set_pixel_xy(x, y, r, g, b);
			}
		}
	}
	else if (strcasecmp(region, "right") == 0)
	{
		for (int y = 0; y < pinConfig.MATRIX_H; y++)
		{
			for (int x = pinConfig.MATRIX_W / 2; x < pinConfig.MATRIX_W; x++)
			{
				led_set_pixel_xy(x, y, r, g, b);
			}
		}
	}
	else if (strcasecmp(region, "top") == 0)
	{
		for (int y = 0; y < pinConfig.MATRIX_H / 2; y++)
		{
			for (int x = 0; x < pinConfig.MATRIX_W; x++)
			{
				led_set_pixel_xy(x, y, r, g, b);
			}
		}
	}
	else if (strcasecmp(region, "bottom") == 0)
	{
		for (int y = pinConfig.MATRIX_H / 2; y < pinConfig.MATRIX_H; y++)
		{
			for (int x = 0; x < pinConfig.MATRIX_W; x++)
			{
				led_set_pixel_xy(x, y, r, g, b);
			}
		}
	}
	// else unknown => remain dark

	ledShow();
	isOn = (r || g || b);
#endif
}

// ------------------------------------------------
// 8b) Ring segment halves: fill segment across all rings using pinConfig indices
//     For illumination board with 4 concentric rings
//     Quadrant layout (viewing from front):
//       Q1 (top-left)  | Q3 (top-right)
//       ---------------+---------------
//       Q2 (bottom-left)| Q4 (bottom-right)
//
//     Halves:
//       left = Q1 + Q2, right = Q3 + Q4
//       top = Q1 + Q3,  bottom = Q2 + Q4
// ------------------------------------------------
static void set_ring_segment(uint16_t ring_start, uint16_t segment_offset,
                             uint16_t count, uint8_t r, uint8_t g, uint8_t b)
{
	for (uint16_t i = 0; i < count; i++) {
		led_strip_set_pixel(matrix, (uint16_t)(ring_start + segment_offset + i),
		                    led_strip_color(r, g, b));
	}
}

void led_fill_halves_ring_segments(const char *region, uint8_t r, uint8_t g, uint8_t b)
{
	log_i("fillHalvesRingSegments: region=%s, r=%d, g=%d, b=%d", region, r, g, b);

	int doQ1 = 0, doQ2 = 0, doQ3 = 0, doQ4 = 0;

	// Individual quadrants
	if (strcasecmp(region, "q1") == 0)
	{
		doQ1 = 1;
	}
	else if (strcasecmp(region, "q2") == 0)
	{
		doQ2 = 1;
	}
	else if (strcasecmp(region, "q3") == 0)
	{
		doQ3 = 1;
	}
	else if (strcasecmp(region, "q4") == 0)
	{
		doQ4 = 1;
	}
	// Halves (combinations of quadrants)
	else if (strcasecmp(region, "left") == 0)
	{
		// left = Q1 + Q2
		doQ1 = 1;
		doQ2 = 1;
	}
	else if (strcasecmp(region, "right") == 0)
	{
		// right = Q3 + Q4
		doQ3 = 1;
		doQ4 = 1;
	}
	else if (strcasecmp(region, "top") == 0)
	{
		// top = Q1 + Q3
		doQ1 = 1;
		doQ3 = 1;
	}
	else if (strcasecmp(region, "bottom") == 0)
	{
		// bottom = Q2 + Q4
		doQ2 = 1;
		doQ4 = 1;
	}
	// else unknown => remain dark

	if (doQ1) {
		// Q1 is the TOP segment (top-left in our coordinate system)
		set_ring_segment(pinConfig.RING_INNER_START, pinConfig.SEGMENT_INNER_TOP_START, pinConfig.SEGMENT_INNER_COUNT, r, g, b);
		set_ring_segment(pinConfig.RING_MIDDLE_START, pinConfig.SEGMENT_MIDDLE_TOP_START, pinConfig.SEGMENT_MIDDLE_COUNT, r, g, b);
		set_ring_segment(pinConfig.RING_BIGGEST_START, pinConfig.SEGMENT_BIGGEST_TOP_START, pinConfig.SEGMENT_BIGGEST_COUNT, r, g, b);
		set_ring_segment(pinConfig.RING_OUTEST_START, pinConfig.SEGMENT_OUTEST_TOP_START, pinConfig.SEGMENT_OUTEST_COUNT, r, g, b);
	}
	if (doQ2) {
		// Q2 (bottom-left) — LEFT segment
		set_ring_segment(pinConfig.RING_INNER_START, pinConfig.SEGMENT_INNER_LEFT_START, pinConfig.SEGMENT_INNER_COUNT, r, g, b);
		set_ring_segment(pinConfig.RING_MIDDLE_START, pinConfig.SEGMENT_MIDDLE_LEFT_START, pinConfig.SEGMENT_MIDDLE_COUNT, r, g, b);
		set_ring_segment(pinConfig.RING_BIGGEST_START, pinConfig.SEGMENT_BIGGEST_LEFT_START, pinConfig.SEGMENT_BIGGEST_COUNT, r, g, b);
		set_ring_segment(pinConfig.RING_OUTEST_START, pinConfig.SEGMENT_OUTEST_LEFT_START, pinConfig.SEGMENT_OUTEST_COUNT, r, g, b);
	}
	if (doQ3) {
		// Q3 (top-right) — RIGHT segment
		set_ring_segment(pinConfig.RING_INNER_START, pinConfig.SEGMENT_INNER_RIGHT_START, pinConfig.SEGMENT_INNER_COUNT, r, g, b);
		set_ring_segment(pinConfig.RING_MIDDLE_START, pinConfig.SEGMENT_MIDDLE_RIGHT_START, pinConfig.SEGMENT_MIDDLE_COUNT, r, g, b);
		set_ring_segment(pinConfig.RING_BIGGEST_START, pinConfig.SEGMENT_BIGGEST_RIGHT_START, pinConfig.SEGMENT_BIGGEST_COUNT, r, g, b);
		set_ring_segment(pinConfig.RING_OUTEST_START, pinConfig.SEGMENT_OUTEST_RIGHT_START, pinConfig.SEGMENT_OUTEST_COUNT, r, g, b);
	}
	if (doQ4) {
		// Q4 (bottom-right) — BOTTOM segment
		set_ring_segment(pinConfig.RING_INNER_START, pinConfig.SEGMENT_INNER_BOTTOM_START, pinConfig.SEGMENT_INNER_COUNT, r, g, b);
		set_ring_segment(pinConfig.RING_MIDDLE_START, pinConfig.SEGMENT_MIDDLE_BOTTOM_START, pinConfig.SEGMENT_MIDDLE_COUNT, r, g, b);
		set_ring_segment(pinConfig.RING_BIGGEST_START, pinConfig.SEGMENT_BIGGEST_BOTTOM_START, pinConfig.SEGMENT_BIGGEST_COUNT, r, g, b);
		set_ring_segment(pinConfig.RING_OUTEST_START, pinConfig.SEGMENT_OUTEST_BOTTOM_START, pinConfig.SEGMENT_OUTEST_COUNT, r, g, b);
	}

	ledShow();
	isOn = (r || g || b);
}

// ------------------------------------------------
// 9) RINGS: for demonstration, fill a big circle,
//           then carve out the interior to simulate a ring
//           or you can do multiple concentric rings, etc.
//           For illumination board with discrete rings, use direct indexing
// ------------------------------------------------
void led_draw_rings(uint8_t radius, uint8_t r, uint8_t g, uint8_t b)
{
	log_i("drawRings: radius=%d, r=%d, g=%d, b=%d", radius, r, g, b);
#ifndef HUB75
	// Check if this is an illumination board with defined ring structure
	#ifdef LED_CONTROLLER
	// Check if we have ring definitions in the pin config (illumination board)
	if (pinConfig.pindefName &&
		(strcmp(pinConfig.pindefName, "seeed_xiao_esp32s3_can_slave_illumination") == 0 ||
		strcmp(pinConfig.pindefName, "UC2_canopen_slave_led") == 0))
	{
		// Use direct ring indexing for illumination board
		led_draw_illumination_rings(radius, r, g, b);
		return;
	}
	#endif

	// Default matrix-based ring drawing for other configurations
	// The simplest approach to get a ring:
	// 1) Fill circle of radius
	// 2) Fill circle of radius-1 in black
	// => That leaves a 1-pixel wide ring
	led_strip_clear(matrix);

	// fill circle
	float cx = (pinConfig.MATRIX_W - 1) / 2.0;
	float cy = (pinConfig.MATRIX_H - 1) / 2.0;
	for (int y = 0; y < pinConfig.MATRIX_H; y++)
	{
		for (int x = 0; x < pinConfig.MATRIX_W; x++)
		{
			float dx = x - cx;
			float dy = y - cy;
			float dist = sqrtf(dx * dx + dy * dy);
			if (dist <= (float)radius)
			{
				led_set_pixel_xy(x, y, r, g, b);
			}
		}
	}

	// carve out inside => smaller circle in black
	if (radius > 0)
	{
		for (int y = 0; y < pinConfig.MATRIX_H; y++)
		{
			for (int x = 0; x < pinConfig.MATRIX_W; x++)
			{
				float dx = x - cx;
				float dy = y - cy;
				float dist = sqrtf(dx * dx + dy * dy);
				if (dist <= (float)(radius - 1))
				{
					led_set_pixel_xy(x, y, 0, 0, 0);
				}
			}
		}
	}

	ledShow();
	isOn = (r || g || b);
#endif
}

// ------------------------------------------------
// 9b) ILLUMINATION RINGS: Handle discrete ring addressing
//     for the UC2 illumination board with 4 concentric rings
// ------------------------------------------------
void led_draw_illumination_rings(uint8_t ring_id, uint8_t r, uint8_t g, uint8_t b)
{
	log_i("drawIlluminationRings: ring_id=%d, r=%d, g=%d, b=%d", ring_id, r, g, b);

	// Hard-coded ring mapping for illumination board
	// Ring mapping: radius 0=inner, 1=middle, 2=biggest, 3=outest
	uint16_t start_idx = 0;
	uint16_t count = 0;

	switch (ring_id) {
		case 0: // Inner ring
			start_idx = pinConfig.RING_INNER_START;
			count = pinConfig.RING_INNER_COUNT;
			break;
		case 1: // Middle ring
			start_idx = pinConfig.RING_MIDDLE_START;
			count = pinConfig.RING_MIDDLE_COUNT;
			break;
		case 2: // Biggest ring
			start_idx = pinConfig.RING_BIGGEST_START;
			count = pinConfig.RING_BIGGEST_COUNT;
			break;
		case 3: // Outest ring
			start_idx = pinConfig.RING_OUTEST_START;
			count = pinConfig.RING_OUTEST_COUNT;
			break;
		default: {
			// Invalid ring, light up all rings
			uint16_t total_leds = pinConfig.RING_OUTEST_START + pinConfig.RING_OUTEST_COUNT;
			for (uint16_t i = 0; i < total_leds; i++) {
				led_strip_set_pixel(matrix, i, led_strip_color(r, g, b));
			}
			ledShow();
			isOn = (r || g || b);
			return;
		}
	}

	// Clear all LEDs first
	led_strip_clear(matrix);

	// Set the specified ring
	for (uint16_t i = 0; i < count; i++) {
		led_strip_set_pixel(matrix, (uint16_t)(start_idx + i), led_strip_color(r, g, b));
		log_i("Setting LED at index %d (ring %d) to color: R=%d, G=%d, B=%d", (int)(start_idx + i), ring_id, r, g, b);
	}

	ledShow();
	isOn = (r || g || b);
}

// ------------------------------------------------
// 9c) ILLUMINATION RING SEGMENTS: Handle ring segments (left/right/top/bottom)
//     for the UC2 illumination board
// ------------------------------------------------
void led_draw_illumination_ring_segment(uint8_t ring_id, const char *region, uint8_t r, uint8_t g, uint8_t b, bool clear_before_draw)
{

	// Get ring parameters
	uint16_t start_idx = 0;
	uint16_t count = 0;
	log_i("drawIlluminationRingSegment: ring_id=%d, region=%s, r=%d, g=%d, b=%d", ring_id, region, r, g, b);
	switch (ring_id) {
		case 0: start_idx = pinConfig.RING_INNER_START; count = pinConfig.RING_INNER_COUNT; break;   // Inner ring
		case 1: start_idx = pinConfig.RING_MIDDLE_START; count = pinConfig.RING_MIDDLE_COUNT; break;  // Middle ring
		case 2: start_idx = pinConfig.RING_BIGGEST_START; count = pinConfig.RING_BIGGEST_COUNT; break;  // Biggest ring
		case 3: start_idx = pinConfig.RING_OUTEST_START; count = pinConfig.RING_OUTEST_COUNT; break;  // Outest ring
		default: return; // Invalid ring
	}

	// Clear all LEDs first
	if (clear_before_draw)
		led_strip_clear(matrix);

	// Calculate segment within the ring (relative to ring start)
	uint16_t relative_start = 0;
	uint16_t segment_count = count / 2; // Half ring for each segment

	if (strcasecmp(region, "top") == 0) {
		relative_start = 0;                 // Start at beginning of ring
	}
	else if (strcasecmp(region, "right") == 0) {
		relative_start = count / 4;         // 25% into the ring
	}
	else if (strcasecmp(region, "bottom") == 0) {
		relative_start = count / 2;         // 50% into the ring
	}
	else if (strcasecmp(region, "left") == 0) {
		relative_start = (3 * count) / 4;   // 75% into the ring
	}
	else {
		// Unknown region, light up whole ring
		relative_start = 0;
		segment_count = count;
	}

	// Set the specified ring segment with proper wraparound
	for (uint16_t i = 0; i < segment_count; i++) {
		uint16_t led_idx = (uint16_t)(start_idx + ((relative_start + i) % count));
		led_strip_set_pixel(matrix, led_idx, led_strip_color(r, g, b));
	}

	ledShow();
	isOn = (r || g || b);
}

// ------------------------------------------------
// 10) Filled circle
// ------------------------------------------------
void led_draw_circle(uint8_t radius, uint8_t rVal, uint8_t gVal, uint8_t bVal)
{
	log_i("drawCircle: radius=%d, r=%d, g=%d, b=%d", radius, rVal, gVal, bVal);
#ifndef HUB75
	led_strip_clear(matrix);

	// draw LEDs from 0 to ring segment count for a filled circle
	for (int iRing = 0; iRing < (int)radius; iRing++) {
		// For each ring, draw the full segment (all LEDs in that ring)
		led_draw_illumination_ring_segment((uint8_t)iRing, "None", rVal, gVal, bVal, false);
	}

	ledShow();
	isOn = (rVal || gVal || bVal);
#endif
}

// ------------------------------------------------
// 11) Parse cJSON into LedCommand struct
// ------------------------------------------------
bool led_parse_command(cJSON *root, LedCommand *cmd)
{
	/*
	Expect a top-level "task" == "/ledarr_act" plus "qid",
	then a nested "led" object with the actual LED parameters.
	E.g.:
	{ "task": "/ledarr_act", "qid": 17, "led": { "LEDArrMode": 1, "action": "rings", "region": "left", "radius": 4, "r": 255, "g": 255, "b": 255, "ledIndex": 12, "led_array": [ { "id": 0, "r": 255, "g": 255, "b": 0 }, { "id": 5, "r": 128, "g": 0,   "b": 128 } ] } }
	{ "task": "/ledarr_act", "qid": 17, "led": { "action": "off" } }
	{ "task": "/ledarr_act", "qid": 17, "led": { "action": "fill", "r": 255, "g": 255, "b": 255 } }
	{ "task": "/ledarr_act", "qid": 17, "led": { "action": "single", "ledIndex": 12, "r": 255, "g": 0, "b": 0 } }
	{ "task": "/ledarr_act", "qid": 17, "led": { "action": "halves", "region": "left", "r": 255, "g": 255, "b": 255 } }
	{ "task": "/ledarr_act", "qid": 17, "led": { "action": "halves", "region": "right", "r": 15, "g": 15, "b": 15} }
	{ "task": "/ledarr_act", "qid": 17, "led": { "action": "halves", "region": "top", "r": 15, "g": 15, "b": 15} }
	{ "task": "/ledarr_act", "qid": 17, "led": { "action": "halves", "region": "bottom", "r": 15, "g": 15, "b": 15} }
	{ "task": "/ledarr_act", "qid": 17, "led": { "action": "halves", "region": "q1", "r": 15, "g": 15, "b": 15} }
	{ "task": "/ledarr_act", "qid": 17, "led": { "action": "halves", "region": "q2", "r": 15, "g": 15, "b": 15} }
	{ "task": "/ledarr_act", "qid": 17, "led": { "action": "halves", "region": "q3", "r": 15, "g": 15, "b": 15} }
	{ "task": "/ledarr_act", "qid": 17, "led": { "action": "halves", "region": "q4", "r": 15, "g": 15, "b": 15} }
	{ "task": "/ledarr_act", "qid": 17, "led": { "action": "rings", "radius": 3, "r": 255, "g": 255, "b": 255 } }
	{ "task": "/ledarr_act", "qid": 17, "led": { "action": "circles", "radius": 2, "r": 255, "g": 255, "b": 255 } }
	{ "task": "/ledarr_act", "qid": 17, "led": { "action": "status", "status":"idle" } }
	{ "task": "/ledarr_act", "qid": 17, "led"}
	{"task": "/laser_act", "LASERid":4, "LASERval": 1000, "qid":5}

	*/
	// 1) Check for "task"
	cJSON *task = cJSON_GetObjectItem(root, "task");
	if (!task || strcmp(task->valuestring, "/ledarr_act") != 0)
	{
		log_e("parseLedCommand: Invalid task: %s", task ? task->valuestring : "NULL");
		// Not our command structure
		return false;
	}

	// 2) Extract qid (if present)
	cmd->qid = 0;
	cJSON *jqid = cJSON_GetObjectItem(root, "qid");
	if (jqid && cJSON_IsNumber(jqid))
	{
		cmd->qid = (uint16_t)jqid->valueint;
	}

	// 3) Look for the "led" object
	cJSON *ledObj = cJSON_GetObjectItem(root, "led");
	if (!ledObj || !cJSON_IsObject(ledObj))
	{
		log_e("parseLedCommand: No 'led' object found");
		// If there's no "led" object, we can't parse further
		return false;
	}

	// 4) Read LEDArrMode from "led" if needed
	//    (Might map to an internal mode or just store it.)
	cJSON *jMode = cJSON_GetObjectItem(ledObj, "LEDArrMode");
	if (jMode && cJSON_IsNumber(jMode))
	{
		int modeVal = jMode->valueint;
		log_i("parseLedCommand: LEDArrMode: %d", modeVal);
		// If you want to store it somewhere:
		// cmd.ledArrMode = modeVal;
	}

	// 5) Decide the LedMode from "action"
	cmd->mode = LM_UNKNOWN;
	cJSON *jaction = cJSON_GetObjectItem(ledObj, "action");
	if (jaction && jaction->valuestring)
	{
		log_i("ParseLedCommand: action: %s", jaction->valuestring);
		char actBuf[32];
		str_lower_into(actBuf, sizeof(actBuf), jaction->valuestring);
		if (strcmp(actBuf, "rings") == 0)
		{
			cmd->mode = LM_RINGS;
		}
		else if (strcmp(actBuf, "circles") == 0)
		{
			cmd->mode = LM_CIRCLE;
		}
		else if (strcmp(actBuf, "halves") == 0)
		{
			cmd->mode = LM_HALVES;
		}
		else if (strcmp(actBuf, "fill") == 0)
		{
			cmd->mode = LM_FILL;
		}
		else if (strcmp(actBuf, "on") == 0)
		{
			cmd->mode = LM_FILL;
		}
		else if (strcmp(actBuf, "single") == 0)
		{
			cmd->mode = LM_SINGLE;
		}
		else if (strcmp(actBuf, "off") == 0)
		{
			cmd->mode = LM_OFF;
		}
		else if (strcmp(actBuf, "array") == 0)
		{
			cmd->mode = LM_ARRAY;
		}
		else if (strcmp(actBuf, "status") == 0)
		{
			cmd->mode = LM_STATUS;
		}
		else if (strcmp(actBuf, "unknown") == 0)
		{
			cmd->mode = LM_UNKNOWN;
		}
		else
		{
			log_e("parseLedCommand: Unknown action: %s", actBuf);
			return false; // Invalid action
		}
		// else remains UNKNOWN
	}

	// 6) Parse color (r, g, b)
	cmd->r = 0;
	cmd->g = 0;
	cmd->b = 0;
	cJSON *jr = cJSON_GetObjectItem(ledObj, "r");
	cJSON *jg = cJSON_GetObjectItem(ledObj, "g");
	cJSON *jb = cJSON_GetObjectItem(ledObj, "b");
	if (jr && cJSON_IsNumber(jr))
	{
		cmd->r = (uint8_t)jr->valueint;
	}
	if (jg && cJSON_IsNumber(jg))
	{
		cmd->g = (uint8_t)jg->valueint;
	}
	if (jb && cJSON_IsNumber(jb))
	{
		cmd->b = (uint8_t)jb->valueint;
	}

	// 7) Parse radius (for circles/rings)
	cmd->radius = 0;
	cJSON *jradius = cJSON_GetObjectItem(ledObj, "radius");
	if (jradius && cJSON_IsNumber(jradius))
	{
		log_i("parseLedCommand: radius: %d", jradius->valueint);
		cmd->radius = (uint8_t)jradius->valueint;
	}

	// 8) Parse region ("left", "right", "top", "bottom")
	memset(cmd->region, 0, sizeof(cmd->region));
	cJSON *jregion = cJSON_GetObjectItem(ledObj, "region");
	if (jregion && jregion->valuestring)
	{
		strncpy(cmd->region, jregion->valuestring, sizeof(cmd->region) - 1);
	}

	// 9) Parse single LED index
	cmd->ledIndex = 0;
	cJSON *jledIndex = cJSON_GetObjectItem(ledObj, "ledIndex");
	if (jledIndex && cJSON_IsNumber(jledIndex))
	{
		log_i("parseLedCommand: ledIndex: %d", jledIndex->valueint);
		cmd->ledIndex = (uint16_t)jledIndex->valueint;
	}

	// 10) If mode == ARRAY, parse "led_array"
	//     (not fully implemented here, just an example)
	cJSON *ledarr = cJSON_GetObjectItem(ledObj, "led_array");
	if (ledarr && cJSON_IsArray(ledarr))
	{
		// You can iterate over each item in the array:
		// cJSON *item = NULL;
		// cJSON_ArrayForEach(item, ledarr)
		// {
		//   int id   = cJSON_GetObjectItem(item, "id")->valueint;
		//   int rr   = cJSON_GetObjectItem(item, "r")->valueint;
		//   int gg   = cJSON_GetObjectItem(item, "g")->valueint;
		//   int bb   = cJSON_GetObjectItem(item, "b")->valueint;
		//   ...
		// }
	}

	// 11) If mode == STATUS, you can add more status info here
	//     (e.g. we want to address e.g. red, green, blue, rainbow for status on the FRAME )
	if (cmd->mode == LM_STATUS)
	{
		// {"task": "/ledarr_act", "qid": 17, "led": { "action": "status", "status":"idle" } }
		// {"task": "/ledarr_act", "qid": 17, "led": { "action": "status", "status":"error" } }
		// {"task": "/ledarr_act", "qid": 17, "led": { "action": "status", "status":"rainbow" } }
		// {"task": "/ledarr_act", "qid": 17, "led": { "action": "status", "status":"on" } }
		// {"task": "/ledarr_act", "qid": 17, "led": { "action": "status", "status":"off" } }
		// we parse the incoming status to LedForStatus
		// and then we can set the color of the LED in the loop to e.g. rainbow, red/green/blue glowing
		log_i("parseLedCommand: Parsing status for STATUS mode");
		cJSON *jstatus = cJSON_GetObjectItem(ledObj, "status");
		if (jstatus && jstatus->valuestring)
		{
			char statusBuf[32];
			str_lower_into(statusBuf, sizeof(statusBuf), jstatus->valuestring);
			if (strcmp(statusBuf, "warn") == 0)
			{
				currentLedForStatus = warn;
			}
			else if (strcmp(statusBuf, "error") == 0)
			{
				currentLedForStatus = error;
			}
			else if (strcmp(statusBuf, "idle") == 0)
			{
				currentLedForStatus = idle;
			}
			else if (strcmp(statusBuf, "success") == 0)
			{
				currentLedForStatus = success;
			}
			else if (strcmp(statusBuf, "busy") == 0)
			{
				currentLedForStatus = busy;
			}
			else if (strcmp(statusBuf, "rainbow") == 0)
			{
				currentLedForStatus = rainbow;
			}
			else if (strcmp(statusBuf, "off") == 0)
			{
				currentLedForStatus = off_;
			}
			else if (strcmp(statusBuf, "on") == 0)
			{
				currentLedForStatus = on_;
			}
			else
			{
				log_e("parseLedCommand: Unknown status: %s", statusBuf);
				currentLedForStatus = unknown;
				return false; // Invalid status
			}
			log_i("parseLedCommand: Set currentLedForStatus to %d", (int)currentLedForStatus);
		}
	}

	return true; // Successfully parsed
}

// Refuse to light anything up while the heat-sink is over temperature.
// Guards every entry point that can turn LEDs on (serial and CANopen) —
// without this the host would simply re-send its last command after the
// thermal cut and cook the board anyway. turnOff() deliberately stays
// reachable.
static bool thermalBlocked(const char *what)
{
#ifdef THERMAL_CONTROLLER
	if (thermal_is_led_cut_active())
	{
		log_w("%s ignored - over-temperature protection active", what);
		return true;
	}
#endif
	return false;
}

// ------------------------------------------------
// 11b) CANopen: Set LED mode from OD values
// ------------------------------------------------
void led_set_mode(uint8_t mode, uint8_t brightness, uint32_t colour)
{
	if (thermalBlocked("setMode")) return;
	log_i("setMode: mode=%d, brightness=%d, colour=0x%06X", mode, brightness, colour);
	// check if matrix is initialized (most likely when do setup)
	if (!matrix)		{
		log_e("setMode: LED matrix not initialized");
		return;
	}

	// Disable pattern animation when a mode is explicitly set
	activePatternId = 0;
	// Only apply brightness when explicitly non-zero. Brightness=0 (the OD
	// default when the master never wrote x2201_led_brightness via SDO)
	// would permanently zero Adafruit_NeoPixel's global brightness scaler,
	// making all subsequent ledShow() calls render black.
	if (brightness > 0) led_strip_set_brightness(matrix, brightness);

	// Build a LedCommand and dispatch through led_exec_command so the same
	// thermal protection / mode handling logic applies as for JSON input.
	LedCommand cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.qid = 0;
	cmd.r = (uint8_t)((colour >> 16) & 0xFF);
	cmd.g = (uint8_t)((colour >> 8)  & 0xFF);
	cmd.b = (uint8_t)((colour)       & 0xFF);
	cmd.radius = 0;
	cmd.ledIndex = 0;
	cmd.region[0] = '\0';

	switch (mode) {
		case 0: cmd.mode = LM_OFF;    break;
		case 1: cmd.mode = LM_FILL;   break;
		case 2: cmd.mode = LM_HALVES; strncpy(cmd.region, "left",   sizeof(cmd.region) - 1); break;
		case 3: cmd.mode = LM_HALVES; strncpy(cmd.region, "right",  sizeof(cmd.region) - 1); break;
		case 4: cmd.mode = LM_HALVES; strncpy(cmd.region, "top",    sizeof(cmd.region) - 1); break;
		case 5: cmd.mode = LM_HALVES; strncpy(cmd.region, "bottom", sizeof(cmd.region) - 1); break;
		default: cmd.mode = LM_FILL;  break;
	}
	led_exec_command(&cmd);
}

// ------------------------------------------------
// 11c) CANopen: Set LED pattern
// ------------------------------------------------
void led_set_pattern(uint8_t patternId, uint16_t speed)
{
	if (thermalBlocked("setPattern")) return;
	log_i("setPattern: patternId=%d, speed=%d", patternId, speed);
	// check if matrix is initialized (most likely when do setup)
	if (!matrix)		{
		log_e("setMode: LED matrix not initialized");
		return;
	}


	activePatternId = patternId;
	activePatternSpeed = (speed > 0) ? speed : 50;
	patternFrame = 0;
	lastPatternUpdateMs = 0;
	log_i("LED pattern set: id=%d speed=%d", patternId, speed);
}

// ------------------------------------------------
// 11d) CANopen: Bulk set pixels from RGB buffer
// ------------------------------------------------
void led_set_pixels(const uint8_t *data, uint16_t pixelCount)
{
	if (thermalBlocked("setPixels")) return;
	log_i("setPixels: pixelCount=%d", pixelCount);
	// check if matrix is initialized (most likely when do setup)
	if (!matrix)		{
		log_e("setMode: LED matrix not initialized");
		return;
	}

	// Disable pattern animation when pixel data is set directly
	activePatternId = 0;

	uint16_t count = (pixelCount > LED_COUNT) ? LED_COUNT : pixelCount;
	for (uint16_t i = 0; i < count; i++) {
		uint8_t r = data[i * 3 + 0];
		uint8_t g = data[i * 3 + 1];
		uint8_t b = data[i * 3 + 2];
		led_strip_set_pixel(matrix, i, led_strip_color(r, g, b));
	}
	ledShow();
	isOn = (count > 0);
	log_i("setPixels: %d pixels applied", count);
}

// ------------------------------------------------
// 12) Execute a LedCommand
// ------------------------------------------------
void led_exec_command(const LedCommand *cmd)
{
	if (thermalBlocked("execLedCommand")) return;
	log_i("execLedCommand: Executing command with mode %d", (int)cmd->mode);

	// Track intensity for auto-off safety (only for LED arrays that need thermal protection)
	// Check if this is a device that requires thermal protection
	bool needsThermalProtection = (pinConfig.pindefName &&
		(strcmp(pinConfig.pindefName, "waveshare_esp32s3_ledarray") == 0 ||
		 strcmp(pinConfig.pindefName, "seeed_xiao_esp32s3_can_slave_illumination") == 0 ||
		 strcmp(pinConfig.pindefName, "UC2_canopen_slave_led") == 0 ||
		 strcmp(pinConfig.pindefName, "waveshare_esp32s3_ledarray") == 0 ));

	if (needsThermalProtection)
	{
		// Calculate total intensity as sum of all RGB channels for accurate thermal calculation
		// This better represents actual power/heat than just the max channel
		uint16_t totalIntensity = (uint16_t)cmd->r + (uint16_t)cmd->g + (uint16_t)cmd->b;
		currentTotalIntensity = totalIntensity;

		if (totalIntensity > pinConfig.LED_AUTO_OFF_INTENSITY_THRESHOLD)
		{
			if (highIntensityStartTime == 0)
			{
				// First time exceeding threshold
				highIntensityStartTime = millis();
				highIntensityWarningShown = false;
				ledAutoOffTriggered = false;
				log_w("LED total intensity %d (R:%d+G:%d+B:%d) exceeds threshold %d - auto-off timer started",
					  totalIntensity, cmd->r, cmd->g, cmd->b, pinConfig.LED_AUTO_OFF_INTENSITY_THRESHOLD);
			}
		}
		else
		{
			// Intensity dropped below threshold, reset timer
			if (highIntensityStartTime != 0)
			{
				log_i("LED total intensity dropped to %d - auto-off timer reset", totalIntensity);
			}
			highIntensityStartTime = 0;
			highIntensityWarningShown = false;
			ledAutoOffTriggered = false;
		}
	}

	switch (cmd->mode)
	{
	case LM_OFF:
		led_turn_off();
		// Reset auto-off tracking when LEDs are turned off
		if (needsThermalProtection)
		{
			highIntensityStartTime = 0;
			highIntensityWarningShown = false;
			ledAutoOffTriggered = false;
			currentTotalIntensity = 0;
		}
		break;
	case LM_FILL:
		led_fill_all(cmd->r, cmd->g, cmd->b);
		break;
	case LM_SINGLE:
		led_set_single(cmd->ledIndex, cmd->r, cmd->g, cmd->b);
		break;
	case LM_HALVES:
		led_fill_halves(cmd->region, cmd->r, cmd->g, cmd->b);
		break;
	case LM_RINGS:
		// For illumination board, check if region is specified for ring segments
		if (pinConfig.pindefName &&
			(strcmp(pinConfig.pindefName, "seeed_xiao_esp32s3_can_slave_illumination") == 0 ||
			strcmp(pinConfig.pindefName, "UC2_canopen_slave_led")==0) &&
			strlen(cmd->region) > 0)
		{
			led_draw_illumination_ring_segment(cmd->radius, cmd->region, cmd->r, cmd->g, cmd->b, false);
		}
		else
		{
			led_draw_rings(cmd->radius, cmd->r, cmd->g, cmd->b);
		}
		break;
	case LM_CIRCLE:
		led_draw_circle(cmd->radius, cmd->r, cmd->g, cmd->b);
		break;
	case LM_ARRAY:
		// TODO: Here you could parse an array of LED changes.
		// For brevity, not implemented in detail.
		// e.g. cJSON_GetObjectItem(root,"led_array")...
		// Then set each LED individually
		// ledShow();
		break;
	case LM_STATUS:
		// Status handling is done in the loop() for dynamic effects
		// Just log here
		log_i("execLedCommand: Set status to %d", (int)currentLedForStatus);
		break;
	default:
		// Unknown => do nothing or turn off
		break;
	}
}

// ------------------------------------------------
// 13) The main 'act' function that the user calls
//     to pass cJSON with LED commands
// ------------------------------------------------
int led_act(cJSON *root)
{
	LedCommand cmd;
	memset(&cmd, 0, sizeof(cmd));
	if (!led_parse_command(root, &cmd))
	{
		// Invalid or missing "task": "/led_arr"
		return -1;
	}
	led_exec_command(&cmd);


	// LED is synchronous: register and immediately report done
	if (cmd.qid > 0)
	{
		qid_register(cmd.qid, 1, QID_DEFAULT_TIMEOUT_MS); // 3-arg: C has no default-arg overload
		qid_report_action_done(cmd.qid);
	}

	return (int)cmd.qid; // return the same QID
}

// ------------------------------------------------
// 14) Optional 'get' function
//     e.g. returns current state in JSON
// ------------------------------------------------
cJSON *led_get(cJSON *root)
{
	// Example:
	// {"task": "/ledarr_get", "qid": 17, "led": { "isOn": true, "count": 64 }}
	(void)root;
	cJSON *j = cJSON_CreateObject();
	cJSON *ld = cJSON_CreateObject();
	cJSON_AddItemToObject(j, "led", ld);

	cJSON_AddBoolToObject(ld, "isOn", isOn);
	cJSON_AddNumberToObject(ld, "count", (double)LED_COUNT);

	// Could add more info about the last command etc.
	return j;
}



void led_circle_changed_event(int pressed)
{
	if (pressed)
	{
		unsigned long currentTime = millis();

		// Debounce check - ignore if too soon after last event
		if (currentTime - circleLastEventTime < BUTTON_DEBOUNCE_TIME)
		{
			return;
		}

		circleLastEventTime = currentTime;

		// Toggle LED state
		ledToggleState = !ledToggleState;

		log_i("Circle pressed - LED toggle to %s", ledToggleState ? "ON" : "OFF");
		// TODO: Shall we route through dictionaries? Much slower probably?
		cJSON* doc = cJSON_CreateObject();
		cJSON_AddStringToObject(doc, "task", "/ledarr_act");
		cJSON* led = cJSON_CreateObject();
		if (ledToggleState)
		{
			// Turn LED ON via circles mode
			cJSON_AddStringToObject(led, "action", "circles");
			cJSON_AddNumberToObject(led, "r", 0);
			cJSON_AddNumberToObject(led, "g", 255);
			cJSON_AddNumberToObject(led, "b", 0);
			cJSON_AddNumberToObject(led, "radius", 8);
			isOn = true;
		}
		else
		{
			// Turn LED OFF
			cJSON_AddStringToObject(led, "action", "off");
			isOn = false;
		}
		cJSON_AddItemToObject(doc, "led", led);
		cJSON* resp = device_router_handle_led_act(doc);
		if (resp) cJSON_Delete(resp);
		cJSON_Delete(doc);
	}
}

void led_loop(void)
{
	// LED auto-off safety: Monitor high intensity LEDs and auto-shut off to prevent overheating
	// Check if this is a device that requires thermal protection
	bool needsThermalProtection = (pinConfig.pindefName &&
		(strcmp(pinConfig.pindefName, "waveshare_esp32s3_ledarray") == 0 ||
		 strcmp(pinConfig.pindefName, "seeed_xiao_esp32s3_can_slave_illumination") == 0 ||
		 strcmp(pinConfig.pindefName, "UC2_canopen_slave_led") == 0));

#ifdef THERMAL_CONTROLLER
	// Measured heat-sink temperature supersedes this stopwatch entirely.
	// The intensity timer only ever guessed at heat from brightness and
	// elapsed time — with NTCs fitted it would cut perfectly cool LEDs.
	if (thermal_is_enabled())
		needsThermalProtection = false;
#endif

	if (needsThermalProtection && highIntensityStartTime != 0 && !ledAutoOffTriggered)
	{
		unsigned long elapsedTime = millis() - highIntensityStartTime;

		// Show warning at 75% of auto-off time
		if (!highIntensityWarningShown && elapsedTime >= (unsigned long)(pinConfig.LED_AUTO_OFF_TIME_MS * 3 / 4))
		{
			highIntensityWarningShown = true;
			log_w("WARNING: LED total intensity %d has been high for %lu ms. Auto-off in %lu ms to prevent overheating!",
				  currentTotalIntensity, elapsedTime, (unsigned long)(pinConfig.LED_AUTO_OFF_TIME_MS - elapsedTime));
#ifndef UNIT_TEST
			Serial_println("+|");
			Serial_println("{\"led\":{\"warning\":\"High intensity detected - hardware can overheat and be destroyed!\"}}");
			Serial_println("--");
#endif
		}

		// Auto-off when time limit exceeded
		if (elapsedTime >= (unsigned long)pinConfig.LED_AUTO_OFF_TIME_MS)
		{
			ledAutoOffTriggered = true;
			log_e("LED AUTO-OFF TRIGGERED! Total intensity %d exceeded %d threshold for %lu ms - shutting down LEDs to prevent damage",
				  currentTotalIntensity, pinConfig.LED_AUTO_OFF_INTENSITY_THRESHOLD, elapsedTime);

			// Turn off LEDs for safety
			led_turn_off();

			// Send warning message to user
#ifndef UNIT_TEST
			Serial_println("++");
			Serial_println("{\"led\":{\"error\":\"AUTO-OFF: LEDs too hot! Hardware protection activated. Reduce intensity or wait before restarting.\"}}");
			Serial_println("--");
#endif

			// Reset tracking (user must send new command to restart)
			highIntensityStartTime = 0;
			currentTotalIntensity = 0;
		}
	}

	// Pattern animation (CANopen-driven patterns)
	if (activePatternId > 0)
	{
		uint32_t now = millis();
		if (now - lastPatternUpdateMs >= activePatternSpeed) {
			lastPatternUpdateMs = now;
			patternFrame++;
			switch (activePatternId) {
				case 1: { // Rainbow
					for (uint16_t i = 0; i < LED_COUNT; i++) {
						uint16_t hue = ((uint16_t)patternFrame * 256 + i * 65536 / LED_COUNT) & 0xFFFF;
						uint32_t c = led_strip_gamma32(led_strip_color_hsv(hue, 255, 255));
						led_strip_set_pixel(matrix, i, c);
					}
					ledShow();
					break;
				}
				case 2: { // Breathe (white)
					uint8_t br = (patternFrame < 128) ? (uint8_t)(patternFrame * 2) : (uint8_t)((255 - patternFrame) * 2);
					for (uint16_t i = 0; i < LED_COUNT; i++)
						led_strip_set_pixel(matrix, i, led_strip_color(br, br, br));
					ledShow();
					break;
				}
				case 3: { // Chase
					led_strip_clear(matrix);
					uint16_t pos = (uint16_t)(patternFrame % LED_COUNT);
					for (uint8_t t = 0; t < 3; t++) {
						uint16_t idx = (uint16_t)((pos + t * LED_COUNT / 3) % LED_COUNT);
						led_strip_set_pixel(matrix, idx, led_strip_color(255, 255, 255));
					}
					ledShow();
					break;
				}
				case 4: { // Fire
					for (uint16_t i = 0; i < LED_COUNT; i++) {
						uint8_t heat = (uint8_t)uc2_random(100, 255);
						led_strip_set_pixel(matrix, i, led_strip_color(heat, (uint8_t)(heat / 3), 0));
					}
					ledShow();
					break;
				}
				case 5: { // Sparkle
					led_strip_clear(matrix);
					for (uint8_t s = 0; s < 3; s++) {
						uint16_t idx = (uint16_t)uc2_random(0, LED_COUNT);
						led_strip_set_pixel(matrix, idx, led_strip_color(255, 255, 255));
					}
					ledShow();
					break;
				}
				case 6: { // Heatmap (gradient red->yellow->white)
					for (uint16_t i = 0; i < LED_COUNT; i++) {
						uint8_t t = (uint8_t)(((uint16_t)patternFrame + i * 4) & 0xFF);
						uint8_t r = 255;
						uint8_t g = (t < 128) ? (uint8_t)(t * 2) : 255;
						uint8_t b = (t < 128) ? 0 : (uint8_t)((t - 128) * 2);
						led_strip_set_pixel(matrix, i, led_strip_color(r, g, b));
					}
					ledShow();
					break;
				}
				case 7: { // Spiral
					led_strip_clear(matrix);
					for (uint8_t t = 0; t < 5; t++) {
						uint16_t idx = (uint16_t)((patternFrame + t * 3) % LED_COUNT);
						uint16_t hue = (uint16_t)(t * 65536 / 5);
						uint32_t c = led_strip_gamma32(led_strip_color_hsv(hue, 255, 255));
						led_strip_set_pixel(matrix, idx, c);
					}
					ledShow();
					break;
				}
				default:
					break;
			}
		}
	}

	// Status-indicator handling has moved to SignalController (/signal_act).
	// LedController is now responsible only for the (multi-pixel) LED array
	// driven via /ledarr_act and CANopen pattern animations.

}

#ifdef HUB75
static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
	return led_strip_color565(matrix, r, g, b);
}
#endif

// C-facing shims consumed by the pure-C ThermalController (led cut on
// over-temperature) and FocusScan (focus-stack circle highlight).
// Was: LedCommand{CIRCLE, r=g=b=rgb, radius, qid} via
// LedController::execLedCommand.
void led_c_turn_off(void)
{
	led_turn_off();
}

void led_exec_circ_command(uint8_t rgb, uint8_t radius, uint8_t qid)
{
	LedCommand cmd;
	memset(&cmd, 0, sizeof(cmd));
	cmd.qid = (uint16_t)qid;
	cmd.mode = LM_CIRCLE;
	cmd.r = rgb;
	cmd.g = rgb;
	cmd.b = rgb;
	cmd.radius = radius;
	led_exec_command(&cmd);
}

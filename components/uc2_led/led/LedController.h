#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

// T4.1: converted from C++ (namespace LedController) to C.
// The Adafruit library objects live behind the led_strip_c.h shim
// (led/led_strip_c.cpp); this file is pure C.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <PinConfig.h>

#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LED_COUNT (pinConfig.MATRIX_W * pinConfig.MATRIX_H)

// LED modes (values pinned by test LDC-001). Was `enum class LedMode : uint8_t`
// in C++; the LM_ prefix replaces the scoped `LedMode::` access.
typedef enum {
  LM_OFF = 0,
  LM_FILL,
  LM_SINGLE,
  LM_HALVES,
  LM_RINGS,
  LM_CIRCLE,
  LM_ARRAY, // for multiple arbitrary pixels
  LM_STATUS,
  LM_UNKNOWN,
} LedMode;

// Plain C enums (unscoped) — values unchanged from the C++ version.
typedef enum {
  warn = 0,
  error,
  idle,
  success,
  busy,
  rainbow,
  unknown,
  on_,
  off_,
} LedForStatus;

typedef enum {
  array = 0,
  full,
  single,
  off,
  left,
  right,
  top,
  bottom,
  multi,
} LedModes;

#pragma pack(push, 1)
typedef struct {
  uint16_t qid;   // user-assigned ID (2 bytes — keeps the 17-byte frame)
  uint8_t mode;   // LedMode as 1 byte (the C enum is int-sized; store the value)
  uint8_t r, g, b;
  uint8_t radius;
  char region[8]; // "left","right","top","bottom" (for HALVES)
  uint16_t ledIndex;
} LedCommand;
#pragma pack(pop)

void led_setup(void);
void led_loop(void);
int led_act(cJSON *doc);
cJSON *led_get(cJSON *doc);
void led_turn_off(void);
void led_fill_all(uint8_t r, uint8_t g, uint8_t b);
void led_set_single(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
void led_fill_halves(const char *region, uint8_t r, uint8_t g, uint8_t b);
void led_fill_halves_ring_segments(const char *region, uint8_t r, uint8_t g,
                                   uint8_t b);
void led_draw_rings(uint8_t radius, uint8_t r, uint8_t g, uint8_t b);
void led_draw_illumination_rings(uint8_t ring_id, uint8_t r, uint8_t g,
                                 uint8_t b);
void led_draw_illumination_ring_segment(uint8_t ring_id, const char *region,
                                        uint8_t r, uint8_t g, uint8_t b,
                                        bool clear_before_draw);
void led_draw_circle(uint8_t radius, uint8_t r, uint8_t g, uint8_t b);
bool led_parse_command(cJSON *root, LedCommand *cmd);
void led_exec_command(const LedCommand *cmd);
void led_circle_changed_event(int pressed);
void led_set_pixel_xy(int x, int y, uint8_t r, uint8_t g, uint8_t b);
uint16_t led_xy_to_index(int x, int y);
void led_set_mode(uint8_t mode, uint8_t brightness, uint32_t colour);
void led_set_pattern(uint8_t pattern_id, uint16_t speed);
void led_set_pixels(const uint8_t *data, uint16_t pixel_count);

// C-facing shims consumed by C callers (OD.c, ThermalController.c,
// FocusScan.c).
void led_c_turn_off(void);
void led_exec_circ_command(uint8_t rgb, uint8_t radius, uint8_t qid);

#ifdef __cplusplus
}
#endif

#endif // LED_CONTROLLER_H

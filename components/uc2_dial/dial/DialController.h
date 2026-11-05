#pragma once
#include <PinConfig.h>
#include "cJSON.h"
#include <freertos/FreeRTOS.h>

// T4.7: DialController C-ified (was DialController.cpp C++ namespace).
// Pure C flat API: dial_act(), dial_get(), dial_setup(), dial_loop().
// The M5 stack includes (M5Unified.hpp / M5Dial.h) remain in DialController.c
// so consumers only see the C API.

// Wire format the master writes to the dial (i2c_master -> i2c_slave_dial):
// four axis positions + laser intensity. Packed, 20 bytes, no padding.
#pragma pack(push, 1)
typedef struct {
    int32_t pos_a;
    int32_t pos_x;
    int32_t pos_y;
    int32_t pos_z;
    int32_t intensity;
} DialData;
#pragma pack(pop)

// Operating modes
typedef enum {
    DIAL_MODE_MOTOR = 0,
    DIAL_MODE_ILLUMINATION
} DialMode;

// Motor axis identifiers — values are the RoutingTable logicalId
// (Stepper enum order A=0, X=1, Y=2, Z=3).
typedef enum {
    MOTOR_AXIS_A = 0,
    MOTOR_AXIS_X = 1,
    MOTOR_AXIS_Y = 2,
    MOTOR_AXIS_Z = 3,
    MOTOR_AXIS_MAX = 4
} MotorAxis;

// Illumination channels: 0..3 = lasers, 4..7 = LED matrix
#define LASER_CHANNEL_COUNT 4
#define ILLUM_LED_RGB 4
#define ILLUM_LED_R   5
#define ILLUM_LED_G   6
#define ILLUM_LED_B   7
#define ILLUM_CHANNEL_COUNT 8
#define MAX_LED 255

// M5Dial encoder: quadrature on GPIO 41/40, counted by the PCNT peripheral
#define ENCODER_PIN_A 41
#define ENCODER_PIN_B 40
#define ENCODER_COUNTS_PER_DETENT 4

typedef struct {
    int32_t motorSpeed;  // steps/s for relative moves
} DialConfig;

// Available step increments for motor mode
extern const int MOTOR_INCREMENTS[];
#define MOTOR_INCREMENT_COUNT 5

// Available step increments for illumination mode
extern const int ILLUM_INCREMENTS[];
#define ILLUM_INCREMENT_COUNT 5
// Laser slaves default to 10-bit PWM (pinConfig.LASER_PWM_RESOLUTION = 10).
#define MAX_ILLUMINATION 1023

// Timing constants
#define LONG_PRESS_DURATION_MS 500
#define SHORT_PRESS_MAX_MS 500
#define SEND_INTERVAL_MS 250
#define DEBOUNCE_MS 50

// Display colors (RGB565 format)
#define COLOR_BG 0x0000
#define COLOR_AXIS_X 0xF800
#define COLOR_AXIS_Y 0x07E0
#define COLOR_AXIS_Z 0x001F
#define COLOR_AXIS_A 0xFFE0
#define COLOR_ILLUM_ON 0xF81F
#define COLOR_ILLUM_OFF 0x7BEF
#define COLOR_TEXT 0xFFFF
#define COLOR_ACCENT 0x07FF
#define COLOR_INACTIVE 0x7BEF

// API functions (C flat API)
#ifdef __cplusplus
extern "C" {
#endif

int dial_act(cJSON *jsonDocument);
cJSON *dial_get(cJSON *jsonDocument);
void dial_setup(void);
void dial_loop(void);

// Display functions
void dial_update_display(void);
void dial_draw_motor_screen(void);
void dial_draw_illumination_screen(void);

// CANopen communication (expedited SDO writes to the routed slave node)
void dial_send_motor_command(int axis, int32_t steps);
void dial_send_laser_command(int laser_id, int intensity);
void dial_send_led_command(void);

// Dial data (received via I2C or applied locally)
extern DialData dial_data;
void dial_set_values(DialData d);
DialData dial_get_values(void);

// Internal state getters (for debugging/API)
DialMode dial_get_current_mode(void);
MotorAxis dial_get_current_axis(void);
int dial_get_current_illum_channel(void);
int dial_get_current_increment(void);
int dial_get_illumination_value(void);
bool dial_is_illumination_on(void);

#ifdef __cplusplus
}
#endif
#pragma once
#include <PinConfig.h>
#include "cJSON.h"
#include <freertos/FreeRTOS.h>

// T5.4: the M5 includes (M5Unified.hpp / M5Dial.h) moved to DialController.cpp.
// M5Dial.h pulls the RFID utility (MFRC522 -> Arduino.h / Wire.h); keeping the M5
// includes out of this header stops non-dial consumers (i2c_slave_dial.cpp in
// uc2_periph, main.cpp in uc2_app) from dragging the whole M5 stack + its Arduino
// shims into every TU that merely uses the DialData wire API.

// Wire format the master writes to the dial (i2c_master -> i2c_slave_dial):
// four axis positions + laser intensity. Packed, 20 bytes, no padding.
#pragma pack(push, 1)
struct DialData {
    int32_t pos_a = 0;
    int32_t pos_x = 0;
    int32_t pos_y = 0;
    int32_t pos_z = 0;
    int32_t intensity = 0;
};
#pragma pack(pop)

namespace DialController
{
    // API functions
    int act(cJSON *jsonDocument);
    cJSON *get(cJSON *jsonDocument);
    void setup();
    void loop();

    // Operating modes
    enum class DialMode {
        MOTOR = 0,      // Control motor axes X, Y, Z, A
        ILLUMINATION    // Control laser illumination
    };

    // Motor axis identifiers — values are the RoutingTable logicalId
    // (Stepper enum order A=0, X=1, Y=2, Z=3). Node ids come from the
    // routing table (pinConfig.CAN_ID_MOT_*), not from here.
    enum class MotorAxis {
        A = 0,
        X = 1,
        Y = 2,
        Z = 3,
        AXIS_COUNT = 4
    };

    // Illumination channels: 0..3 = lasers (RoutingTable LASER logicalId,
    // pinConfig.CAN_NODE_LASER[ch] / CAN_SUBAXIS_LASER[ch]); 4..7 = LED matrix
    // (RoutingTable LED 0): RGB sets all three components at once, R/G/B one each.
    static const int LASER_CHANNEL_COUNT = 4;
    static const int ILLUM_LED_RGB = 4;
    static const int ILLUM_LED_R   = 5;
    static const int ILLUM_LED_G   = 6;
    static const int ILLUM_LED_B   = 7;
    static const int ILLUM_CHANNEL_COUNT = 8;
    static const int MAX_LED = 255;

    // M5Dial encoder: quadrature on GPIO 41/40, counted by the PCNT peripheral
    // (ESP32Encoder). 4 edges per detent; tune if the knob feels off.
    static const int ENCODER_PIN_A = 41;
    static const int ENCODER_PIN_B = 40;
    static const int ENCODER_COUNTS_PER_DETENT = 4;

    struct DialConfig {
        int32_t motorSpeed = 10000; // steps/s for relative moves
    };

    // Available step increments for motor mode
    static const int MOTOR_INCREMENTS[] = {1, 5, 10, 100, 1000};
    static const int MOTOR_INCREMENT_COUNT = 5;

    // Available step increments for illumination mode
    static const int ILLUM_INCREMENTS[] = {1, 5, 10, 25, 100};
    static const int ILLUM_INCREMENT_COUNT = 5;
    // Laser slaves default to 10-bit PWM (pinConfig.LASER_PWM_RESOLUTION = 10).
    static const int MAX_ILLUMINATION = 1023;

    // Timing constants
    static const unsigned long LONG_PRESS_DURATION_MS = 500;   // Long press to switch axis/mode
    static const unsigned long SHORT_PRESS_MAX_MS = 500;       // Max duration for short press
    static const unsigned long SEND_INTERVAL_MS = 250;
    static const unsigned long DEBOUNCE_MS = 50;               // Touch debounce

    // Display colors (RGB565 format)
    static const uint16_t COLOR_BG = 0x0000;           // Black background
    static const uint16_t COLOR_AXIS_X = 0xF800;       // Red for X
    static const uint16_t COLOR_AXIS_Y = 0x07E0;       // Green for Y
    static const uint16_t COLOR_AXIS_Z = 0x001F;       // Blue for Z
    static const uint16_t COLOR_AXIS_A = 0xFFE0;       // Yellow for A
    static const uint16_t COLOR_ILLUM_ON = 0xF81F;     // Magenta for illumination ON
    static const uint16_t COLOR_ILLUM_OFF = 0x7BEF;    // Gray for illumination OFF
    static const uint16_t COLOR_TEXT = 0xFFFF;         // White text
    static const uint16_t COLOR_ACCENT = 0x07FF;       // Cyan accent
    static const uint16_t COLOR_INACTIVE = 0x7BEF;     // Gray for inactive elements

    // Display functions
    void updateDisplay();
    void drawMotorScreen();
    void drawIlluminationScreen();
    void drawModeIndicator();

    // CANopen communication (expedited SDO writes to the routed slave node)
    void sendMotorCommand(int axis, int32_t steps);
    void sendLaserCommand(int laserId, int intensity);
    void sendLedCommand();

    // Last dial data (received via I2C in i2c_slave_dial, or applied locally).
    // extern: DialData has default member initializers, so a plain
    // "DialData mDialData;" in this header is a DEFINITION (one per TU);
    // the single definition lives in DialController.cpp.
    extern DialData mDialData;
    void setDialValues(const DialData &d);
    DialData getDialValues();

    // Internal state getters (for debugging/API)
    DialMode getCurrentMode();
    MotorAxis getCurrentAxis();
    int getCurrentIllumChannel();
    int getCurrentIncrement();
    int getIlluminationValue();
    bool isIlluminationOn();

} // namespace DialController
#pragma once
#include <PinConfig.h>
#include "cJSON.h"
#include <freertos/FreeRTOS.h>

#ifdef M5DIAL
#include <M5Unified.hpp>
#include <M5Dial.h>
#endif

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

    // Laser channels 0..3 = RoutingTable LASER logicalId
    // (pinConfig.CAN_NODE_LASER[ch] / CAN_SUBAXIS_LASER[ch]).
    static const int LASER_CHANNEL_COUNT = 4;

    struct DialConfig {
        int32_t motorSpeed = 10000; // steps/s for relative moves
    };

    // Available step increments for motor mode
    static const int MOTOR_INCREMENTS[] = {1, 5, 10, 100};
    static const int MOTOR_INCREMENT_COUNT = 4;

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

    // Internal state getters (for debugging/API)
    DialMode getCurrentMode();
    MotorAxis getCurrentAxis();
    int getCurrentLaser();
    int getCurrentIncrement();
    int getIlluminationValue();
    bool isIlluminationOn();

} // namespace DialController
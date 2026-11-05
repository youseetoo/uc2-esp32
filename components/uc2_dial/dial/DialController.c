#include <PinConfig.h>
#include "DialController.h"
// T5.4: M5 includes live here (not in DialController.h) so only this TU drags in
// the M5 stack. Under M5DIAL the dial uses M5GFX (display) + M5Dial (encoder);
// M5Dial.h pulls the RFID utility (MFRC522 -> Arduino.h / Wire.h), satisfied by
// the shims in components/m5dial/src.
#ifdef M5DIAL
#include <M5Unified.hpp>
#include <M5Dial.h>
#endif
#include <uc2_hal.h>
#include <string.h>
#include "cJsonTool.h"

#include "MotorTypes.h"
// SdoEmit.h / CANopenModule.h are not yet converted to direct IDF (they
// include Arduino.h). The dial SDO path only compiles where SdoEmit works:
// Arduino builds (ARDUINO defined) or, once the CANopen layer is converted,
// IDF builds that define UC2_SDOEMIT_AVAILABLE (see main/CMakeLists.txt).
// Note: -UCAN_CONTROLLER_CANOPEN cannot suppress this — PinConfig.h
// redefines the macro via #ifndef, so the guard keys off SdoEmit
// availability instead.
#if defined(CAN_CONTROLLER_CANOPEN) && (defined(UC2_SDOEMIT_AVAILABLE) || defined(ARDUINO))
#include "RoutingTable.h"
#include "SdoEmit.h"
#endif
#ifdef M5DIAL
#include <ESP32Encoder.h>
// M5GFX + the dial draw code reference PI (Arduino defines it; neither IDF nor
// M5GFX does). Guarded so a future M5GFX PI definition wins.
#ifndef PI
#define PI 3.14159265358979323846
#endif
#endif

// T4.7: C-ified — namespace DialController replaced with dial_ prefix functions.
// M5DIAL is still C++ (M5Unified/M5Dial/ESP32Encoder are C++ libs); this file
// stays .c but compiles as C++ when M5DIAL is defined.

DialData dial_data = {0, 0, 0, 0, 0};

const int MOTOR_INCREMENTS[] = {1, 5, 10, 100, 1000};
const int ILLUM_INCREMENTS[] = {1, 5, 10, 25, 100};

// Internal state variables
static DialMode current_mode = DIAL_MODE_MOTOR;
static MotorAxis current_axis = MOTOR_AXIS_Z;
static int motor_increment_index = 0;
static int32_t accumulated_steps = 0;
static long last_encoder_pos = 0;

static int current_illum = 0;
static int illum_value[ILLUM_CHANNEL_COUNT] = {0};
static bool illum_on[ILLUM_CHANNEL_COUNT] = {false};
static int illum_increment_index = 2;

#ifdef M5DIAL
static ESP32Encoder encoder;
#endif

static unsigned long touch_start_time = 0;
static bool touch_active = false;
static unsigned long last_send_time = 0;

static DialConfig config;

// Display dimensions (M5Dial has 240x240 display)
#define DISPLAY_WIDTH 240
#define DISPLAY_HEIGHT 240
#define CENTER_X (DISPLAY_WIDTH / 2)
#define CENTER_Y (DISPLAY_HEIGHT / 2)

// Helper: get color for current axis
static uint16_t get_axis_color(MotorAxis axis) {
    switch (axis) {
        case MOTOR_AXIS_X: return COLOR_AXIS_X;
        case MOTOR_AXIS_Y: return COLOR_AXIS_Y;
        case MOTOR_AXIS_Z: return COLOR_AXIS_Z;
        case MOTOR_AXIS_A: return COLOR_AXIS_A;
        default: return COLOR_TEXT;
    }
}

// Helper: get axis name as string
static const char* get_axis_name(MotorAxis axis) {
    switch (axis) {
        case MOTOR_AXIS_X: return "X";
        case MOTOR_AXIS_Y: return "Y";
        case MOTOR_AXIS_Z: return "Z";
        case MOTOR_AXIS_A: return "A";
        default: return "?";
    }
}

// Illumination channel helpers
static inline bool is_led_channel(int ch) { return ch >= ILLUM_LED_RGB; }
static inline int on_index(int ch) { return is_led_channel(ch) ? ILLUM_LED_RGB : ch; }
static inline int channel_max(int ch) { return is_led_channel(ch) ? MAX_LED : MAX_ILLUMINATION; }
static const char* channel_name(int ch) {
    static const char* names[ILLUM_CHANNEL_COUNT] =
        {"LASER 0", "LASER 1", "LASER 2", "LASER 3", "LED RGB", "LED R", "LED G", "LED B"};
    return (ch >= 0 && ch < ILLUM_CHANNEL_COUNT) ? names[ch] : "?";
}

// Get axis index for CAN motor arrays (0=A, 1=X, 2=Y, 3=Z)
static int get_axis_index(MotorAxis axis) {
    switch (axis) {
        case MOTOR_AXIS_A: return 0;
        case MOTOR_AXIS_X: return 1;
        case MOTOR_AXIS_Y: return 2;
        case MOTOR_AXIS_Z: return 3;
        default: return 0;
    }
}

// CANopen communication functions
void dial_send_motor_command(int axis, int32_t steps) {
    if (steps == 0) return;
#if defined(CAN_CONTROLLER_CANOPEN) && (defined(UC2_SDOEMIT_AVAILABLE) || defined(ARDUINO))
    const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::MOTOR, (uint8_t)axis);
    if (!route || route->where != UC2::RouteEntry::REMOTE) {
        log_w("Dial: motor axis %d has no REMOTE route", axis);
        return;
    }
    log_d("Dial motor: axis=%d steps=%d -> node 0x%02X sub %u", axis, steps, route->nodeId, route->subAxis);
    SdoEmit::motor(route->nodeId, route->subAxis, steps, config.motorSpeed,
                  (uint32_t)MAX_ACCELERATION_A, false, false, false);
#endif
}

void dial_send_laser_command(int laser_id, int intensity) {
#if defined(CAN_CONTROLLER_CANOPEN) && (defined(UC2_SDOEMIT_AVAILABLE) || defined(ARDUINO))
    const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::LASER, (uint8_t)laser_id);
    if (!route || route->where != UC2::RouteEntry::REMOTE) {
        log_w("Dial: laser %d has no REMOTE route", laser_id);
        return;
    }
    log_d("Dial laser: id=%d value=%d -> node 0x%02X sub %u", laser_id, intensity, route->nodeId, route->subAxis);
    SdoEmit::laser(route->nodeId, route->subAxis, (uint16_t)intensity);
#endif
}

void dial_send_led_command() {
#if defined(CAN_CONTROLLER_CANOPEN) && (defined(UC2_SDOEMIT_AVAILABLE) || defined(ARDUINO))
    const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::LED, 0);
    if (!route || route->where != UC2::RouteEntry::REMOTE) {
        log_w("Dial: LED has no REMOTE route");
        return;
    }
    uint32_t rgb = ((uint32_t)illum_value[ILLUM_LED_R] << 16) |
                  ((uint32_t)illum_value[ILLUM_LED_G] << 8)  |
                   (uint32_t)illum_value[ILLUM_LED_B];
    bool on = illum_on[ILLUM_LED_RGB];
    log_d("Dial LED: on=%d rgb=0x%06X -> node 0x%02X", on, rgb, route->nodeId);
    SdoEmit::led(route->nodeId, on, rgb);
#endif
}

// Push the current state of one illumination channel to the bus.
static void send_illum(int ch) {
    if (is_led_channel(ch)) dial_send_led_command();
    else dial_send_laser_command(ch, illum_on[ch] ? illum_value[ch] : 0);
}

// Display functions
void dial_draw_motor_screen() {
#ifdef M5DIAL
    M5Dial.Display.fillScreen(COLOR_BG);
    
    uint16_t axis_color = get_axis_color(current_axis);
    int increment = MOTOR_INCREMENTS[motor_increment_index];
    
    // Draw decorative ring around edge
    M5Dial.Display.drawCircle(CENTER_X, CENTER_Y, 115, axis_color);
    M5Dial.Display.drawCircle(CENTER_X, CENTER_Y, 114, axis_color);
    
    // Draw large axis letter in center
    M5Dial.Display.setTextColor(axis_color);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setTextSize(3);
    M5Dial.Display.drawString(get_axis_name(current_axis), CENTER_X, CENTER_Y - 20);
    
    // Draw increment value below axis
    M5Dial.Display.setTextColor(COLOR_TEXT);
    M5Dial.Display.setTextSize(1.5);
    char inc_text[32];
    snprintf(inc_text, sizeof(inc_text), "Step: %d", increment);
    M5Dial.Display.drawString(inc_text, CENTER_X, CENTER_Y + 40);
    
    // Draw mode indicator at bottom
    M5Dial.Display.setTextSize(0.8);
    M5Dial.Display.setTextColor(COLOR_ACCENT);
    M5Dial.Display.drawString("MOTOR", CENTER_X, DISPLAY_HEIGHT - 25);
    
    // Draw axis indicators around the ring
    M5Dial.Display.setTextSize(0.8);
    const char* axes[] = {"X", "Y", "Z", "A"};
    const MotorAxis axis_enums[] = {MOTOR_AXIS_X, MOTOR_AXIS_Y, MOTOR_AXIS_Z, MOTOR_AXIS_A};
    const int angles[] = {0, 90, 180, 270};
    
    for (int i = 0; i < 4; i++) {
        float rad = (angles[i] - 90) * PI / 180.0;
        int x = CENTER_X + (int)(85 * cos(rad));
        int y = CENTER_Y + (int)(85 * sin(rad));
        
        if (axis_enums[i] == current_axis) {
            M5Dial.Display.setTextColor(axis_color);
            M5Dial.Display.fillCircle(x, y, 15, axis_color);
            M5Dial.Display.setTextColor(COLOR_BG);
        } else {
            M5Dial.Display.setTextColor(COLOR_INACTIVE);
        }
        M5Dial.Display.drawString(axes[i], x, y);
    }
    M5Dial.Display.setTextColor(COLOR_TEXT);
#endif
}

void dial_draw_illumination_screen() {
#ifdef M5DIAL
    M5Dial.Display.fillScreen(COLOR_BG);
    
    uint16_t main_color = illum_on[on_index(current_illum)] ? COLOR_ILLUM_ON : COLOR_ILLUM_OFF;
    int increment = ILLUM_INCREMENTS[illum_increment_index];
    
    // Draw decorative ring
    M5Dial.Display.drawCircle(CENTER_X, CENTER_Y, 115, main_color);
    M5Dial.Display.drawCircle(CENTER_X, CENTER_Y, 114, main_color);
    
    // Draw illumination icon
    M5Dial.Display.setTextColor(main_color);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.drawString(illum_on[on_index(current_illum)] ? "ON" : "OFF", CENTER_X, CENTER_Y - 50);
    
    // Draw illumination value in large text
    M5Dial.Display.setTextSize(2.5);
    M5Dial.Display.setTextColor(COLOR_TEXT);
    char value_text[32];
    snprintf(value_text, sizeof(value_text), "%d", illum_value[current_illum]);
    M5Dial.Display.drawString(value_text, CENTER_X, CENTER_Y);
    
    // Draw progress arc showing illumination level
    int arc_angle = map(illum_value[current_illum], 0, channel_max(current_illum), 0, 360);
    if (arc_angle > 0) {
        for (int a = -90; a < -90 + arc_angle && a < 270; a += 2) {
            float rad = a * PI / 180.0;
            int x = CENTER_X + (int)(100 * cos(rad));
            int y = CENTER_Y + (int)(100 * sin(rad));
            M5Dial.Display.fillCircle(x, y, 3, main_color);
        }
    }
    
    // Draw increment value
    M5Dial.Display.setTextSize(1);
    M5Dial.Display.setTextColor(COLOR_TEXT);
    char inc_text[32];
    snprintf(inc_text, sizeof(inc_text), "Step: %d", increment);
    M5Dial.Display.drawString(inc_text, CENTER_X, CENTER_Y + 50);
    
    // Draw mode indicator with selected channel
    M5Dial.Display.setTextSize(0.8);
    M5Dial.Display.setTextColor(COLOR_ACCENT);
    M5Dial.Display.drawString(channel_name(current_illum), CENTER_X, DISPLAY_HEIGHT - 25);
#endif
}

void dial_update_display() {
#ifdef M5DIAL
    if (current_mode == DIAL_MODE_MOTOR) {
        dial_draw_motor_screen();
    } else {
        dial_draw_illumination_screen();
    }
#endif
}

// State getters
DialMode dial_get_current_mode() { return current_mode; }
MotorAxis dial_get_current_axis() { return current_axis; }
int dial_get_current_illum_channel() { return current_illum; }
int dial_get_current_increment() {
    return current_mode == DIAL_MODE_MOTOR ?
           MOTOR_INCREMENTS[motor_increment_index] :
           ILLUM_INCREMENTS[illum_increment_index];
}
int dial_get_illumination_value() { return illum_value[current_illum]; }
bool dial_is_illumination_on() { return illum_on[on_index(current_illum)]; }

// Input handling
static void handle_short_press() {
    if (current_mode == DIAL_MODE_MOTOR) {
        motor_increment_index = (motor_increment_index + 1) % MOTOR_INCREMENT_COUNT;
        log_d("Motor increment changed to: %d", MOTOR_INCREMENTS[motor_increment_index]);
    } else {
        int oi = on_index(current_illum);
        illum_on[oi] = !illum_on[oi];
        send_illum(current_illum);
        log_d("%s toggled: %s", channel_name(current_illum), illum_on[oi] ? "ON" : "OFF");
    }
    dial_update_display();
}

static void handle_long_press() {
    if (current_mode == DIAL_MODE_MOTOR) {
        switch (current_axis) {
            case MOTOR_AXIS_X: current_axis = MOTOR_AXIS_Y; break;
            case MOTOR_AXIS_Y: current_axis = MOTOR_AXIS_Z; break;
            case MOTOR_AXIS_Z: current_axis = MOTOR_AXIS_A; break;
            case MOTOR_AXIS_A: current_axis = MOTOR_AXIS_X; break;
            default: current_axis = MOTOR_AXIS_X; break;
        }
        log_d("Motor axis changed to: %s", get_axis_name(current_axis));
    } else {
        current_illum = (current_illum + 1) % ILLUM_CHANNEL_COUNT;
        log_d("Illumination channel changed to: %s", channel_name(current_illum));
    }
    dial_update_display();
}

static void handle_encoder_change(long delta) {
    if (current_mode == DIAL_MODE_MOTOR) {
        int increment = MOTOR_INCREMENTS[motor_increment_index];
        accumulated_steps += delta * increment;
    } else {
        int increment = ILLUM_INCREMENTS[illum_increment_index];
        illum_value[current_illum] += delta * increment;
        
        if (illum_value[current_illum] < 0) illum_value[current_illum] = 0;
        if (illum_value[current_illum] > channel_max(current_illum)) illum_value[current_illum] = channel_max(current_illum);

        if (current_illum == ILLUM_LED_RGB)
            illum_value[ILLUM_LED_R] = illum_value[ILLUM_LED_G] = illum_value[ILLUM_LED_B] = illum_value[current_illum];
        
        if (illum_on[on_index(current_illum)]) {
            send_illum(current_illum);
        }
        dial_update_display();
    }
}

// API functions
int dial_act(cJSON *json_document) {
    int qid = cJsonTool::getJsonInt(json_document, "qid");
    
    cJSON *mode_item = cJSON_GetObjectItem(json_document, "mode");
    if (mode_item != NULL) {
        if (strcmp(mode_item->valuestring, "motor") == 0) {
            current_mode = DIAL_MODE_MOTOR;
            dial_update_display();
        } else if (strcmp(mode_item->valuestring, "illumination") == 0) {
            current_mode = DIAL_MODE_ILLUMINATION;
            dial_update_display();
        }
    }
    
    cJSON *axis_item = cJSON_GetObjectItem(json_document, "axis");
    if (axis_item != NULL) {
        const char* axis_str = axis_item->valuestring;
        if (strcmp(axis_str, "X") == 0 || strcmp(axis_str, "x") == 0) current_axis = MOTOR_AXIS_X;
        else if (strcmp(axis_str, "Y") == 0 || strcmp(axis_str, "y") == 0) current_axis = MOTOR_AXIS_Y;
        else if (strcmp(axis_str, "Z") == 0 || strcmp(axis_str, "z") == 0) current_axis = MOTOR_AXIS_Z;
        else if (strcmp(axis_str, "A") == 0 || strcmp(axis_str, "a") == 0) current_axis = MOTOR_AXIS_A;
        dial_update_display();
    }
    
    cJSON *speed_item = cJSON_GetObjectItem(json_document, "speed");
    if (speed_item != NULL) {
        config.motorSpeed = speed_item->valueint;
    }
    
    cJSON *ch_item = cJSON_GetObjectItem(json_document, "channel");
    if (ch_item != NULL && ch_item->valueint >= 0 && ch_item->valueint < ILLUM_CHANNEL_COUNT) {
        current_illum = ch_item->valueint;
        dial_update_display();
    }
    
    cJSON *inc_item = cJSON_GetObjectItem(json_document, "increment");
    if (inc_item != NULL) {
        int inc = inc_item->valueint;
        if (current_mode == DIAL_MODE_MOTOR) {
            for (int i = 0; i < MOTOR_INCREMENT_COUNT; i++)
                if (MOTOR_INCREMENTS[i] == inc) { motor_increment_index = i; break; }
        } else {
            for (int i = 0; i < ILLUM_INCREMENT_COUNT; i++)
                if (ILLUM_INCREMENTS[i] == inc) { illum_increment_index = i; break; }
        }
        dial_update_display();
    }
    
    cJSON *illum_item = cJSON_GetObjectItem(json_document, "illumination");
    if (illum_item != NULL) {
        illum_value[current_illum] = illum_item->valueint;
        if (illum_value[current_illum] < 0) illum_value[current_illum] = 0;
        if (illum_value[current_illum] > channel_max(current_illum)) illum_value[current_illum] = channel_max(current_illum);
        if (current_illum == ILLUM_LED_RGB)
            illum_value[ILLUM_LED_R] = illum_value[ILLUM_LED_G] = illum_value[ILLUM_LED_B] = illum_value[current_illum];
        if (illum_on[on_index(current_illum)]) send_illum(current_illum);
        dial_update_display();
    }
    
    log_d("dial_act_fct");
    return qid;
}

cJSON *dial_get(cJSON *json_document) {
    cJSON *result = cJSON_CreateObject();
    
    cJSON_AddStringToObject(result, "mode", current_mode == DIAL_MODE_MOTOR ? "motor" : "illumination");
    cJSON_AddStringToObject(result, "axis", get_axis_name(current_axis));
    cJSON_AddNumberToObject(result, "channel", current_illum);
    cJSON_AddStringToObject(result, "channelName", channel_name(current_illum));
    cJSON_AddNumberToObject(result, "increment", dial_get_current_increment());
    cJSON_AddNumberToObject(result, "speed", config.motorSpeed);
    cJSON_AddNumberToObject(result, "illumination", illum_value[current_illum]);
    cJSON_AddBoolToObject(result, "illuminationOn", illum_on[on_index(current_illum)]);
    
    return result;
}

void dial_set_values(DialData d) {
    dial_data = d;
}

DialData dial_get_values() {
    return dial_data;
}

// Main loop
void dial_loop() {
#ifdef M5DIAL
    M5Dial.update();
    
    if (M5Dial.BtnA.wasPressed()) {
        M5Dial.Speaker.tone(8000, 20);
        current_mode = (current_mode == DIAL_MODE_MOTOR) ? DIAL_MODE_ILLUMINATION : DIAL_MODE_MOTOR;
        log_d("Mode switched to: %s", current_mode == DIAL_MODE_MOTOR ? "MOTOR" : "ILLUMINATION");
        dial_update_display();
    }
    
    long new_encoder_pos = (long)encoder.getCount();
    long detents = (new_encoder_pos - last_encoder_pos) / ENCODER_COUNTS_PER_DETENT;
    if (detents != 0) {
        last_encoder_pos += detents * ENCODER_COUNTS_PER_DETENT;
        handle_encoder_change(detents);
    }
    
    auto t = M5Dial.Touch.getDetail();
    
    if (t.state == 3) {
        touch_start_time = millis();
        touch_active = true;
    }
    
    if ((t.state == 2 || t.state == 7) && touch_active) {
        touch_active = false;
        unsigned long touch_duration = millis() - touch_start_time;
        
        if (touch_duration >= LONG_PRESS_DURATION_MS) {
            M5Dial.Speaker.tone(6000, 30);
            handle_long_press();
        } else if (touch_duration >= DEBOUNCE_MS) {
            M5Dial.Speaker.tone(8000, 20);
            handle_short_press();
        }
    }
    
    if (current_mode == DIAL_MODE_MOTOR && accumulated_steps != 0) {
        unsigned long current_time = millis();
        if (current_time - last_send_time >= SEND_INTERVAL_MS) {
            int axis_idx = get_axis_index(current_axis);
            dial_send_motor_command(axis_idx, accumulated_steps);
            accumulated_steps = 0;
            last_send_time = current_time;
        }
    }
#endif
}

void dial_setup() {
#ifdef M5DIAL
    log_i("Initializing Dial Controller (CANopen originator)");
    
    auto cfg = M5.config();
    cfg.external_spk = false;
    cfg.external_rtc = false;
    M5Dial.begin(cfg, false, false);

    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachFullQuad(ENCODER_PIN_A, ENCODER_PIN_B);
    encoder.setFilter(1023);
    encoder.clearCount();
    
    M5Dial.Display.setTextColor(COLOR_TEXT);
    M5Dial.Display.setTextDatum(middle_center);
    M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
    M5Dial.Display.setTextSize(1);
    
    last_encoder_pos = (long)encoder.getCount();
    
    M5Dial.Display.fillScreen(COLOR_BG);
    M5Dial.Display.setTextColor(COLOR_ACCENT);
    M5Dial.Display.setTextSize(1.5);
    M5Dial.Display.drawString("UC2 DIAL", CENTER_X, CENTER_Y - 30);
    M5Dial.Display.setTextSize(0.8);
    M5Dial.Display.setTextColor(COLOR_TEXT);
    M5Dial.Display.drawString("CANopen", CENTER_X, CENTER_Y + 10);
    M5Dial.Display.drawString("Initializing...", CENTER_X, CENTER_Y + 40);
    
    delay(1000);
    
    dial_update_display();
    
    log_i("Dial Controller initialized - Mode: MOTOR, Axis: %s, Increment: %d",
          get_axis_name(current_axis), MOTOR_INCREMENTS[motor_increment_index]);
#endif
}
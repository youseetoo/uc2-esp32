#include <PinConfig.h>
#include "DialController.h"
#include "Arduino.h"
#include "../../JsonKeys.h"
#include "cJsonTool.h"

#include "../motor/MotorTypes.h"
#include "../canopen/RoutingTable.h"
#include "../canopen/SdoEmit.h"
#ifdef M5DIAL
#include <ESP32Encoder.h>
#endif

namespace DialController
{
    // ========================================================================
    // Internal State Variables
    // ========================================================================
    
    // Current operating mode
    static DialMode currentMode = DialMode::MOTOR;
    
    // Motor mode state
    static MotorAxis currentAxis = MotorAxis::Z;    // Start with Z axis
    static int motorIncrementIndex = 0;             // Index into MOTOR_INCREMENTS array
    static int32_t accumulatedSteps = 0;            // Steps accumulated since last send
    static long lastEncoderPos = 0;                 // Last encoder position
    
    // Illumination mode state (per channel; LED channels 4..7 share one on/off
    // flag at index ILLUM_LED_RGB since they are one device)
    static int currentIllum = 0;                            // Selected channel 0..7
    static int illumValue[ILLUM_CHANNEL_COUNT] = {0};       // Intensity per channel
    static bool illumOn[ILLUM_CHANNEL_COUNT] = {false};     // On/off (lasers per channel, LED shared)
    static int illumIncrementIndex = 2;                     // Index into ILLUM_INCREMENTS array (default 10)

#ifdef M5DIAL
    static ESP32Encoder encoder;                            // PCNT-backed, never misses an edge
#endif
    
    // Touch handling state
    static unsigned long touchStartTime = 0;
    static bool touchActive = false;
    
    // Timing for motor command sending
    static unsigned long lastSendTime = 0;
    
    // Configuration (loaded from pinConfig)
    static DialConfig config;
    
    // Display dimensions (M5Dial has 240x240 display)
    static const int DISPLAY_WIDTH = 240;
    static const int DISPLAY_HEIGHT = 240;
    static const int CENTER_X = DISPLAY_WIDTH / 2;
    static const int CENTER_Y = DISPLAY_HEIGHT / 2;

    // ========================================================================
    // Helper Functions
    // ========================================================================
    
    // Get color for current axis
    uint16_t getAxisColor(MotorAxis axis)
    {
        switch (axis)
        {
            case MotorAxis::X: return COLOR_AXIS_X;
            case MotorAxis::Y: return COLOR_AXIS_Y;
            case MotorAxis::Z: return COLOR_AXIS_Z;
            case MotorAxis::A: return COLOR_AXIS_A;
            default: return COLOR_TEXT;
        }
    }
    
    // Get axis name as string
    const char* getAxisName(MotorAxis axis)
    {
        switch (axis)
        {
            case MotorAxis::X: return "X";
            case MotorAxis::Y: return "Y";
            case MotorAxis::Z: return "Z";
            case MotorAxis::A: return "A";
            default: return "?";
        }
    }
    
    // Illumination channel helpers
    static inline bool isLedChannel(int ch) { return ch >= ILLUM_LED_RGB; }
    static inline int  onIndex(int ch)      { return isLedChannel(ch) ? ILLUM_LED_RGB : ch; }
    static inline int  channelMax(int ch)   { return isLedChannel(ch) ? MAX_LED : MAX_ILLUMINATION; }
    static const char* channelName(int ch)
    {
        static const char* names[ILLUM_CHANNEL_COUNT] =
            {"LASER 0", "LASER 1", "LASER 2", "LASER 3", "LED RGB", "LED R", "LED G", "LED B"};
        return (ch >= 0 && ch < ILLUM_CHANNEL_COUNT) ? names[ch] : "?";
    }

    // Get axis index for CAN motor arrays (0=A, 1=X, 2=Y, 3=Z)
    int getAxisIndex(MotorAxis axis)
    {
        switch (axis)
        {
            case MotorAxis::A: return 0;
            case MotorAxis::X: return 1;
            case MotorAxis::Y: return 2;
            case MotorAxis::Z: return 3;
            default: return 0;
        }
    }

    // ========================================================================
    // CANopen Communication Functions
    // The dial is a NODE_ROLE=2 originator: it looks up the REMOTE route the
    // master would use and writes the same expedited SDOs itself, so the
    // master is bypassed entirely (same pattern as PtzRouter / JoystickRouter).
    // ========================================================================
    
    void sendMotorCommand(int axis, int32_t steps)
    {
        if (steps == 0) return;
#ifdef CAN_CONTROLLER_CANOPEN
        const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::MOTOR, (uint8_t)axis);
        if (!route || route->where != UC2::RouteEntry::REMOTE)
        {
            log_w("Dial: motor axis %d has no REMOTE route", axis);
            return;
        }
        log_d("Dial motor: axis=%d steps=%d -> node 0x%02X sub %u", axis, steps, route->nodeId, route->subAxis);
        SdoEmit::motor(route->nodeId, route->subAxis, steps, config.motorSpeed,
                       (uint32_t)MAX_ACCELERATION_A, /*isAbs*/false, /*isForever*/false, /*isStop*/false);
#endif
    }
    
    void sendLaserCommand(int laserId, int intensity)
    {
#ifdef CAN_CONTROLLER_CANOPEN
        const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::LASER, (uint8_t)laserId);
        if (!route || route->where != UC2::RouteEntry::REMOTE)
        {
            log_w("Dial: laser %d has no REMOTE route", laserId);
            return;
        }
        log_d("Dial laser: id=%d value=%d -> node 0x%02X sub %u", laserId, intensity, route->nodeId, route->subAxis);
        SdoEmit::laser(route->nodeId, route->subAxis, (uint16_t)intensity);
#endif
    }

    void sendLedCommand()
    {
#ifdef CAN_CONTROLLER_CANOPEN
        const auto* route = UC2::RoutingTable::find(UC2::RouteEntry::LED, 0);
        if (!route || route->where != UC2::RouteEntry::REMOTE)
        {
            log_w("Dial: LED has no REMOTE route");
            return;
        }
        uint32_t rgb = ((uint32_t)illumValue[ILLUM_LED_R] << 16) |
                       ((uint32_t)illumValue[ILLUM_LED_G] << 8)  |
                        (uint32_t)illumValue[ILLUM_LED_B];
        bool on = illumOn[ILLUM_LED_RGB];
        log_d("Dial LED: on=%d rgb=0x%06X -> node 0x%02X", on, rgb, route->nodeId);
        SdoEmit::led(route->nodeId, on, rgb);
#endif
    }

    // Push the current state of one illumination channel to the bus.
    static void sendIllum(int ch)
    {
        if (isLedChannel(ch)) sendLedCommand();
        else                  sendLaserCommand(ch, illumOn[ch] ? illumValue[ch] : 0);
    }

    // ========================================================================
    // Display Functions
    // ========================================================================
    
    void drawMotorScreen()
    {
#ifdef M5DIAL
        M5Dial.Display.fillScreen(COLOR_BG);
        
        uint16_t axisColor = getAxisColor(currentAxis);
        int increment = MOTOR_INCREMENTS[motorIncrementIndex];
        
        // Draw decorative ring around edge
        M5Dial.Display.drawCircle(CENTER_X, CENTER_Y, 115, axisColor);
        M5Dial.Display.drawCircle(CENTER_X, CENTER_Y, 114, axisColor);
        
        // Draw large axis letter in center
        M5Dial.Display.setTextColor(axisColor);
        M5Dial.Display.setTextDatum(middle_center);
        M5Dial.Display.setTextSize(3);
        M5Dial.Display.drawString(getAxisName(currentAxis), CENTER_X, CENTER_Y - 20);
        
        // Draw increment value below axis
        M5Dial.Display.setTextColor(COLOR_TEXT);
        M5Dial.Display.setTextSize(1.5);
        String incText = "Step: " + String(increment);
        M5Dial.Display.drawString(incText, CENTER_X, CENTER_Y + 40);
        
        // Draw mode indicator at bottom
        M5Dial.Display.setTextSize(0.8);
        M5Dial.Display.setTextColor(COLOR_ACCENT);
        M5Dial.Display.drawString("MOTOR", CENTER_X, DISPLAY_HEIGHT - 25);
        
        // Draw axis indicators around the ring
        M5Dial.Display.setTextSize(0.8);
        const char* axes[] = {"X", "Y", "Z", "A"};
        const MotorAxis axisEnums[] = {MotorAxis::X, MotorAxis::Y, MotorAxis::Z, MotorAxis::A};
        const int angles[] = {0, 90, 180, 270};  // Degrees for each axis position
        
        for (int i = 0; i < 4; i++)
        {
            float rad = (angles[i] - 90) * PI / 180.0;  // -90 to start from top
            int x = CENTER_X + (int)(85 * cos(rad));
            int y = CENTER_Y + (int)(85 * sin(rad));
            
            if (axisEnums[i] == currentAxis)
            {
                M5Dial.Display.setTextColor(axisColor);
                M5Dial.Display.fillCircle(x, y, 15, axisColor);
                M5Dial.Display.setTextColor(COLOR_BG);
            }
            else
            {
                M5Dial.Display.setTextColor(COLOR_INACTIVE);
            }
            M5Dial.Display.drawString(axes[i], x, y);
        }
        M5Dial.Display.setTextColor(COLOR_TEXT);  // Reset color
#endif
    }
    
    void drawIlluminationScreen()
    {
#ifdef M5DIAL
        M5Dial.Display.fillScreen(COLOR_BG);
        
        uint16_t mainColor = illumOn[onIndex(currentIllum)] ? COLOR_ILLUM_ON : COLOR_ILLUM_OFF;
        int increment = ILLUM_INCREMENTS[illumIncrementIndex];
        
        // Draw decorative ring
        M5Dial.Display.drawCircle(CENTER_X, CENTER_Y, 115, mainColor);
        M5Dial.Display.drawCircle(CENTER_X, CENTER_Y, 114, mainColor);
        
        // Draw illumination icon (simple sun/bulb representation)
        M5Dial.Display.setTextColor(mainColor);
        M5Dial.Display.setTextDatum(middle_center);
        M5Dial.Display.setTextSize(1);
        M5Dial.Display.drawString(illumOn[onIndex(currentIllum)] ? "ON" : "OFF", CENTER_X, CENTER_Y - 50);
        
        // Draw illumination value in large text
        M5Dial.Display.setTextSize(2.5);
        M5Dial.Display.setTextColor(COLOR_TEXT);
        String valueText = String(illumValue[currentIllum]);
        M5Dial.Display.drawString(valueText, CENTER_X, CENTER_Y);
        
        // Draw progress arc showing illumination level
        int arcAngle = map(illumValue[currentIllum], 0, channelMax(currentIllum), 0, 360);
        if (arcAngle > 0)
        {
            for (int a = -90; a < -90 + arcAngle && a < 270; a += 2)
            {
                float rad = a * PI / 180.0;
                int x = CENTER_X + (int)(100 * cos(rad));
                int y = CENTER_Y + (int)(100 * sin(rad));
                M5Dial.Display.fillCircle(x, y, 3, mainColor);
            }
        }
        
        // Draw increment value
        M5Dial.Display.setTextSize(1);
        M5Dial.Display.setTextColor(COLOR_TEXT);
        String incText = "Step: " + String(increment);
        M5Dial.Display.drawString(incText, CENTER_X, CENTER_Y + 50);
        
        // Draw mode indicator with selected channel
        M5Dial.Display.setTextSize(0.8);
        M5Dial.Display.setTextColor(COLOR_ACCENT);
        M5Dial.Display.drawString(channelName(currentIllum), CENTER_X, DISPLAY_HEIGHT - 25);
#endif
    }
    
    void updateDisplay()
    {
#ifdef M5DIAL
        if (currentMode == DialMode::MOTOR)
        {
            drawMotorScreen();
        }
        else
        {
            drawIlluminationScreen();
        }
#endif
    }

    // ========================================================================
    // State Getters
    // ========================================================================
    
    DialMode getCurrentMode() { return currentMode; }
    MotorAxis getCurrentAxis() { return currentAxis; }
    int getCurrentIllumChannel() { return currentIllum; }
    int getCurrentIncrement() 
    { 
        return currentMode == DialMode::MOTOR ? 
               MOTOR_INCREMENTS[motorIncrementIndex] : 
               ILLUM_INCREMENTS[illumIncrementIndex]; 
    }
    int getIlluminationValue() { return illumValue[currentIllum]; }
    bool isIlluminationOn() { return illumOn[onIndex(currentIllum)]; }

    // ========================================================================
    // Input Handling
    // ========================================================================
    
    void handleShortPress()
    {
        if (currentMode == DialMode::MOTOR)
        {
            // Cycle through motor increments
            motorIncrementIndex = (motorIncrementIndex + 1) % MOTOR_INCREMENT_COUNT;
            log_d("Motor increment changed to: %d", MOTOR_INCREMENTS[motorIncrementIndex]);
        }
        else
        {
            // Toggle selected channel on/off (LED channels share one flag)
            int oi = onIndex(currentIllum);
            illumOn[oi] = !illumOn[oi];
            sendIllum(currentIllum);
            log_d("%s toggled: %s", channelName(currentIllum), illumOn[oi] ? "ON" : "OFF");
        }
        updateDisplay();
    }
    
    void handleLongPress()
    {
        if (currentMode == DialMode::MOTOR)
        {
            // Cycle through motor axes: X -> Y -> Z -> A -> X
            switch (currentAxis)
            {
                case MotorAxis::X: currentAxis = MotorAxis::Y; break;
                case MotorAxis::Y: currentAxis = MotorAxis::Z; break;
                case MotorAxis::Z: currentAxis = MotorAxis::A; break;
                case MotorAxis::A: currentAxis = MotorAxis::X; break;
                default: currentAxis = MotorAxis::X; break;
            }
            log_d("Motor axis changed to: %s", getAxisName(currentAxis));
        }
        else
        {
            // Cycle channels: lasers 0..3 -> LED RGB -> R -> G -> B -> laser 0
            currentIllum = (currentIllum + 1) % ILLUM_CHANNEL_COUNT;
            log_d("Illumination channel changed to: %s", channelName(currentIllum));
        }
        updateDisplay();
    }
    
    void handleEncoderChange(long delta)
    {
        if (currentMode == DialMode::MOTOR)
        {
            // Accumulate steps (will be sent periodically)
            int increment = MOTOR_INCREMENTS[motorIncrementIndex];
            accumulatedSteps += delta * increment;
            // No display update needed for motor mode (we don't show counts)
        }
        else
        {
            // Immediately update illumination value
            int increment = ILLUM_INCREMENTS[illumIncrementIndex];
            int &val = illumValue[currentIllum];
            val += delta * increment;
            
            // Clamp to valid range
            if (val < 0) val = 0;
            if (val > channelMax(currentIllum)) val = channelMax(currentIllum);

            // LED RGB drives all three components together
            if (currentIllum == ILLUM_LED_RGB)
                illumValue[ILLUM_LED_R] = illumValue[ILLUM_LED_G] = illumValue[ILLUM_LED_B] = val;
            
            // Send immediately if this channel is on
            if (illumOn[onIndex(currentIllum)])
            {
                sendIllum(currentIllum);
            }
            updateDisplay();
        }
    }

    // ========================================================================
    // API Functions
    // ========================================================================
    
    int act(cJSON *jsonDocument)
    {
        int qid = cJsonTool::getJsonInt(jsonDocument, "qid");
        
        // Handle mode switching via API
        cJSON *modeItem = cJSON_GetObjectItem(jsonDocument, "mode");
        if (modeItem != nullptr)
        {
            if (strcmp(modeItem->valuestring, "motor") == 0)
            {
                currentMode = DialMode::MOTOR;
                updateDisplay();
            }
            else if (strcmp(modeItem->valuestring, "illumination") == 0)
            {
                currentMode = DialMode::ILLUMINATION;
                updateDisplay();
            }
        }
        
        // Handle axis selection via API
        cJSON *axisItem = cJSON_GetObjectItem(jsonDocument, "axis");
        if (axisItem != nullptr)
        {
            const char* axisStr = axisItem->valuestring;
            if (strcmp(axisStr, "X") == 0 || strcmp(axisStr, "x") == 0) currentAxis = MotorAxis::X;
            else if (strcmp(axisStr, "Y") == 0 || strcmp(axisStr, "y") == 0) currentAxis = MotorAxis::Y;
            else if (strcmp(axisStr, "Z") == 0 || strcmp(axisStr, "z") == 0) currentAxis = MotorAxis::Z;
            else if (strcmp(axisStr, "A") == 0 || strcmp(axisStr, "a") == 0) currentAxis = MotorAxis::A;
            updateDisplay();
        }
        
        // Handle speed configuration via API
        cJSON *speedItem = cJSON_GetObjectItem(jsonDocument, "speed");
        if (speedItem != nullptr)
        {
            config.motorSpeed = speedItem->valueint;
        }
        
        // Handle illumination channel selection via API (0..3 lasers, 4..7 LED RGB/R/G/B)
        cJSON *chItem = cJSON_GetObjectItem(jsonDocument, "channel");
        if (chItem != nullptr && chItem->valueint >= 0 && chItem->valueint < ILLUM_CHANNEL_COUNT)
        {
            currentIllum = chItem->valueint;
            updateDisplay();
        }
        
        // Handle increment setting via API (applies to the current mode)
        cJSON *incItem = cJSON_GetObjectItem(jsonDocument, "increment");
        if (incItem != nullptr)
        {
            int inc = incItem->valueint;
            if (currentMode == DialMode::MOTOR)
            {
                for (int i = 0; i < MOTOR_INCREMENT_COUNT; i++)
                    if (MOTOR_INCREMENTS[i] == inc) { motorIncrementIndex = i; break; }
            }
            else
            {
                for (int i = 0; i < ILLUM_INCREMENT_COUNT; i++)
                    if (ILLUM_INCREMENTS[i] == inc) { illumIncrementIndex = i; break; }
            }
            updateDisplay();
        }
        
        // Handle illumination value setting via API (for the selected channel)
        cJSON *illumItem = cJSON_GetObjectItem(jsonDocument, "illumination");
        if (illumItem != nullptr)
        {
            int &val = illumValue[currentIllum];
            val = illumItem->valueint;
            if (val < 0) val = 0;
            if (val > channelMax(currentIllum)) val = channelMax(currentIllum);
            if (currentIllum == ILLUM_LED_RGB)
                illumValue[ILLUM_LED_R] = illumValue[ILLUM_LED_G] = illumValue[ILLUM_LED_B] = val;
            if (illumOn[onIndex(currentIllum)]) sendIllum(currentIllum);
            updateDisplay();
        }
        
        log_d("dial_act_fct");
        return qid;
    }
    
    cJSON *get(cJSON *jsonDocument)
    {
        cJSON *result = cJSON_CreateObject();
        
        cJSON_AddStringToObject(result, "mode", currentMode == DialMode::MOTOR ? "motor" : "illumination");
        cJSON_AddStringToObject(result, "axis", getAxisName(currentAxis));
        cJSON_AddNumberToObject(result, "channel", currentIllum);
        cJSON_AddStringToObject(result, "channelName", channelName(currentIllum));
        cJSON_AddNumberToObject(result, "increment", getCurrentIncrement());
        cJSON_AddNumberToObject(result, "speed", config.motorSpeed);
        cJSON_AddNumberToObject(result, "illumination", illumValue[currentIllum]);
        cJSON_AddBoolToObject(result, "illuminationOn", illumOn[onIndex(currentIllum)]);
        
        return result;
    }

    // ========================================================================
    // Main Loop
    // ========================================================================
    
    void loop()
    {
#ifdef M5DIAL
        M5Dial.update();
        
        // Handle hardware button (BtnA) - switch between motor and illumination mode
        if (M5Dial.BtnA.wasPressed())
        {
            M5Dial.Speaker.tone(8000, 20);
            currentMode = (currentMode == DialMode::MOTOR) ? DialMode::ILLUMINATION : DialMode::MOTOR;
            log_d("Mode switched to: %s", currentMode == DialMode::MOTOR ? "MOTOR" : "ILLUMINATION");
            updateDisplay();
        }
        
        // Handle encoder rotation. The PCNT counter never drops edges even
        // while this loop is busy redrawing or blocked in an SDO write; we
        // consume whole detents and leave the remainder in lastEncoderPos.
        long newEncoderPos = (long)encoder.getCount();
        long detents = (newEncoderPos - lastEncoderPos) / ENCODER_COUNTS_PER_DETENT;
        if (detents != 0)
        {
            lastEncoderPos += detents * ENCODER_COUNTS_PER_DETENT;
            handleEncoderChange(detents);
        }
        
        // Handle touch screen input
        auto t = M5Dial.Touch.getDetail();
        
        // Touch started
        if (t.state == 3)  // TOUCH_BEGIN
        {
            touchStartTime = millis();
            touchActive = true;
        }
        
        // Touch ended
        if ((t.state == 2 || t.state == 7) && touchActive)  // TOUCH_END or TOUCH_HOLD_END
        {
            touchActive = false;
            unsigned long touchDuration = millis() - touchStartTime;
            
            if (touchDuration >= LONG_PRESS_DURATION_MS)
            {
                // Long press detected
                M5Dial.Speaker.tone(6000, 30);
                handleLongPress();
            }
            else if (touchDuration >= DEBOUNCE_MS)
            {
                // Short press detected
                M5Dial.Speaker.tone(8000, 20);
                handleShortPress();
            }
        }
        
        // Periodically send accumulated motor steps
        if (currentMode == DialMode::MOTOR && accumulatedSteps != 0)
        {
            unsigned long currentTime = millis();
            if (currentTime - lastSendTime >= SEND_INTERVAL_MS)
            {
                int axisIdx = getAxisIndex(currentAxis);
                sendMotorCommand(axisIdx, accumulatedSteps);
                accumulatedSteps = 0;
                lastSendTime = currentTime;
            }
        }
#endif
    }

    // ========================================================================
    // Setup
    // ========================================================================
    
    void setup()
    {
#ifdef M5DIAL
        log_i("Initializing Dial Controller (CANopen originator)");
        
        // Initialize M5Dial - disable external I2C to free up Grove pins for CAN.
        // M5Dial's own Encoder class is NOT used: its ESP32 interrupt table stops
        // at GPIO 39, so on GPIO 40/41 it silently degrades to polling once per
        // loop() and loses edges whenever the loop is busy (display, SDO writes).
        auto cfg = M5.config();
        cfg.external_spk = false;  // Disable external speaker I2C
        cfg.external_rtc = false;  // Disable external RTC I2C
        M5Dial.begin(cfg, /*enableEncoder*/false, /*enableRFID*/false);

        ESP32Encoder::useInternalWeakPullResistors = puType::up;
        encoder.attachFullQuad(ENCODER_PIN_A, ENCODER_PIN_B);
        encoder.setFilter(1023);   // max PCNT glitch filter (~12.8 us @ 80 MHz)
        encoder.clearCount();
        
        // Configure display
        M5Dial.Display.setTextColor(COLOR_TEXT);
        M5Dial.Display.setTextDatum(middle_center);
        M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
        M5Dial.Display.setTextSize(1);
        
        // Initialize encoder position
        lastEncoderPos = (long)encoder.getCount();
        
        // Show startup screen
        M5Dial.Display.fillScreen(COLOR_BG);
        M5Dial.Display.setTextColor(COLOR_ACCENT);
        M5Dial.Display.setTextSize(1.5);
        M5Dial.Display.drawString("UC2 DIAL", CENTER_X, CENTER_Y - 30);
        M5Dial.Display.setTextSize(0.8);
        M5Dial.Display.setTextColor(COLOR_TEXT);
        M5Dial.Display.drawString("CANopen", CENTER_X, CENTER_Y + 10);
        M5Dial.Display.drawString("Initializing...", CENTER_X, CENTER_Y + 40);
        
        delay(1000);
        
        // Draw initial screen
        updateDisplay();
        
        log_i("Dial Controller initialized - Mode: MOTOR, Axis: %s, Increment: %d",
              getAxisName(currentAxis), MOTOR_INCREMENTS[motorIncrementIndex]);
#endif
    }

} // namespace DialController

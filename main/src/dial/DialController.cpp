#include <PinConfig.h>
#include "DialController.h"
#include "Arduino.h"
#include "../../JsonKeys.h"
#include "cJsonTool.h"

#ifdef CAN_SEND_COMMANDS
#include "../can/can_controller.h"
#include "../motor/MotorTypes.h"
#include "../laser/LaserController.h"
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
    
    // Illumination mode state
    static int illuminationValue = 0;               // Current illumination value (0-255)
    static bool illuminationOn = false;             // Illumination on/off state
    static int illumIncrementIndex = 0;             // Index into ILLUM_INCREMENTS array
    
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
    
    // Get CAN ID for axis
    uint8_t getCanIdForAxis(MotorAxis axis)
    {
#ifdef CAN_SEND_COMMANDS
        switch (axis)
        {
            case MotorAxis::X: return pinConfig.CAN_ID_MOT_X;
            case MotorAxis::Y: return pinConfig.CAN_ID_MOT_Y;
            case MotorAxis::Z: return pinConfig.CAN_ID_MOT_Z;
            case MotorAxis::A: return pinConfig.CAN_ID_MOT_A;
            default: return 0;
        }
#else
        return 0;
#endif
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
    // CAN Communication Functions
    // ========================================================================
    
    void sendMotorCommand(int axis, int32_t steps)
    {
#ifdef CAN_SEND_COMMANDS
        if (steps == 0) return;
        
        // Create MotorDataReduced for efficient CAN transmission
        MotorDataReduced motorCmd;
        motorCmd.targetPosition = steps;
        motorCmd.speed = config.motorSpeed;
        motorCmd.isforever = false;
        motorCmd.absolutePosition = false;  // Relative movement
        motorCmd.isStop = false;
        
        uint8_t canId = getCanIdForAxis(static_cast<MotorAxis>(axis));
        
        log_d("Dial sending motor command: axis=%d, canId=%d, steps=%d", axis, canId, steps);
        
        // Send via CAN using reduced motor data format
        int err = can_controller::sendCanMessage(canId, (uint8_t*)&motorCmd, sizeof(MotorDataReduced));
        if (err != 0)
        {
            log_e("Failed to send motor command via CAN");
        }
#endif
    }
    
    void sendLaserCommand(int laserId, int intensity)
    {
#ifdef CAN_SEND_COMMANDS
        LaserData laserCmd;
        laserCmd.LASERid = laserId;
        laserCmd.LASERval = intensity;
        laserCmd.LASERdespeckle = 0;
        laserCmd.LASERdespecklePeriod = 0;
        
        log_d("Dial sending laser command: id=%d, intensity=%d", laserId, intensity);
        
        can_controller::sendLaserDataToCANDriver(laserCmd);
#endif
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
        
        uint16_t mainColor = illuminationOn ? COLOR_ILLUM_ON : COLOR_ILLUM_OFF;
        int increment = ILLUM_INCREMENTS[illumIncrementIndex];
        
        // Draw decorative ring
        M5Dial.Display.drawCircle(CENTER_X, CENTER_Y, 115, mainColor);
        M5Dial.Display.drawCircle(CENTER_X, CENTER_Y, 114, mainColor);
        
        // Draw illumination icon (simple sun/bulb representation)
        M5Dial.Display.setTextColor(mainColor);
        M5Dial.Display.setTextDatum(middle_center);
        M5Dial.Display.setTextSize(1);
        M5Dial.Display.drawString(illuminationOn ? "ON" : "OFF", CENTER_X, CENTER_Y - 50);
        
        // Draw illumination value in large text
        M5Dial.Display.setTextSize(2.5);
        M5Dial.Display.setTextColor(COLOR_TEXT);
        String valueText = String(illuminationValue);
        M5Dial.Display.drawString(valueText, CENTER_X, CENTER_Y);
        
        // Draw progress arc showing illumination level
        int arcAngle = map(illuminationValue, 0, MAX_ILLUMINATION, 0, 360);
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
        
        // Draw mode indicator
        M5Dial.Display.setTextSize(0.8);
        M5Dial.Display.setTextColor(COLOR_ACCENT);
        M5Dial.Display.drawString("ILLUMINATION", CENTER_X, DISPLAY_HEIGHT - 25);
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
    int getCurrentIncrement() 
    { 
        return currentMode == DialMode::MOTOR ? 
               MOTOR_INCREMENTS[motorIncrementIndex] : 
               ILLUM_INCREMENTS[illumIncrementIndex]; 
    }
    int getIlluminationValue() { return illuminationValue; }
    bool isIlluminationOn() { return illuminationOn; }

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
            // Toggle illumination on/off
            illuminationOn = !illuminationOn;
            sendLaserCommand(0, illuminationOn ? illuminationValue : 0);
            log_d("Illumination toggled: %s", illuminationOn ? "ON" : "OFF");
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
            // In illumination mode, cycle through increments
            illumIncrementIndex = (illumIncrementIndex + 1) % ILLUM_INCREMENT_COUNT;
            log_d("Illumination increment changed to: %d", ILLUM_INCREMENTS[illumIncrementIndex]);
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
            illuminationValue += delta * increment;
            
            // Clamp to valid range
            if (illuminationValue < 0) illuminationValue = 0;
            if (illuminationValue > MAX_ILLUMINATION) illuminationValue = MAX_ILLUMINATION;
            
            // Send immediately if illumination is on
            if (illuminationOn)
            {
                sendLaserCommand(0, illuminationValue);
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
        
        // Handle increment setting via API
        cJSON *incItem = cJSON_GetObjectItem(jsonDocument, "increment");
        if (incItem != nullptr)
        {
            int inc = incItem->valueint;
            // Find matching increment index
            for (int i = 0; i < MOTOR_INCREMENT_COUNT; i++)
            {
                if (MOTOR_INCREMENTS[i] == inc)
                {
                    motorIncrementIndex = i;
                    break;
                }
            }
            updateDisplay();
        }
        
        // Handle illumination value setting via API
        cJSON *illumItem = cJSON_GetObjectItem(jsonDocument, "illumination");
        if (illumItem != nullptr)
        {
            illuminationValue = illumItem->valueint;
            if (illuminationValue < 0) illuminationValue = 0;
            if (illuminationValue > MAX_ILLUMINATION) illuminationValue = MAX_ILLUMINATION;
            if (illuminationOn) sendLaserCommand(0, illuminationValue);
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
        cJSON_AddNumberToObject(result, "increment", getCurrentIncrement());
        cJSON_AddNumberToObject(result, "speed", config.motorSpeed);
        cJSON_AddNumberToObject(result, "illumination", illuminationValue);
        cJSON_AddBoolToObject(result, "illuminationOn", illuminationOn);
        
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
        
        // Handle encoder rotation
        long newEncoderPos = M5Dial.Encoder.read();
        if (newEncoderPos != lastEncoderPos)
        {
            long delta = newEncoderPos - lastEncoderPos;
            lastEncoderPos = newEncoderPos;
            handleEncoderChange(delta);
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
        log_i("Initializing Dial Controller (CAN Master Mode)");
        
        // Initialize M5Dial - disable external I2C to free up Grove pins for CAN
        auto cfg = M5.config();
        cfg.external_spk = false;  // Disable external speaker I2C
        cfg.external_rtc = false;  // Disable external RTC I2C
        M5Dial.begin(cfg, true, false);
        
        // Configure display
        M5Dial.Display.setTextColor(COLOR_TEXT);
        M5Dial.Display.setTextDatum(middle_center);
        M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
        M5Dial.Display.setTextSize(1);
        
        // Load CAN IDs from pinConfig
#ifdef CAN_SEND_COMMANDS
        config.canIdMotorX = pinConfig.CAN_ID_MOT_X;
        config.canIdMotorY = pinConfig.CAN_ID_MOT_Y;
        config.canIdMotorZ = pinConfig.CAN_ID_MOT_Z;
        config.canIdMotorA = pinConfig.CAN_ID_MOT_A;
        config.canIdLaser = pinConfig.CAN_ID_LASER_0;
		// send some info to the master

#endif
        
        // Initialize encoder position
        lastEncoderPos = M5Dial.Encoder.read();
        
        // Show startup screen
        M5Dial.Display.fillScreen(COLOR_BG);
        M5Dial.Display.setTextColor(COLOR_ACCENT);
        M5Dial.Display.setTextSize(1.5);
        M5Dial.Display.drawString("UC2 DIAL", CENTER_X, CENTER_Y - 30);
        M5Dial.Display.setTextSize(0.8);
        M5Dial.Display.setTextColor(COLOR_TEXT);
        M5Dial.Display.drawString("CAN Master", CENTER_X, CENTER_Y + 10);
        M5Dial.Display.drawString("Initializing...", CENTER_X, CENTER_Y + 40);
        
        delay(1000);
        
        // Draw initial screen
        updateDisplay();
        
        log_i("Dial Controller initialized - Mode: MOTOR, Axis: X, Increment: %d", MOTOR_INCREMENTS[0]);
#endif
    }

} // namespace DialController

/**
 * LinearEncoderControllerSimple.cpp
 * 
 * Simplified encoder-based motion control for single axis operation.
 * Ported from testFeedbackStepper standalone implementation.
 * 
 * Key features:
 * - Two-stage PID control (aggressive far, precise near)
 * - 1kHz control loop for fast response
 * - Stall-based homing with automatic zero position
 * - Works with encoder counts directly (no unit conversion)
 * - Single axis focus (one ESP32 per motor-encoder pair)
 */

#include "LinearEncoderControllerSimple.h"
#include "../config/ConfigController.h"
#include "esp_task_wdt.h"
#include <Preferences.h>

#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
#endif

#include "PCNTEncoderController.h"
#include "InterruptController.h"
#include "JsonKeys.h"

namespace LinearEncoderController
{
    // ============= Module State =============
    
    // Encoder data for X-axis (index 1)
    static LinearEncoderDataSimple encoderData;
    
    // Function pointers to motor control
    static FocusMotor::MotorData **(*getData)() = nullptr;
    static void (*startStepper)(int, int) = nullptr;
    
    // Control loop timing
    static const uint32_t CONTROL_INTERVAL_US = 1000; // 1ms = 1kHz
    static uint32_t lastControlTimeUs = 0;
    
    // Task handle for background operation (optional)
    static TaskHandle_t motionTask = nullptr;
    static volatile bool motionTaskRunning = false;
    
    // ============= ISR for interrupt-based encoder =============
    
    static void IRAM_ATTR encoderISR(uint8_t pin)
    {
        // Quadrature decoding for X-axis encoder
        bool pinA = digitalRead(pinConfig.ENC_X_A);
        bool pinB = digitalRead(pinConfig.ENC_X_B);
        
        if (pin == pinConfig.ENC_X_A) {
            // A changed
            if ((pinA != pinB) ^ encoderData.encoderDirection)
                encoderData.encoderCount++;
            else
                encoderData.encoderCount--;
        } else {
            // B changed
            if ((pinA == pinB) ^ encoderData.encoderDirection)
                encoderData.encoderCount++;
            else
                encoderData.encoderCount--;
        }
    }
    
    // ============= Encoder Position Functions =============
    
    int32_t getEncoderPosition(int encoderIndex)
    {
        if (encoderIndex != 1) return 0; // Only X-axis supported
        
        // Use PCNT if available, otherwise interrupt-based
        if (PCNTEncoderController::isPCNTAvailable()) {
            return (int32_t)PCNTEncoderController::getPCNTCount(1);
        }
        return encoderData.encoderCount;
    }
    
    void setEncoderPosition(int encoderIndex, int32_t position)
    {
        if (encoderIndex != 1) return;
        
        if (PCNTEncoderController::isPCNTAvailable()) {
            PCNTEncoderController::setCurrentPosition(1, (float)position);
        }
        encoderData.encoderCount = position;
    }
    
    void zeroEncoder(int encoderIndex)
    {
        setEncoderPosition(encoderIndex, 0);
    }
    
    // ============= Motion Control Core =============
    
    /**
     * Execute precision motion blocking.
     * 1kHz PID control loop with two-stage gains.
     */
    static void executePrecisionMotionBlocking(int axisIndex)
    {
        if (axisIndex != 1 || !getData) return;
        
        int32_t startPos = getEncoderPosition(1);
        int32_t target = encoderData.targetPosition;
        
        log_i("Precision motion: start=%d, target=%d, distance=%d", 
              startPos, target, target - startPos);
        
        // Initialize PID controller
        encoderData.pid.setTargetPosition(target, startPos);
        encoderData.isMovePrecise = true;
        
        // Start motor in continuous mode
        getData()[axisIndex]->isforever = true;
        getData()[axisIndex]->speed = 0;
        startStepper(axisIndex, 0);
        
        // Motion timeout calculation
        int32_t distance = abs(target - startPos);
        float safetyMargin = 5.0f; // seconds
        unsigned long maxTime = (unsigned long)((distance / encoderData.pid.maxVelocity) * 1000.0f) 
                                + (safetyMargin * 1000.0f);
        maxTime = max(maxTime, 3000UL); // Minimum 3 seconds
        
        unsigned long startTime = millis();
        unsigned long lastWatchdogReset = millis();
        
        // Main control loop
        while (encoderData.isMovePrecise && (millis() - startTime) < maxTime)
        {
            // Read encoder at maximum speed
            int32_t currentPos = getEncoderPosition(1);
            
            // Compute PID velocity command
            float velocityCmd = encoderData.pid.compute(currentPos);
            
            // Check if motion complete
            if (encoderData.pid.isComplete()) {
                log_i("Motion complete: pos=%d, target=%d, error=%d", 
                      currentPos, target, target - currentPos);
                break;
            }
            
            // Check for stall error
            if (encoderData.pid.isStallError()) {
                log_e("Motion aborted: stall error at pos=%d", currentPos);
                break;
            }
            
            // Debug output
            if (encoderData.enableDebug && (millis() - encoderData.lastDebugTime > encoderData.debugInterval)) {
                log_i("pos=%d, target=%d, err=%d, vel=%.1f", 
                      currentPos, target, target - currentPos, velocityCmd);
                encoderData.lastDebugTime = millis();
            }
            
            // Update motor speed
            getData()[axisIndex]->speed = (int)velocityCmd;
            getData()[axisIndex]->isforever = true;
            startStepper(axisIndex, 0);
            
            // Watchdog reset
            if ((millis() - lastWatchdogReset) > 100) {
                esp_task_wdt_reset();
                lastWatchdogReset = millis();
            }
            
            // Control loop delay (1kHz)
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        
        // Stop motor
        getData()[axisIndex]->speed = 0;
        getData()[axisIndex]->isforever = false;
        FocusMotor::stopStepper(axisIndex);
        encoderData.isMovePrecise = false;
        encoderData.pid.stop();
        
        // Final position
        int32_t finalPos = getEncoderPosition(1);
        log_i("Motion finished: final=%d, target=%d, error=%d", 
              finalPos, target, target - finalPos);
        
        // Save position
        saveEncoderPosition(1); // TODO: check if the motor returns this count or the stepper count
    }
    
    /**
     * Execute stall-based homing.
     * Motor runs until stall detected, then position is set to 0.
     */
    static void executeHomingBlocking(int axisIndex, int speed)
    {
        if (axisIndex != 1 || !getData) return;
        
        log_i("Starting stall-based homing at speed=%d", speed);
        
        encoderData.isHoming = true;
        
        // Start motor in continuous mode
        getData()[axisIndex]->isforever = true;
        getData()[axisIndex]->speed = speed;
        startStepper(axisIndex, 0);
        
        // Stall detection parameters
        const int32_t stallThreshold = encoderData.pid.stallNoiseThreshold;
        const uint32_t stallTimeout = encoderData.pid.stallTimeMs;
        const unsigned long maxHomingTime = 30000; // 30 second safety timeout
        
        int32_t lastPosition = getEncoderPosition(1);
        unsigned long lastMoveTime = millis();
        unsigned long startTime = millis();
        unsigned long lastWatchdogReset = millis();
        
        while (encoderData.isHoming && (millis() - startTime) < maxHomingTime)
        {
            int32_t currentPos = getEncoderPosition(1);
            int32_t posChange = abs(currentPos - lastPosition);
            
            // Check if motor is moving
            if (posChange > stallThreshold) {
                lastPosition = currentPos;
                lastMoveTime = millis();
            } else if ((millis() - lastMoveTime) > stallTimeout) {
                // Stall detected - mechanical limit reached
                log_i("Homing complete: stall detected at pos=%d", currentPos);
                break;
            }
            
            // Watchdog reset
            if ((millis() - lastWatchdogReset) > 100) {
                esp_task_wdt_reset();
                lastWatchdogReset = millis();
            }
            
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        
        // Stop motor
        getData()[axisIndex]->speed = 0;
        getData()[axisIndex]->isforever = false;
        FocusMotor::stopStepper(axisIndex);
        encoderData.isHoming = false;
        
        // Set position to zero
        zeroEncoder(1);
        
        // Save zero position
        saveEncoderPosition(1);
        
        log_i("Homing finished: position zeroed");
    }
    
    // ============= Public Motion Interface =============
    
    void movePrecise(int32_t targetPosition, bool isAbsolute, int axisIndex)
    {
        if (axisIndex != 1) return;
        
        int32_t target = targetPosition;
        if (!isAbsolute) {
            target = getEncoderPosition(1) + targetPosition;
        }
        
        encoderData.targetPosition = target;
        executePrecisionMotionBlocking(axisIndex);
    }
    
    void homeAxis(int speed, int axisIndex)
    {
        if (axisIndex != 1) return;
        executeHomingBlocking(axisIndex, speed);
    }
    
    void stopMotion(int axisIndex)
    {
        if (axisIndex != 1) return;
        
        encoderData.isMovePrecise = false;
        encoderData.isHoming = false;
        encoderData.pid.stop();
        
        if (getData) {
            getData()[axisIndex]->speed = 0;
            getData()[axisIndex]->isforever = false;
            FocusMotor::stopStepper(axisIndex);
        }
    }
    
    bool isMotionActive(int axisIndex)
    {
        if (axisIndex != 1) return false;
        return encoderData.isMovePrecise || encoderData.isHoming;
    }
    
    // ============= JSON Interface =============
    
    int act(cJSON *j)
    {
        int qid = cJsonTool::getJsonInt(j, "qid");
        
        // Move precise: {"task":"/linearencoder_act", "moveP":{"position":1000, "isabs":1}}
        // Simplified: {"task":"/linearencoder_act", "moveP":{"position":1000}}
        cJSON *moveP = cJSON_GetObjectItem(j, key_linearencoder_moveprecise);
        if (moveP != NULL) {
            int32_t position = 0;
            bool isAbsolute = true;
            
            cJSON *pos = cJSON_GetObjectItem(moveP, key_linearencoder_position);
            if (pos) position = pos->valueint;
            
            cJSON *isabs = cJSON_GetObjectItem(moveP, key_isabs);
            if (isabs) isAbsolute = isabs->valueint;
            
            // Optional: Configure PID from JSON
            cJSON *kpFar = cJSON_GetObjectItem(moveP, "kpFar");
            if (kpFar) encoderData.pid.kpFar = kpFar->valuedouble;
            
            cJSON *kpNear = cJSON_GetObjectItem(moveP, "kpNear");
            if (kpNear) encoderData.pid.kpNear = kpNear->valuedouble;
            
            cJSON *ki = cJSON_GetObjectItem(moveP, key_linearencoder_ci);
            if (ki) encoderData.pid.kiNear = ki->valuedouble;
            
            cJSON *kd = cJSON_GetObjectItem(moveP, key_linearencoder_cd);
            if (kd) {
                encoderData.pid.kdFar = kd->valuedouble;
                encoderData.pid.kdNear = kd->valuedouble;
            }
            
            cJSON *maxVel = cJSON_GetObjectItem(moveP, key_speed);
            if (maxVel) encoderData.pid.maxVelocity = abs(maxVel->valueint);
            
            cJSON *debug = cJSON_GetObjectItem(moveP, key_linearencoder_debug);
            if (debug) encoderData.enableDebug = debug->valueint;
            
            log_i("moveP: pos=%d, abs=%d, kpF=%.1f, kpN=%.1f", 
                  position, isAbsolute, encoderData.pid.kpFar, encoderData.pid.kpNear);
            
            movePrecise(position, isAbsolute, 1);
        }
        
        // Home: {"task":"/linearencoder_act", "home":{"speed":-5000}}
        cJSON *home = cJSON_GetObjectItem(j, key_linearencoder_home);
        if (home != NULL) {
            int speed = -5000; // Default homing speed
            
            cJSON *spd = cJSON_GetObjectItem(home, key_speed);
            if (spd) speed = spd->valueint;
            
            log_i("home: speed=%d", speed);
            homeAxis(speed, 1);
        }
        
        // Zero: {"task":"/linearencoder_act", "zero":1}
        cJSON *zero = cJSON_GetObjectItem(j, "zero");
        if (zero != NULL && zero->valueint) {
            zeroEncoder(1);
            log_i("Encoder zeroed");
        }
        
        // Stop: {"task":"/linearencoder_act", "stop":1}
        cJSON *stop = cJSON_GetObjectItem(j, "stop");
        if (stop != NULL && stop->valueint) {
            stopMotion(1);
            log_i("Motion stopped");
        }
        
        // Configure: {"task":"/linearencoder_act", "config":{...}}
        cJSON *config = cJSON_GetObjectItem(j, "config");
        if (config != NULL) {
            // Two-stage PID configuration
            cJSON *kpFar = cJSON_GetObjectItem(config, "kpFar");
            if (kpFar) encoderData.pid.kpFar = kpFar->valuedouble;
            
            cJSON *kpNear = cJSON_GetObjectItem(config, "kpNear");
            if (kpNear) encoderData.pid.kpNear = kpNear->valuedouble;
            
            cJSON *kiFar = cJSON_GetObjectItem(config, "kiFar");
            if (kiFar) encoderData.pid.kiFar = kiFar->valuedouble;
            
            cJSON *kiNear = cJSON_GetObjectItem(config, "kiNear");
            if (kiNear) encoderData.pid.kiNear = kiNear->valuedouble;
            
            cJSON *kdFar = cJSON_GetObjectItem(config, "kdFar");
            if (kdFar) encoderData.pid.kdFar = kdFar->valuedouble;
            
            cJSON *kdNear = cJSON_GetObjectItem(config, "kdNear");
            if (kdNear) encoderData.pid.kdNear = kdNear->valuedouble;
            
            cJSON *nearThresh = cJSON_GetObjectItem(config, "nearThreshold");
            if (nearThresh) encoderData.pid.nearThreshold = nearThresh->valueint;
            
            cJSON *maxVel = cJSON_GetObjectItem(config, "maxVelocity");
            if (maxVel) encoderData.pid.maxVelocity = maxVel->valuedouble;
            
            cJSON *minVel = cJSON_GetObjectItem(config, "minVelocity");
            if (minVel) encoderData.pid.minVelocity = minVel->valuedouble;
            
            cJSON *posTol = cJSON_GetObjectItem(config, "positionTolerance");
            if (posTol) encoderData.pid.positionTolerance = posTol->valueint;
            
            // Stall detection
            cJSON *stallNoise = cJSON_GetObjectItem(config, "stallNoiseThreshold");
            if (stallNoise) encoderData.pid.stallNoiseThreshold = stallNoise->valueint;
            
            cJSON *stallTime = cJSON_GetObjectItem(config, "stallTimeMs");
            if (stallTime) encoderData.pid.stallTimeMs = stallTime->valueint;
            
            cJSON *stallRetries = cJSON_GetObjectItem(config, "stallMaxRetries");
            if (stallRetries) encoderData.pid.stallMaxRetries = stallRetries->valueint;
            
            // Encoder direction
            cJSON *encDir = cJSON_GetObjectItem(config, "encdir");
            if (encDir) {
                encoderData.encoderDirection = encDir->valueint;
                // Save to preferences
                Preferences prefs;
                prefs.begin("UC2_ENC", false);
                prefs.putBool("encdir1", encoderData.encoderDirection);
                prefs.end();
            }
            
            log_i("Config updated: kpF=%.1f, kpN=%.1f, ki=%.1f, nearThresh=%d",
                  encoderData.pid.kpFar, encoderData.pid.kpNear, 
                  encoderData.pid.kiNear, encoderData.pid.nearThreshold);
        }
        
        return qid;
    }
    
    cJSON *get(cJSON *docin)
    {
        cJSON *doc = cJSON_CreateObject();
        
        // Current position in encoder counts
        int32_t pos = getEncoderPosition(1);
        cJSON_AddNumberToObject(doc, "position", pos);
        
        // Motion state
        cJSON_AddBoolToObject(doc, "isMoving", isMotionActive(1));
        cJSON_AddBoolToObject(doc, "isHoming", encoderData.isHoming);
        
        // Target position
        if (encoderData.isMovePrecise) {
            cJSON_AddNumberToObject(doc, "target", encoderData.targetPosition);
        }
        
        // Stall state
        cJSON_AddBoolToObject(doc, "stallError", encoderData.pid.isStallError());
        cJSON_AddNumberToObject(doc, "stallRetries", encoderData.pid.getStallRetryCount());
        
        // PCNT availability
        cJSON_AddBoolToObject(doc, "pcntAvailable", isPCNTAvailable());
        
        // Configuration
        cJSON *config = cJSON_CreateObject();
        cJSON_AddNumberToObject(config, "kpFar", encoderData.pid.kpFar);
        cJSON_AddNumberToObject(config, "kpNear", encoderData.pid.kpNear);
        cJSON_AddNumberToObject(config, "kiNear", encoderData.pid.kiNear);
        cJSON_AddNumberToObject(config, "kdNear", encoderData.pid.kdNear);
        cJSON_AddNumberToObject(config, "nearThreshold", encoderData.pid.nearThreshold);
        cJSON_AddNumberToObject(config, "maxVelocity", encoderData.pid.maxVelocity);
        cJSON_AddNumberToObject(config, "positionTolerance", encoderData.pid.positionTolerance);
        cJSON_AddBoolToObject(config, "encdir", encoderData.encoderDirection);
        cJSON_AddItemToObject(doc, "config", config);
        
        return doc;
    }
    
    // ============= Configuration Functions =============
    
    LinearEncoderDataSimple* getEncoderData(int axisIndex)
    {
        if (axisIndex != 1) return nullptr;
        return &encoderData;
    }
    
    void configurePID(int axisIndex, float kpFar, float kpNear, float ki, float kd, int32_t nearThreshold)
    {
        if (axisIndex != 1) return;
        
        encoderData.pid.kpFar = kpFar;
        encoderData.pid.kpNear = kpNear;
        encoderData.pid.kiFar = 0.0f;  // No integral in far mode
        encoderData.pid.kiNear = ki;
        encoderData.pid.kdFar = kd;
        encoderData.pid.kdNear = kd;
        encoderData.pid.nearThreshold = nearThreshold;
    }
    
    void configureStallDetection(int axisIndex, int32_t noiseThreshold, uint32_t timeoutMs, uint8_t maxRetries)
    {
        if (axisIndex != 1) return;
        
        encoderData.pid.stallNoiseThreshold = noiseThreshold;
        encoderData.pid.stallTimeMs = timeoutMs;
        encoderData.pid.stallMaxRetries = maxRetries;
    }
    
    void configureVelocity(int axisIndex, float maxVelocity, float minVelocity)
    {
        if (axisIndex != 1) return;
        
        encoderData.pid.maxVelocity = maxVelocity;
        encoderData.pid.minVelocity = minVelocity;
    }
    
    void setEncoderDirection(int axisIndex, bool inverted)
    {
        if (axisIndex != 1) return;
        encoderData.encoderDirection = inverted;
    }
    
    void setMotorDirection(int axisIndex, bool inverted)
    {
        if (axisIndex != 1 || !getData) return;
        getData()[axisIndex]->directionPinInverted = inverted;
    }
    
    // ============= Persistence =============
    
    void saveEncoderPosition(int axisIndex)
    {
        if (axisIndex != 1) return;
        
        Preferences prefs;
        prefs.begin("UC2_ENC", false);
        prefs.putInt("encpos1", getEncoderPosition(1));
        prefs.end();
        log_i("Saved encoder position: %d", getEncoderPosition(1));
    }
    
    void loadEncoderPosition(int axisIndex)
    {
        if (axisIndex != 1) return;
        
        Preferences prefs;
        prefs.begin("UC2_ENC", true);
        int32_t savedPos = prefs.getInt("encpos1", 0);
        prefs.end();
        
        setEncoderPosition(1, savedPos);
        log_i("Loaded encoder position: %d", savedPos);
    }
    
    // ============= Debug =============
    
    void setDebugOutput(int axisIndex, bool enabled, unsigned long intervalMs)
    {
        if (axisIndex != 1) return;
        
        encoderData.enableDebug = enabled;
        encoderData.debugInterval = intervalMs;
    }
    
    bool isPCNTAvailable()
    {
        return PCNTEncoderController::isPCNTAvailable();
    }
    
    // ============= Setup and Loop =============
    
    void setup()
    {
#ifdef MOTOR_CONTROLLER
        getData = FocusMotor::getData;
        startStepper = FocusMotor::startStepper;
#endif
        
        log_i("LinearEncoderController Simple setup");
        
        // Initialize encoder data
        encoderData.linearencoderID = 1; // X-axis
        encoderData.encoderCount = 0;
        
        // Load encoder direction from preferences
        Preferences prefs;
        prefs.begin("UC2_ENC", true);
        encoderData.encoderDirection = prefs.getBool("encdir1", false);
        prefs.end();
        
        // Initialize PCNT if available
        if (PCNTEncoderController::isPCNTAvailable()) {
            log_i("Using PCNT hardware encoder interface");
            PCNTEncoderController::setup();
        } else {
            log_i("Using interrupt-based encoder interface");
            
            // Configure encoder pins
            if (pinConfig.ENC_X_A >= 0 && pinConfig.ENC_X_B >= 0) {
                pinMode(pinConfig.ENC_X_A, INPUT_PULLUP);
                pinMode(pinConfig.ENC_X_B, INPUT_PULLUP);
                
                InterruptController::addInterruptListner(
                    pinConfig.ENC_X_A, 
                    (void (*)(uint8_t))&encoderISR, 
                    GPIO_INTR_ANYEDGE);
                InterruptController::addInterruptListner(
                    pinConfig.ENC_X_B, 
                    (void (*)(uint8_t))&encoderISR, 
                    GPIO_INTR_ANYEDGE);
                
                log_i("Encoder pins configured: A=%d, B=%d", 
                      pinConfig.ENC_X_A, pinConfig.ENC_X_B);
            }
        }
        
        // Load saved position
        loadEncoderPosition(1);
        
        log_i("Encoder setup complete, position=%d, encdir=%d", 
              getEncoderPosition(1), encoderData.encoderDirection);
    }
    
    void loop()
    {
        // Non-blocking - all motion control is done in blocking functions
        // or can be moved to a task for background operation
    }

} // namespace LinearEncoderController

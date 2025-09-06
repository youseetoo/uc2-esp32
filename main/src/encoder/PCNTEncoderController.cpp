#include "PCNTEncoderController.h"
#include "LinearEncoderController.h"  // For EncoderInterface enum
#include "esp_log.h"
#include <PinConfig.h>

// Try to include ESP32Encoder library
#ifdef ESP_IDF_VERSION_MAJOR
#if ESP_IDF_VERSION_MAJOR >= 4
    #include <soc/soc_caps.h>
    #include "ESP32Encoder.h"
    #endif
#endif

static const char *TAG = "PCNTEncoder";

// Critical section mutex for encoder access
static portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;

namespace PCNTEncoderController
{

    // ESP32Encoder instances for X-axis only (focus on accuracy)
    static ESP32Encoder* encoders[4] = {nullptr, nullptr, nullptr, nullptr};
    
    // Position offsets for each encoder
    static float positionOffsets[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    // Steps per mm from LinearEncoderData  
    static float mumPerStep[4] = {12.8f, 12.8f, 12.8f, 12.8f}; // TODO: We have 4µm stepsize though I thought 2µm => so 3200steps/250 counts per mm => 12.8 steps per µm
    
    // Count consistency tracking for accuracy validation
    static volatile int64_t lastValidCount[4] = {0, 0, 0, 0};
    static volatile uint32_t countReadCounter[4] = {0, 0, 0, 0};
    
    // Encoder accuracy test function
    void testEncoderAccuracy(int encoderIndex = 1) {
        if (encoderIndex < 1 || encoderIndex > 3 || encoders[encoderIndex] == nullptr) {
            ESP_LOGW(TAG, "Cannot test encoder %d - not available", encoderIndex);
            return;
        }
        
        ESP_LOGI(TAG, "Starting encoder accuracy test for axis %d", encoderIndex);
        
        // Take 10 rapid consecutive readings to check consistency
        int64_t readings[10];
        for (int i = 0; i < 10; i++) {
            portENTER_CRITICAL(&encoderMux);
            readings[i] = encoders[encoderIndex]->getCount();
            portEXIT_CRITICAL(&encoderMux);
            delayMicroseconds(100); // 100µs between readings
        }
        
        // Check for consistency (readings should be identical or show minimal drift)
        int64_t minCount = readings[0], maxCount = readings[0];
        for (int i = 1; i < 10; i++) {
            if (readings[i] < minCount) minCount = readings[i];
            if (readings[i] > maxCount) maxCount = readings[i];
        }
        
        int64_t countVariation = maxCount - minCount;
        ESP_LOGI(TAG, "Encoder %d accuracy test: min=%lld, max=%lld, variation=%lld", 
                 encoderIndex, minCount, maxCount, countVariation);
        
        if (countVariation <= 2) {
            ESP_LOGI(TAG, "Encoder %d: EXCELLENT accuracy (variation ≤ 2 counts)", encoderIndex);
        } else if (countVariation <= 5) {
            ESP_LOGI(TAG, "Encoder %d: GOOD accuracy (variation ≤ 5 counts)", encoderIndex);
        } else {
            ESP_LOGW(TAG, "Encoder %d: POOR accuracy (variation = %lld counts) - check for interference", 
                     encoderIndex, countVariation);
        }
    }
    

    void setup()
    {
        #ifdef USE_PCNT_COUNTER
        ESP_LOGI(TAG, "Setting up optimized ESP32Encoder interface for maximum accuracy");
        
        // Optimize ESP32Encoder for minimum interference and maximum accuracy
        ESP32Encoder::useInternalWeakPullResistors = puType::none;
        
        // Set ISR service to Core 0 with high priority to isolate from serial/main tasks on Core 1
        ESP32Encoder::isrServiceCpuCore = 0;  
        
        // Only configure X-axis encoder for maximum stability and count accuracy
        // Multiple encoders can cause ISR conflicts and count loss
        if (pinConfig.ENC_X_A >= 0 && pinConfig.ENC_X_B >= 0) {
            encoders[1] = new ESP32Encoder();
            
            // Clear any existing state and configure for maximum accuracy
            encoders[1]->clearCount();
            encoders[1]->attachFullQuad(pinConfig.ENC_X_A, pinConfig.ENC_X_B);
            encoders[1]->setCount(0);
            
            // Use minimal filtering for maximum resolution while maintaining stability
            // Filter value 0 = no filtering (most sensitive but can be noisy)
            // Filter value 1-3 = minimal filtering (good balance of accuracy and stability)
            encoders[1]->setFilter(1);  // Minimal filtering for stability without losing accuracy
            
            // Verify encoder is working and test accuracy
            int64_t initialCount = encoders[1]->getCount();
            ESP_LOGI(TAG, "X-axis encoder configured: A=%d, B=%d, filter=1, count=%lld", 
                     pinConfig.ENC_X_A, pinConfig.ENC_X_B, initialCount);
                     
            // Test encoder responsiveness and accuracy
            delay(10);
            int64_t testCount = encoders[1]->getCount();
            ESP_LOGI(TAG, "Encoder responsiveness test: initial=%lld, after_10ms=%lld", initialCount, testCount);
            
            // Run accuracy test
            delay(100); // Let system settle
            testEncoderAccuracy(1);
        } else {
            ESP_LOGW(TAG, "X-axis encoder pins not defined, encoder disabled");
        }
        
        // Only enable Y and Z encoders if specifically requested and pins are defined
        // For now, focus on X-axis only to minimize interference and maximize accuracy
        if (pinConfig.ENC_Y_A >= 0 && pinConfig.ENC_Y_B >= 0) {
            ESP_LOGI(TAG, "Y-axis encoder pins available but not configured (focusing on X-axis accuracy)");
        }
        if (pinConfig.ENC_Z_A >= 0 && pinConfig.ENC_Z_B >= 0) {
            ESP_LOGI(TAG, "Z-axis encoder pins available but not configured (focusing on X-axis accuracy)");
        }
        
        ESP_LOGI(TAG, "ESP32Encoder setup complete - X-axis only for maximum accuracy");
#else
        ESP_LOGW(TAG, "ESP32Encoder not available, encoder interface disabled");
#endif
    }
    
    int64_t getEncoderCount(int encoderIndex)
    {
        if (encoderIndex < 1 || encoderIndex > 3) {
            return 0;
        }
        
#ifdef USE_PCNT_COUNTER
        if (encoders[encoderIndex] != nullptr && encoders[encoderIndex]->isAttached()) {
            // Use critical section to ensure atomic read of count
            // This prevents race conditions between ISR updates and main thread reads
            portENTER_CRITICAL(&encoderMux);
            int64_t count = encoders[encoderIndex]->getCount();
            portEXIT_CRITICAL(&encoderMux);
            
            // Minimal debug logging to avoid serial interference with encoder counting
            // Only log occasionally for debugging purposes, not during normal operation
            static int debugCounter = 0;
            static int64_t lastCount[4] = {0, 0, 0, 0};
            if (++debugCounter % 10000 == 0) {  // Very reduced frequency: Log every 10000 calls only for debugging
                int64_t countDiff = count - lastCount[encoderIndex];
                ESP_LOGI(TAG, "Enc%d: cnt=%lld, diff=%lld", encoderIndex, count, countDiff);
                lastCount[encoderIndex] = count;
            }
            
            // Apply direction based on configuration
            if (encoderIndex == 1 && !pinConfig.ENC_X_encoderDirection) {
                count = -count;
            } else if (encoderIndex == 2 && !pinConfig.ENC_Y_encoderDirection) {
                count = -count;
            } else if (encoderIndex == 3 && !pinConfig.ENC_Z_encoderDirection) {
                count = -count;
            }
            return count;
        }
#endif
        return 0;
    }
    
    void resetEncoderCount(int encoderIndex)
    {
        if (encoderIndex < 1 || encoderIndex > 3) {
            return;
        }
        
#ifdef USE_PCNT_COUNTER
        if (encoders[encoderIndex] != nullptr && encoders[encoderIndex]->isAttached()) {
            encoders[encoderIndex]->setCount(0);
            positionOffsets[encoderIndex] = 0.0f;
        }
#endif
    }
    
    float getCurrentPosition(int encoderIndex)
    {
        if (encoderIndex < 1 || encoderIndex > 3) {
            return 0.0f;
        }
        
        int64_t count = getEncoderCount(encoderIndex);
        return positionOffsets[encoderIndex] + (count * mumPerStep[encoderIndex]);
    }
    
    void setCurrentPosition(int encoderIndex, float offsetPos)
    {
        if (encoderIndex < 1 || encoderIndex > 3) {
            return;
        }
        
        int64_t count = getEncoderCount(encoderIndex);
        positionOffsets[encoderIndex] = offsetPos - (count * mumPerStep[encoderIndex]);
    }
    
    bool isESP32EncoderAvailable()
    {
#ifdef USE_PCNT_COUNTER
        return true;
#else
        return false;
#endif
    }

    // Legacy PCNT function names for backward compatibility
    int16_t getPCNTCount(int encoderIndex)
    {
        int64_t count = getEncoderCount(encoderIndex);
        // Clamp to int16_t range for compatibility
        if (count > 32767) return 32767;
        if (count < -32768) return -32768;
        return (int16_t)count;
    }
    
    void resetPCNTCount(int encoderIndex)
    {
        resetEncoderCount(encoderIndex);
    }
    
    bool isPCNTAvailable()
    {
        return isESP32EncoderAvailable();
    }
}
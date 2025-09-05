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

    // ESP32Encoder instances for each axis (index 1=X, 2=Y, 3=Z)
    static ESP32Encoder* encoders[4] = {nullptr, nullptr, nullptr, nullptr};
    
    // Position offsets for each encoder
    static float positionOffsets[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    // Steps per mm from LinearEncoderData  
    static float mumPerStep[4] = {12.8f, 12.8f, 12.8f, 12.8f}; // TODO: unknown what happens; We have 4µm stepsize though I thought 2µm => so 3200steps/250 counts per mm => 12.8 steps per µm
    

    void setup()
    {
        
        
        // Since FastAccelStepper now uses RMT instead of PCNT, we shouldn't need the dummy encoder workaround
        // But let's be safe and explicitly clear any counters first
        #ifdef USE_PCNT_COUNTER

        ESP_LOGI(TAG, "Setting up ESP32Encoder interface");
        ESP32Encoder::useInternalWeakPullResistors = puType::none;
        ESP_LOGI(TAG, "FastAccelStepper uses RMT - no PCNT conflict expected");
        
        // Configure encoder for X axis if pins are defined
        if (pinConfig.ENC_X_A >= 0 && pinConfig.ENC_X_B >= 0) {
            encoders[1] = new ESP32Encoder();
            
            // Clear any existing state first
            encoders[1]->clearCount();
            
            // Use attachFullQuad for maximum resolution (4x encoding)
            encoders[1]->attachFullQuad(pinConfig.ENC_X_A, pinConfig.ENC_X_B);
            encoders[1]->setCount(0);
            
            // Minimize filter for maximum resolution - start with 0 for no filtering
            encoders[1]->setFilter(0);  // No filtering for maximum sensitivity and resolution
            
            // Get initial count to verify it's working
            int64_t initialCount = encoders[1]->getCount();
            ESP_LOGI(TAG, "ESP32Encoder X-axis configured for pins A=%d, B=%d, filter=0 (max resolution), initial count=%lld", 
                     pinConfig.ENC_X_A, pinConfig.ENC_X_B, initialCount);
        }
        
        // Configure encoder for Y axis if pins are defined  
        if (pinConfig.ENC_Y_A >= 0 && pinConfig.ENC_Y_B >= 0) {
            encoders[2] = new ESP32Encoder();
            encoders[2]->clearCount();
            encoders[2]->attachFullQuad(pinConfig.ENC_Y_A, pinConfig.ENC_Y_B);
            encoders[2]->setCount(0);
            encoders[2]->setFilter(0);  // No filtering for maximum resolution
            
            int64_t initialCount = encoders[2]->getCount();
            ESP_LOGI(TAG, "ESP32Encoder Y-axis configured for pins A=%d, B=%d, filter=0 (max resolution), initial count=%lld", 
                     pinConfig.ENC_Y_A, pinConfig.ENC_Y_B, initialCount);
        }
        
        // Configure encoder for Z axis if pins are defined
        if (pinConfig.ENC_Z_A >= 0 && pinConfig.ENC_Z_B >= 0) {
            encoders[3] = new ESP32Encoder();
            encoders[3]->clearCount();
            encoders[3]->attachFullQuad(pinConfig.ENC_Z_A, pinConfig.ENC_Z_B);
            encoders[3]->setCount(0);
            encoders[3]->setFilter(0);  // No filtering for maximum resolution
            
            int64_t initialCount = encoders[3]->getCount();
            ESP_LOGI(TAG, "ESP32Encoder Z-axis configured for pins A=%d, B=%d, filter=0 (max resolution), initial count=%lld", 
                     pinConfig.ENC_Z_A, pinConfig.ENC_Z_B, initialCount);
        }
        
        ESP_LOGI(TAG, "ESP32Encoder setup complete - using dedicated PCNT units");
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
            // Direct read without critical section for better performance
            int64_t count = encoders[encoderIndex]->getCount();
            
            // Debug logging for troubleshooting resolution issues
            static int debugCounter = 0;
            static int64_t lastCount = 0;
            if (++debugCounter % 500 == 0) {  // Log every 500 calls
                int64_t countDiff = count - lastCount;
                ESP_LOGI(TAG, "Encoder %d: raw_count=%lld, count_diff=%lld, attached=%s", 
                         encoderIndex, count, countDiff,
                         encoders[encoderIndex]->isAttached() ? "YES" : "NO");
                lastCount = count;
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
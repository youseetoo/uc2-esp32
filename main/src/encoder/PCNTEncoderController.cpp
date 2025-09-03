#include "PCNTEncoderController.h"
#include "LinearEncoderController.h"  // For EncoderInterface enum
#include "esp_log.h"
#include <PinConfig.h>

// Try to include ESP32Encoder library
#ifdef ESP_IDF_VERSION_MAJOR
#if ESP_IDF_VERSION_MAJOR >= 4
    #include <soc/soc_caps.h>
    #if SOC_PCNT_SUPPORTED
        #define ESP32_ENCODER_AVAILABLE
        #include "ESP32Encoder.h"
    #endif
#endif
#endif

static const char *TAG = "PCNTEncoder";

namespace PCNTEncoderController
{
#ifdef ESP32_ENCODER_AVAILABLE
    // ESP32Encoder instances for each axis (index 1=X, 2=Y, 3=Z)
    static ESP32Encoder* encoders[4] = {nullptr, nullptr, nullptr, nullptr};
    
    // Position offsets for each encoder
    static float positionOffsets[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    // Steps per mm from LinearEncoderData  
    static float mumPerStep[4] = {1.95f, 1.95f, 1.95f, 1.95f};
#endif
    
    void setup()
    {
        ESP_LOGI(TAG, "Setting up ESP32Encoder interface");
        
#ifdef ESP32_ENCODER_AVAILABLE
        // Configure encoder for X axis if pins are defined
        if (pinConfig.ENC_X_A >= 0 && pinConfig.ENC_X_B >= 0) {
            encoders[1] = new ESP32Encoder();
            encoders[1]->attachFullQuad(pinConfig.ENC_X_A, pinConfig.ENC_X_B);
            encoders[1]->setCount(0);
            ESP_LOGI(TAG, "ESP32Encoder X-axis configured for pins A=%d, B=%d", pinConfig.ENC_X_A, pinConfig.ENC_X_B);
        }
        
        // Configure encoder for Y axis if pins are defined  
        if (pinConfig.ENC_Y_A >= 0 && pinConfig.ENC_Y_B >= 0) {
            encoders[2] = new ESP32Encoder();
            encoders[2]->attachFullQuad(pinConfig.ENC_Y_A, pinConfig.ENC_Y_B);
            encoders[2]->setCount(0);
            ESP_LOGI(TAG, "ESP32Encoder Y-axis configured for pins A=%d, B=%d", pinConfig.ENC_Y_A, pinConfig.ENC_Y_B);
        }
        
        // Configure encoder for Z axis if pins are defined
        if (pinConfig.ENC_Z_A >= 0 && pinConfig.ENC_Z_B >= 0) {
            encoders[3] = new ESP32Encoder();
            encoders[3]->attachFullQuad(pinConfig.ENC_Z_A, pinConfig.ENC_Z_B);
            encoders[3]->setCount(0);
            ESP_LOGI(TAG, "ESP32Encoder Z-axis configured for pins A=%d, B=%d", pinConfig.ENC_Z_A, pinConfig.ENC_Z_B);
        }
#else
        ESP_LOGW(TAG, "ESP32Encoder not available, encoder interface disabled");
#endif
    }
    
    int64_t getEncoderCount(int encoderIndex)
    {
        if (encoderIndex < 1 || encoderIndex > 3) {
            return 0;
        }
        
#ifdef ESP32_ENCODER_AVAILABLE
        if (encoders[encoderIndex] != nullptr && encoders[encoderIndex]->isAttached()) {
            int64_t count = encoders[encoderIndex]->getCount();
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
        
#ifdef ESP32_ENCODER_AVAILABLE
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
#ifdef ESP32_ENCODER_AVAILABLE
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
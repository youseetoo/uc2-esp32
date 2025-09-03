#include "PCNTEncoderController.h"
#include "LinearEncoderController.h"  // For EncoderInterface enum
#include "esp_log.h"
#include <PinConfig.h>

#ifdef USE_PCNT_COUNTER
#define PCNT_AVAILABLE
#endif 

#ifdef ESP_IDF_VERSION_MAJOR
#if ESP_IDF_VERSION_MAJOR >= 4
#include "driver/pcnt.h"
#include "driver/gpio.h"
#else
#undef PCNT_AVAILABLE
#endif
#endif


static const char *TAG = "PCNTEncoder";

namespace PCNTEncoderController
{
    // PCNT unit assignments for each encoder
    // X, Y, Z axes correspond to units 0, 1, 2
    static const pcnt_unit_t PCNT_UNITS[4] = {
        PCNT_UNIT_MAX,  // Index 0 unused (A axis)
        PCNT_UNIT_0,    // X axis  
        PCNT_UNIT_1,    // Y axis
        PCNT_UNIT_2     // Z axis
    };
    
    // Position offsets for each encoder
    static float positionOffsets[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
    // Steps per mm from LinearEncoderData
    static float mumPerStep[4] = {1.95f, 1.95f, 1.95f, 1.95f};
    
    // Configuration for PCNT
    void configurePCNTUnit(pcnt_unit_t unit, int pinA, int pinB, bool direction)
    {
#ifdef PCNT_AVAILABLE
        pcnt_config_t pcnt_config = {};
        pcnt_config.pulse_gpio_num = pinA;
        pcnt_config.ctrl_gpio_num = pinB;
        pcnt_config.channel = PCNT_CHANNEL_0;
        pcnt_config.unit = unit;
        pcnt_config.pos_mode = PCNT_COUNT_INC;
        pcnt_config.neg_mode = PCNT_COUNT_DEC;
        pcnt_config.lctrl_mode = direction ? PCNT_MODE_REVERSE : PCNT_MODE_KEEP;
        pcnt_config.hctrl_mode = direction ? PCNT_MODE_KEEP : PCNT_MODE_REVERSE;
        pcnt_config.counter_h_lim = 32767;
        pcnt_config.counter_l_lim = -32768;
        
        ESP_ERROR_CHECK(pcnt_unit_config(&pcnt_config));
        
        // Configure the second channel for quadrature decoding
        pcnt_config.pulse_gpio_num = pinB;
        pcnt_config.ctrl_gpio_num = pinA;
        pcnt_config.channel = PCNT_CHANNEL_1;
        pcnt_config.pos_mode = PCNT_COUNT_DEC;
        pcnt_config.neg_mode = PCNT_COUNT_INC;
        
        ESP_ERROR_CHECK(pcnt_unit_config(&pcnt_config));
        
        // Initialize counter
        ESP_ERROR_CHECK(pcnt_counter_pause(unit));
        ESP_ERROR_CHECK(pcnt_counter_clear(unit));
        ESP_ERROR_CHECK(pcnt_counter_resume(unit));
        
        ESP_LOGI(TAG, "PCNT unit %d configured for pins A=%d, B=%d", unit, pinA, pinB);
#endif
    }
    
    void setup()
    {
        ESP_LOGI(TAG, "Setting up PCNT encoder interface");
        
#ifdef PCNT_AVAILABLE
        // Configure PCNT for X axis if pins are defined
        if (pinConfig.ENC_X_A >= 0 && pinConfig.ENC_X_B >= 0) {
            configurePCNTUnit(PCNT_UNITS[1], pinConfig.ENC_X_A, pinConfig.ENC_X_B, pinConfig.ENC_X_encoderDirection);
        }
        
        // Configure PCNT for Y axis if pins are defined  
        if (pinConfig.ENC_Y_A >= 0 && pinConfig.ENC_Y_B >= 0) {
            configurePCNTUnit(PCNT_UNITS[2], pinConfig.ENC_Y_A, pinConfig.ENC_Y_B, pinConfig.ENC_Y_encoderDirection);
        }
        
        // Configure PCNT for Z axis if pins are defined
        if (pinConfig.ENC_Z_A >= 0 && pinConfig.ENC_Z_B >= 0) {
            configurePCNTUnit(PCNT_UNITS[3], pinConfig.ENC_Z_A, pinConfig.ENC_Z_B, pinConfig.ENC_Z_encoderDirection);
        }
#else
        ESP_LOGW(TAG, "PCNT not available, PCNT encoder interface disabled");
#endif
    }
    
    int16_t getPCNTCount(int encoderIndex)
    {
        if (encoderIndex < 1 || encoderIndex > 3) {
            return 0;
        }
        
#ifdef PCNT_AVAILABLE
        int16_t count = 0;
        esp_err_t err = pcnt_get_counter_value(PCNT_UNITS[encoderIndex], &count);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to get PCNT count for encoder %d: %s", encoderIndex, esp_err_to_name(err));
            return 0;
        }
        return count;
#else
        return 0;
#endif
    }
    
    void resetPCNTCount(int encoderIndex)
    {
        if (encoderIndex < 1 || encoderIndex > 3) {
            return;
        }
        
#ifdef PCNT_AVAILABLE
        ESP_ERROR_CHECK(pcnt_counter_clear(PCNT_UNITS[encoderIndex]));
        positionOffsets[encoderIndex] = 0.0f;
#endif
    }
    
    float getCurrentPosition(int encoderIndex)
    {
        if (encoderIndex < 1 || encoderIndex > 3) {
            return 0.0f;
        }
        
        int16_t count = getPCNTCount(encoderIndex);
        return positionOffsets[encoderIndex] + (count * mumPerStep[encoderIndex]);
    }
    
    void setCurrentPosition(int encoderIndex, float offsetPos)
    {
        if (encoderIndex < 1 || encoderIndex > 3) {
            return;
        }
        
        int16_t count = getPCNTCount(encoderIndex);
        positionOffsets[encoderIndex] = offsetPos - (count * mumPerStep[encoderIndex]);
    }
    
    bool isPCNTAvailable()
    {
#ifdef PCNT_AVAILABLE
        return true;
#else
        return false;
#endif
    }
}
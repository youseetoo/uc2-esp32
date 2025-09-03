#pragma once
#include "Arduino.h"

#ifdef USE_PCNT_COUNTER
#define PCNT_AVAILABLE
enum EncoderInterface;
#endif 

#ifdef ESP_IDF_VERSION_MAJOR
#if ESP_IDF_VERSION_MAJOR >= 4
#include "driver/pcnt.h"
#include "driver/gpio.h"
#else
#undef PCNT_AVAILABLE
#endif
#endif


// Forward declaration to avoid circular dependency
#include "LinearEncoderController.h"

namespace PCNTEncoderController
{
    // Initialize PCNT for encoder channels
    void setup();
    
    // Read current position from PCNT
    float getCurrentPosition(int encoderIndex);
    
    // Set current position offset for PCNT
    void setCurrentPosition(int encoderIndex, float offsetPos);
    
    // Get current encoder count from PCNT unit
    int16_t getPCNTCount(int encoderIndex);
    
    // Reset PCNT counter
    void resetPCNTCount(int encoderIndex);
    
    // Check if PCNT is available and configured
    bool isPCNTAvailable();
};
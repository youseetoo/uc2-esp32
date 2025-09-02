#pragma once
#include "Arduino.h"

#include "LinearEncoderController.h"
#ifdef ESP_IDF_VERSION_MAJOR
#if ESP_IDF_VERSION_MAJOR >= 4
#include "driver/pcnt.h"
#include "driver/gpio.h"
#define PCNT_AVAILABLE
#endif
#endif

// Forward declaration to avoid circular dependency
enum EncoderInterface;

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
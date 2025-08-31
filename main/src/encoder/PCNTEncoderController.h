#pragma once
#include "Arduino.h"

#ifdef ESP_IDF_VERSION_MAJOR
#if ESP_IDF_VERSION_MAJOR >= 4
#include "driver/pcnt.h"
#include "driver/gpio.h"
#define PCNT_AVAILABLE
#endif
#endif

#include "LinearEncoderController.h"

// Encoder interface selection enum
enum EncoderInterface {
    ENCODER_INTERRUPT_BASED = 0,  // Default interrupt-based implementation
    ENCODER_PCNT_BASED = 1        // PCNT hardware-based implementation
};

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
#pragma once
#include "Arduino.h"

// Forward declaration to avoid circular dependency
#include "LinearEncoderController.h"

namespace PCNTEncoderController
{
    // Initialize ESP32Encoder for encoder channels
    void setup();
    
    // Read current position from encoder
    float getCurrentPosition(int encoderIndex);
    
    // Set current position offset for encoder
    void setCurrentPosition(int encoderIndex, float offsetPos);
    
    // Get current encoder count
    int64_t getEncoderCount(int encoderIndex);
    
    // Reset encoder counter
    void resetEncoderCount(int encoderIndex);
    
    // Test encoder accuracy and consistency
    void testEncoderAccuracy(int encoderIndex); // Validates count consistency with rapid consecutive readings
    
    // Track encoder accuracy during motor moves
    void startEncoderTracking(int encoderIndex, int32_t commandedSteps);
    void stopEncoderTracking(int encoderIndex);
    
    // Check if ESP32Encoder is available and configured
    bool isESP32EncoderAvailable();
};
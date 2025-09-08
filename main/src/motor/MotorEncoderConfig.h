#pragma once

// Global motor-encoder conversion configuration
// This defines the relationship between stepper motor steps and encoder units

namespace MotorEncoderConfig {
    // Default conversion factor based on user specification:
    // 1mm = 3200 steps (stepper motor: 200 steps/rotation * 16 microsteps)
    // 1mm = 500 encoder counts (encoder with 2µm step width)
    // Therefore: 1 step = 0.3125µm in encoder units
    extern float stepsToEncoderUnits;
    
    // Function to get the current conversion factor
    float getStepsToEncoderUnits();
    
    // Function to set the conversion factor (µm per step)
    void setStepsToEncoderUnits(float micrometersPerStep);
    
    // Function to load conversion factor from preferences
    void loadFromPreferences();
    
    // Function to save conversion factor to preferences
    void saveToPreferences();
    
    // Functions for encoder position persistence
    void saveEncoderPosition(int encoderIndex, float position);
    float loadEncoderPosition(int encoderIndex);
    void saveAllEncoderPositions();
    void loadAllEncoderPositions();
    
    // Initialize with default values and load from preferences
    void setup();
}
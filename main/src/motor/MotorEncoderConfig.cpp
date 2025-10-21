#include "MotorEncoderConfig.h"
#include <Preferences.h>
#include "HardwareSerial.h"

namespace MotorEncoderConfig {
    // Default conversion factor: 1 step = 0.3125µm
    // Based on: 1mm = 3200 steps = 500 encoder counts (2µm step width)
    float stepsToEncoderUnits = 0.3125f;
    
    float getStepsToEncoderUnits() {
        return stepsToEncoderUnits;
    }
    
    void setStepsToEncoderUnits(float micrometersPerStep) {
        stepsToEncoderUnits = micrometersPerStep;
        log_i("Motor-encoder conversion factor set to: %f µm per step", stepsToEncoderUnits);
    }
    
    void loadFromPreferences() {
        Preferences preferences;
        preferences.begin("enc", false);
        stepsToEncoderUnits = preferences.getFloat("encFac", 0.3125f);
        preferences.end();
        log_i("Loaded motor-encoder conversion factor: %f µm per step", stepsToEncoderUnits);
    }
    
    void saveToPreferences() {
        Preferences preferences;
        preferences.begin("enc", false);
        preferences.putFloat("encFac", stepsToEncoderUnits);
        preferences.end();
        log_i("Saved motor-encoder conversion factor: %f µm per step", stepsToEncoderUnits);
    }
    
    void saveEncoderPosition(int encoderIndex, float position) {
        if (encoderIndex < 0 || encoderIndex >= 4) return;
        
        Preferences preferences;
        preferences.begin("encPos", false);
        String key = "pos" + String(encoderIndex);
        preferences.putFloat(key.c_str(), position);
        preferences.end();
        log_i("Saved encoder %d position: %f µm", encoderIndex, position);
    }
    
    float loadEncoderPosition(int encoderIndex) {
        if (encoderIndex < 0 || encoderIndex >= 4) return 0.0f;
        
        Preferences preferences;
        preferences.begin("encPos", false);
        String key = "pos" + String(encoderIndex);
        float position = preferences.getFloat(key.c_str(), 0.0f);
        preferences.end();
        log_i("Loaded encoder %d position: %f µm", encoderIndex, position);
        return position;
    }
    
    void saveAllEncoderPositions() {
        // This will be called by LinearEncoderController to save all positions
        log_i("Saving all encoder positions to preferences");
        // The actual saving logic will be implemented in LinearEncoderController
    }
    
    void loadAllEncoderPositions() {
        // This will be called by LinearEncoderController to restore all positions
        log_i("Loading all encoder positions from preferences");
        // The actual loading logic will be implemented in LinearEncoderController
    }
    
    void setup() {
        log_i("Setting up motor-encoder configuration");
        loadFromPreferences();
        loadAllEncoderPositions();
    }
}
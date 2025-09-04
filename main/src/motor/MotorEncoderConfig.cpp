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
        preferences.begin("UC2_MOTOR_ENC", false);
        stepsToEncoderUnits = preferences.getFloat("stepsToEncUnits", 0.3125f);
        preferences.end();
        log_i("Loaded motor-encoder conversion factor: %f µm per step", stepsToEncoderUnits);
    }
    
    void saveToPreferences() {
        Preferences preferences;
        preferences.begin("UC2_MOTOR_ENC", false);
        preferences.putFloat("stepsToEncUnits", stepsToEncoderUnits);
        preferences.end();
        log_i("Saved motor-encoder conversion factor: %f µm per step", stepsToEncoderUnits);
    }
    
    void setup() {
        log_i("Setting up motor-encoder configuration");
        loadFromPreferences();
    }
}
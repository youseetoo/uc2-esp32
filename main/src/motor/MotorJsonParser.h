#pragma once

#include "cJSON.h"

// Forward declarations for JSON keys
extern const char *key_precise;
extern const char *key_encoder_precision;

namespace MotorJsonParser
{
    int act(cJSON *doc);
    cJSON *get(cJSON *docin);
    
    // Helper function to check if encoder-based precision motion is requested
    inline bool isEncoderPrecisionRequested(cJSON *stepper) {
        return (cJSON_GetObjectItemCaseSensitive(stepper, key_precise) != NULL && 
                cJSON_GetObjectItemCaseSensitive(stepper, key_precise)->valueint == 1) ||
               (cJSON_GetObjectItemCaseSensitive(stepper, key_encoder_precision) != NULL && 
                cJSON_GetObjectItemCaseSensitive(stepper, key_encoder_precision)->valueint == 1);
    }
};
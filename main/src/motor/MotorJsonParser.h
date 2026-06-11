#pragma once

#include "cJSON.h"
#include "../../JsonKeys.h"

namespace MotorJsonParser
{
    int act(cJSON *doc);
    cJSON *get(cJSON *docin);

    // Individual sub-parsers — called by DeviceRouter::handleMotorAct so that
    // routed /motor_act commands still execute config side-effects (enable,
    // setpos, hardlimits, joystickdir, ...) and stage scans. Previously these
    // only ran when DeviceRouter returned nullptr (i.e. never for routed paths).
    void parseEnableMotor(cJSON *doc);
    void parseAutoEnableMotor(cJSON *doc);
    bool parseSetPosition(cJSON *doc);
    void parseMotorPinDirection(cJSON *doc);
    void parseSetHardLimits(cJSON *doc);
    void parseSetJoystickDirection(cJSON *doc);
    void parseMotorDriveJson(cJSON *doc);
#ifdef STAGE_SCAN
    void parseStageScan(cJSON *doc);
#endif
    
    // Helper function to check if encoder-based precision motion is requested
    inline bool isEncoderPrecisionRequested(cJSON *stepper) {
        return (cJSON_GetObjectItemCaseSensitive(stepper, key_precise) != NULL && 
                cJSON_GetObjectItemCaseSensitive(stepper, key_precise)->valueint == 1) ||
               (cJSON_GetObjectItemCaseSensitive(stepper, key_encoder_precision) != NULL && 
                cJSON_GetObjectItemCaseSensitive(stepper, key_encoder_precision)->valueint == 1);
    }
    
    // Helper function to add JSON parameter if present
    inline void addJsonFloatIfPresent(cJSON *source, cJSON *target, const char *key) {
        cJSON *param = cJSON_GetObjectItemCaseSensitive(source, key);
        if (param != NULL) {
            cJSON_AddNumberToObject(target, key, param->valuedouble);
        }
    }
    
    inline void addJsonIntIfPresent(cJSON *source, cJSON *target, const char *key) {
        cJSON *param = cJSON_GetObjectItemCaseSensitive(source, key);
        if (param != NULL) {
            cJSON_AddNumberToObject(target, key, param->valueint);
        }
    }
};
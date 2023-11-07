#pragma once
#include "cJson.h"
#include "Arduino.h"

namespace cJsonTool
{
    static int getJsonInt(cJSON *job, const char *key)
    {
        if (job == NULL)
        {
            log_e("json %s is null", key);
            return 0;
        }
        cJSON *val = cJSON_GetObjectItemCaseSensitive(job, key);
        if (val == NULL)
        {
            log_e("json value %s is null", key);
            return 0;
        }
        if (cJSON_IsNumber(val))
            return cJSON_GetNumberValue(val);
        return 0;
    };
    static void setJsonInt(cJSON *job, const char *key, int valToSet)
    {
        cJSON *v = cJSON_CreateNumber(valToSet);
        cJSON_AddItemToObject(job, key, v);
    };
    static void setJsonFloat(cJSON *job, const char *key, float valToSet)
    {
        cJSON *v = cJSON_CreateNumber(valToSet);
        cJSON_AddItemToObject(job, key, v);
    };
    static float getJsonFloat(cJSON *job, const char *key)
    {
        cJSON *val = cJSON_GetObjectItemCaseSensitive(job, key);
        if (cJSON_IsNumber(val) && val->valuedouble != NULL)
            return val->valuedouble;
        return 0;
    };
}
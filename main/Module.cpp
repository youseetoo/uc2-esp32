#include "Module.h"

Module::Module(){}
Module::~Module(){}


int Module::getJsonInt(cJSON* job,const char* key){
    if(job == NULL)
    {
        log_e("json %s is null", key);
        return 0;
    }
    cJSON *val = cJSON_GetObjectItemCaseSensitive(job, key);
    if(val == NULL)
    {
        log_e("json value %s is null", key);
        return 0;
    }
    if (cJSON_IsNumber(val))
        return cJSON_GetNumberValue(val);
    return 0;
}

void Module::setJsonInt(cJSON* job,const char* key, int valToSet)
{
    cJSON * v = cJSON_CreateNumber(valToSet);
    cJSON_AddItemToObject(job, key, v);
}

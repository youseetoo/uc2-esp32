#include "Module.h"

Module::Module(){}
Module::~Module(){}


int Module::getJsonInt(cJSON* job,const char* key){
    cJSON *val = cJSON_GetObjectItemCaseSensitive(job, key);
    if (cJSON_IsNumber(val) && val->valueint != NULL)
        return val->valueint;
    return 0;
}

void Module::setJsonInt(cJSON* job,const char* key, int valToSet)
{
    cJSON * v = cJSON_CreateNumber(valToSet);
    cJSON_AddItemToObject(job, key, v);
}

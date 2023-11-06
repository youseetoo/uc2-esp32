#pragma once
#include "cJSON.h"
#include "PinConfig.h"
#include "JsonKeys.h"

class Module
{
    protected:
    int getJsonInt(cJSON* job,const char* key, int defaultVal = 0);
    void setJsonInt(cJSON* job,const char* key, int valToSet);
    void setJsonFloat(cJSON* job,const char* key, float valToSet);
    float getJsonFloat(cJSON* job,const char* key);
    
    public:
    Module();
	virtual ~Module();
    virtual void setup() = 0;
    virtual void loop() = 0;
    virtual int act(cJSON* doc) = 0;
    virtual cJSON* get(cJSON* doc) = 0;
};
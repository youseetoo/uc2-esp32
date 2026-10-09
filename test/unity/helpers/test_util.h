#pragma once
#include <unity.h>
#include <cJSON.h>

// Common test utilities
namespace TestUtil {
    // Create a cJSON object from a JSON string
    inline cJSON* makeJson(const char* jsonStr) {
        return cJSON_Parse(jsonStr);
    }

    // Safely delete a cJSON object
    inline void deleteJson(cJSON* obj) {
        if (obj) cJSON_Delete(obj);
    }

    // Compare two JSON strings (ignoring whitespace)
    inline bool jsonEqual(const char* expected, const char* actual) {
        cJSON* e = cJSON_Parse(expected);
        cJSON* a = cJSON_Parse(actual);
        bool result = cJSON_Compare(e, a, true);
        cJSON_Delete(e);
        cJSON_Delete(a);
        return result;
    }
}

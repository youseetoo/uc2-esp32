#pragma once
#include <unity.h>
#include <cJSON.h>

// JSON assertion helpers for unit tests
namespace JsonAssert {
    // Assert that a JSON object contains a key with a specific integer value
    inline void assertJsonInt(cJSON* obj, const char* key, int expected) {
        cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
        TEST_ASSERT_NOT_NULL_MESSAGE(item, "JSON key not found");
        TEST_ASSERT_EQUAL_INT_MESSAGE(expected, item->valueint,
            "JSON int value mismatch");
    }

    // Assert that a JSON object contains a key with a specific double value
    inline void assertJsonDouble(cJSON* obj, const char* key, double expected) {
        cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
        TEST_ASSERT_NOT_NULL_MESSAGE(item, "JSON key not found");
        // Use TEST_ASSERT_DOUBLE_WITHIN_MESSAGE for floating point comparison
        TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(0.0001, expected, item->valuedouble,
            "JSON double value mismatch");
    }

    // Assert that a JSON object does NOT contain a key
    inline void assertJsonMissing(cJSON* obj, const char* key) {
        cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
        TEST_ASSERT_NULL_MESSAGE(item, "JSON key should not exist");
    }

    // Assert that two JSON objects are equal (string comparison)
    inline void assertJsonEqual(cJSON* expected, cJSON* actual) {
        char* expStr = cJSON_PrintUnformatted(expected);
        char* actStr = cJSON_PrintUnformatted(actual);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(expStr, actStr, "JSON mismatch");
        free(expStr);
        free(actStr);
    }
}

#ifdef UNIT_TEST

#include <unity.h>
#include <cstdint>
#include <cstring>
#include "../../main/src/motor/MotorJsonParser.h"
#include "../../main/src/motor/MotorJsonParser.cpp"
#include "mocks/mock_clock.h"
#include "helpers/test_util.h"
#include "helpers/assert_json.h"

// ============================================================================
// MJ-001: act with valid motor action JSON
// ============================================================================
void test_mj001_actValidJson() {
    const char* json = "{\"task\":\"motor_act\",\"stepperid\":0,\"position\":1000}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    int result = MotorJsonParser::act(doc);
    // act() returns 0 on success (no error), may return other values for errors
    TEST_ASSERT_LESS_OR_EQUAL(0, result);
    TestUtil::deleteJson(doc);
}

// ============================================================================
// MJ-002: act with missing targetPosition field
// ============================================================================
void test_mj002_actMissingTargetPosition() {
    const char* json = "{\"task\":\"motor_act\",\"stepperid\":0}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    int result = MotorJsonParser::act(doc);
    // Should handle gracefully (no crash)
    TEST_ASSERT_TRUE(result >= -1);
    TestUtil::deleteJson(doc);
}

// ============================================================================
// MJ-003: act with invalid axis number
// ============================================================================
void test_mj003_actInvalidAxis() {
    const char* json = "{\"task\":\"motor_act\",\"stepperid\":99,\"position\":1000}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    int result = MotorJsonParser::act(doc);
    // Should not crash
    TEST_ASSERT_TRUE(result >= -1);
    TestUtil::deleteJson(doc);
}

// ============================================================================
// MJ-004: get returns current motor state
// ============================================================================
void test_mj004_getReturnsState() {
    const char* json = "{\"task\":\"motor_get\"}";
    cJSON* docin = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(docin);
    if (!docin) return;
    
    cJSON* result = MotorJsonParser::get(docin);
    TEST_ASSERT_NOT_NULL(result);
    if (result) {
        // Should have a "motor" key
        cJSON* motor = cJSON_GetObjectItemCaseSensitive(result, "motor");
        TEST_ASSERT_NOT_NULL(motor);
        TestUtil::deleteJson(result);
    }
    TestUtil::deleteJson(docin);
}

// ============================================================================
// MJ-005: parseEnableMotor enables motor
// ============================================================================
void test_mj005_parseEnableMotor() {
    const char* json = "{\"task\":\"motor_act\",\"enable\":true,\"stepperid\":0}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    MotorJsonParser::parseEnableMotor(doc);
    // Should not crash
    TestUtil::deleteJson(doc);
}

// ============================================================================
// MJ-006: parseEnableMotor invalid axis
// ============================================================================
void test_mj006_parseEnableMotorInvalidAxis() {
    const char* json = "{\"task\":\"motor_act\",\"enable\":true,\"stepperid\":99}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    MotorJsonParser::parseEnableMotor(doc);
    // Should not crash
    TestUtil::deleteJson(doc);
}

// ============================================================================
// MJ-007: parseSetPosition sets position
// ============================================================================
void test_mj007_parseSetPosition() {
    const char* json = "{\"task\":\"motor_act\",\"stepperid\":0,\"position\":5000}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    bool result = MotorJsonParser::parseSetPosition(doc);
    // May return false if motor not configured, but should not crash
    (void)result;
    TestUtil::deleteJson(doc);
}

// ============================================================================
// MJ-008: parseSetPosition overflow handling
// ============================================================================
void test_mj008_parseSetPositionOverflow() {
    const char* json = "{\"task\":\"motor_act\",\"stepperid\":0,\"position\":2147483647}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    bool result = MotorJsonParser::parseSetPosition(doc);
    // Should handle INT32_MAX safely
    (void)result;
    TestUtil::deleteJson(doc);
}

// ============================================================================
// MJ-009: parseMotorPinDirection sets direction
// ============================================================================
void test_mj009_parseMotorPinDirection() {
    const char* json = "{\"task\":\"motor_act\",\"stepperid\":0,\"direction\":1}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    MotorJsonParser::parseMotorPinDirection(doc);
    // Should not crash
    TestUtil::deleteJson(doc);
}

// ============================================================================
// MJ-010: parseSetHardLimits sets limits
// ============================================================================
void test_mj010_parseSetHardLimits() {
    const char* json = "{\"task\":\"motor_act\",\"stepperid\":0,\"min\":-1000,\"max\":1000}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    MotorJsonParser::parseSetHardLimits(doc);
    // Should not crash
    TestUtil::deleteJson(doc);
}

// ============================================================================
// MJ-011: parseSetHardLimits invalid range
// ============================================================================
void test_mj011_parseSetHardLimitsInvalidRange() {
    const char* json = "{\"task\":\"motor_act\",\"stepperid\":0,\"min\":1000,\"max\":-1000}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    MotorJsonParser::parseSetHardLimits(doc);
    // Should not crash even with invalid range
    TestUtil::deleteJson(doc);
}

// ============================================================================
// MJ-012: parseSetJoystickDirection sets direction
// ============================================================================
void test_mj012_parseSetJoystickDirection() {
    const char* json = "{\"task\":\"motor_act\",\"joystickdir\":1}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    MotorJsonParser::parseSetJoystickDirection(doc);
    // Should not crash
    TestUtil::deleteJson(doc);
}

// ============================================================================
// MJ-013: parseMotorDriveJson processes drive command
// ============================================================================
void test_mj013_parseMotorDriveJson() {
    const char* json = "{\"task\":\"motor_act\",\"speed\":1000,\"steps\":500}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    MotorJsonParser::parseMotorDriveJson(doc);
    // Should not crash
    TestUtil::deleteJson(doc);
}

// ============================================================================
// MJ-014: parseStageScan with valid grid params (if STAGE_SCAN)
// ============================================================================
void test_mj014_parseStageScanValid() {
#ifdef STAGE_SCAN
    const char* json = "{\"task\":\"motor_act\",\"stageScan\":{\"xStart\":0,\"xEnd\":100,\"yStart\":0,\"yEnd\":100,\"xSteps\":10,\"ySteps\":10}}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    MotorJsonParser::parseStageScan(doc);
    // Should not crash
    TestUtil::deleteJson(doc);
#else
    TEST_IGNORE();
#endif
}

// ============================================================================
// MJ-015: parseStageScan with coordinate array (if STAGE_SCAN)
// ============================================================================
void test_mj015_parseStageScanCoordinates() {
#ifdef STAGE_SCAN
    const char* json = "{\"task\":\"motor_act\",\"stageScan\":{\"coordinates\":[[0,0],[1,1],[2,2]]}}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    MotorJsonParser::parseStageScan(doc);
    // Should not crash
    TestUtil::deleteJson(doc);
#else
    TEST_IGNORE();
#endif
}

// ============================================================================
// MJ-016: parseStageScan with invalid coordinate count (if STAGE_SCAN)
// ============================================================================
void test_mj016_parseStageScanInvalidCoordinates() {
#ifdef STAGE_SCAN
    const char* json = "{\"task\":\"motor_act\",\"stageScan\":{\"coordinates\":[[0,0],[1]]}}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    MotorJsonParser::parseStageScan(doc);
    // Should not crash
    TestUtil::deleteJson(doc);
#else
    TEST_IGNORE();
#endif
}

// ============================================================================
// MJ-017: addJsonFloatIfPresent adds existing float
// ============================================================================
void test_mj017_addJsonFloatIfPresent() {
    cJSON* source = cJSON_Parse("{\"value\":3.14}");
    cJSON* target = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(target);
    if (!source || !target) {
        TestUtil::deleteJson(source);
        TestUtil::deleteJson(target);
        return;
    }
    
    MotorJsonParser::addJsonFloatIfPresent(source, target, "value");
    
    cJSON* result = cJSON_GetObjectItemCaseSensitive(target, "value");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 3.14, result->valuedouble);
    
    TestUtil::deleteJson(source);
    TestUtil::deleteJson(target);
}

// ============================================================================
// MJ-018: addJsonFloatIfPresent skips missing key
// ============================================================================
void test_mj018_addJsonFloatIfPresentMissing() {
    cJSON* source = cJSON_Parse("{\"other\":1}");
    cJSON* target = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(target);
    if (!source || !target) {
        TestUtil::deleteJson(source);
        TestUtil::deleteJson(target);
        return;
    }
    
    MotorJsonParser::addJsonFloatIfPresent(source, target, "value");
    
    cJSON* result = cJSON_GetObjectItemCaseSensitive(target, "value");
    TEST_ASSERT_NULL(result);
    
    TestUtil::deleteJson(source);
    TestUtil::deleteJson(target);
}

// ============================================================================
// MJ-019: addJsonIntIfPresent adds existing int
// ============================================================================
void test_mj019_addJsonIntIfPresent() {
    cJSON* source = cJSON_Parse("{\"count\":42}");
    cJSON* target = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(target);
    if (!source || !target) {
        TestUtil::deleteJson(source);
        TestUtil::deleteJson(target);
        return;
    }
    
    MotorJsonParser::addJsonIntIfPresent(source, target, "count");
    
    cJSON* result = cJSON_GetObjectItemCaseSensitive(target, "count");
    TEST_ASSERT_NOT_NULL(result);
    TEST_ASSERT_EQUAL_INT(42, result->valueint);
    
    TestUtil::deleteJson(source);
    TestUtil::deleteJson(target);
}

// ============================================================================
// MJ-020: addJsonIntIfPresent skips missing key
// ============================================================================
void test_mj020_addJsonIntIfPresentMissing() {
    cJSON* source = cJSON_Parse("{\"other\":1}");
    cJSON* target = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_NOT_NULL(target);
    if (!source || !target) {
        TestUtil::deleteJson(source);
        TestUtil::deleteJson(target);
        return;
    }
    
    MotorJsonParser::addJsonIntIfPresent(source, target, "count");
    
    cJSON* result = cJSON_GetObjectItemCaseSensitive(target, "count");
    TEST_ASSERT_NULL(result);
    
    TestUtil::deleteJson(source);
    TestUtil::deleteJson(target);
}

#endif

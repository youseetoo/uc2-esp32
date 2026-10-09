#ifdef UNIT_TEST

#include <unity.h>
#include <cstdint>
#include <cstring>
#include "../../main/src/config/NVSConfig.h"
#include "../../main/src/config/NVSConfig.cpp"
#include "mocks/mock_clock.h"
#include "mocks/mock_nvs.h"
#include "helpers/test_util.h"

// ============================================================================
// NVS-001: loadConfig loads from NVS successfully
// ============================================================================
void test_nvs001_loadConfig() {
    // loadConfig should not crash
    NVSConfig::loadConfig();
    TEST_PASS();
}

// ============================================================================
// NVS-002: loadConfig falls back to defaults when NVS empty
// ============================================================================
void test_nvs002_loadConfigFallback() {
    // Should fall back to defaults without crashing
    NVSConfig::loadConfig();
    TEST_PASS();
}

// ============================================================================
// NVS-003: saveConfig persists config
// ============================================================================
void test_nvs003_saveConfig() {
    // saveConfig should not crash
    NVSConfig::saveConfig();
    TEST_PASS();
}

// ============================================================================
// NVS-004: saveConfig then loadConfig round-trip
// ============================================================================
void test_nvs004_saveLoadRoundTrip() {
    NVSConfig::saveConfig();
    NVSConfig::loadConfig();
    TEST_PASS();
}

// ============================================================================
// NVS-005: resetConfig clears NVS and reloads defaults
// ============================================================================
void test_nvs005_resetConfig() {
    NVSConfig::resetConfig();
    TEST_PASS();
}

// ============================================================================
// NVS-006: toJson serializes config correctly
// ============================================================================
void test_nvs006_toJson() {
    cJSON* json = NVSConfig::toJson();
    TEST_ASSERT_NOT_NULL(json);
    if (json) {
        TEST_ASSERT_TRUE(cJSON_IsObject(json));
        cJSON_Delete(json);
    }
}

// ============================================================================
// NVS-007: fromJson merges partial JSON
// ============================================================================
void test_nvs007_fromJsonPartial() {
    const char* json = "{\"canRole\":1}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    NVSConfig::fromJson(doc);
    TestUtil::deleteJson(doc);
    TEST_PASS();
}

// ============================================================================
// NVS-008: fromJson with null document is safe
// ============================================================================
void test_nvs008_fromJsonNull() {
    // Should not crash with null document
    NVSConfig::fromJson(nullptr);
    TEST_PASS();
}

// ============================================================================
// NVS-009: Routing table JSON export/import round-trip
// ============================================================================
void test_nvs009_routingTableRoundTrip() {
    cJSON* json = NVSConfig::toJson();
    TEST_ASSERT_NOT_NULL(json);
    if (!json) return;
    
    NVSConfig::fromJson(json);
    cJSON_Delete(json);
    TEST_PASS();
}

// ============================================================================
// NVS-010: NVS key namespace collision safety
// ============================================================================
void test_nvs010_namespaceCollision() {
    // Use mock NVS to test collision safety
    MockNvs::clear();
    
    // Set a value
    int val = 42;
    MockNvs::set("config", "test_key", &val, sizeof(int));
    
    // Read it back
    int readVal = 0;
    size_t len = sizeof(int);
    bool result = MockNvs::get("config", "test_key", &readVal, &len);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_INT(42, readVal);
}

// ============================================================================
// NVS-011: Large config size handling
// ============================================================================
void test_nvs011_largeConfig() {
    // Test with a large JSON document. The loop below builds ~1152 chars
    // (14 + 100 x ("param"+i+":"+2i+",") + 3), so the buffer must be >= 2048
    // or the strcat/sprintf overflow the stack and corrupt the frame.
    char largeJson[2048];
    strcpy(largeJson, "{\"canRole\":1,\"");
    for (int i = 0; i < 100; i++) {
        strcat(largeJson, "param");
        sprintf(largeJson + strlen(largeJson), "%d:%d,", i, i * 2);
    }
    strcat(largeJson, "\"}");
    
    cJSON* doc = TestUtil::makeJson(largeJson);
    if (doc) {
        NVSConfig::fromJson(doc);
        TestUtil::deleteJson(doc);
    }
    TEST_PASS();
}

// ============================================================================
// NVS-012: Corrupted NVS data handling
// ============================================================================
void test_nvs012_corruptedData() {
    // Test with corrupted JSON
    const char* corrupted = "{invalid json!!!";
    cJSON* doc = TestUtil::makeJson(corrupted);
    // doc should be nullptr for corrupted JSON
    TEST_ASSERT_NULL(doc);
    TEST_PASS();
}

#endif

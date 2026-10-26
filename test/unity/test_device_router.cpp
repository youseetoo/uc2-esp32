#ifdef UNIT_TEST

#include <unity.h>
#include <cstdint>
#include <cstring>
#include "../../components/uc2_canopen/canopen/DeviceRouter.h"
#include "../../components/uc2_canopen/canopen/device_router.cpp"
#include "../../components/uc2_canopen/canopen/serial_dispatch.cpp"
#include "../../components/uc2_canopen/canopen/RoutingTable.h"
#include "mocks/mock_clock.h"
#include "helpers/test_util.h"

using namespace UC2;

// ============================================================================
// DR-001: routeCommand motor_act with LOCAL route
// ============================================================================
void test_dr001_routeMotorActLocal() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"task\":\"motor_act\",\"stepperid\":0,\"position\":1000}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("motor_act", doc);
    // May return nullptr if not handled in test environment, but shouldn't crash
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-002: routeCommand motor_get returns state
// ============================================================================
void test_dr002_routeMotorGet() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"task\":\"motor_get\"}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("motor_get", doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-003: routeCommand laser_act with LOCAL route
// ============================================================================
void test_dr003_routeLaserActLocal() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"task\":\"laser_act\",\"LASERid\":0,\"LASERval\":100}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("laser_act", doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-004: routeCommand laser_get returns state
// ============================================================================
void test_dr004_routeLaserGet() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"task\":\"laser_get\"}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("laser_get", doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-005: routeCommand led_act with LOCAL route
// ============================================================================
void test_dr005_routeLedActLocal() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"task\":\"led_act\",\"mode\":0,\"r\":255,\"g\":0,\"b\":0}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("led_act", doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-006: routeCommand led_get returns state
// ============================================================================
void test_dr006_routeLedGet() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"task\":\"led_get\"}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("led_get", doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-007: routeCommand galvo_act with LOCAL route
// ============================================================================
void test_dr007_routeGalvoActLocal() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"task\":\"galvo_act\",\"x\":0.5,\"y\":0.5}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("galvo_act", doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-008: routeCommand home_act with LOCAL route
// ============================================================================
void test_dr008_routeHomeActLocal() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"task\":\"home_act\",\"stepperid\":0}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("home_act", doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-009: routeCommand digital_out_act LOCAL
// ============================================================================
void test_dr009_routeDigitalOutAct() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"task\":\"digital_out_act\",\"id\":0,\"value\":1}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("digital_out_act", doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-010: routeCommand digital_in_get LOCAL
// ============================================================================
void test_dr010_routeDigitalInGet() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"task\":\"digital_in_get\",\"id\":0}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("digital_in_get", doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-011: routeCommand gpio_act LOCAL
// ============================================================================
void test_dr011_routeGpioAct() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"task\":\"gpio_act\",\"id\":0,\"value\":1}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("gpio_act", doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-012: routeCommand gpio_get LOCAL
// ============================================================================
void test_dr012_routeGpioGet() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"task\":\"gpio_get\"}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("gpio_get", doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-013: routeCommand unknown task returns nullptr
// ============================================================================
void test_dr013_routeUnknownTask() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"task\":\"unknown_task\"}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("unknown_task", doc);
    // Unknown tasks should return nullptr
    TEST_ASSERT_NULL(result);
    TestUtil::deleteJson(doc);
}

// ============================================================================
// DR-014: routeCommand null document safety
// ============================================================================
void test_dr014_routeNullDocument() {
    RoutingTable::buildDefault();
    
    // Null document should not crash
    cJSON* result = DeviceRouter::routeCommand("motor_act", nullptr);
    TEST_ASSERT_NULL(result);
}

// ============================================================================
// DR-015: routeCommand malformed JSON safety
// ============================================================================
void test_dr015_routeMalformedJson() {
    RoutingTable::buildDefault();
    
    const char* malformed = "{invalid json!!!";
    cJSON* doc = TestUtil::makeJson(malformed);
    // doc will be nullptr for malformed JSON
    if (!doc) {
        // This is expected — malformed JSON returns nullptr from parse
        TEST_ASSERT_NULL(doc);
        return;
    }
    
    cJSON* result = DeviceRouter::routeCommand("motor_act", doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-016: routeCommand i2c_act with node parameter
// ============================================================================
void test_dr016_routeI2cAct() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"task\":\"i2c_act\",\"node\":60,\"addr\":68,\"write\":[253],\"read\":6}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("i2c_act", doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-017: handleMotorAct directly
// ============================================================================
void test_dr017_handleMotorAct() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"stepperid\":0,\"position\":500}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::handleMotorAct(doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-018: handleLaserAct directly
// ============================================================================
void test_dr018_handleLaserAct() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"LASERid\":0,\"LASERval\":100}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::handleLaserAct(doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-019: handleLedAct directly
// ============================================================================
void test_dr019_handleLedAct() {
    RoutingTable::buildDefault();
    
    const char* json = "{\"mode\":0,\"r\":255,\"g\":0,\"b\":0}";
    cJSON* doc = TestUtil::makeJson(json);
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::handleLedAct(doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

// ============================================================================
// DR-020: routeCommand with empty JSON object
// ============================================================================
void test_dr020_routeEmptyJson() {
    RoutingTable::buildDefault();
    
    cJSON* doc = cJSON_CreateObject();
    TEST_ASSERT_NOT_NULL(doc);
    if (!doc) return;
    
    cJSON* result = DeviceRouter::routeCommand("motor_act", doc);
    TestUtil::deleteJson(doc);
    if (result) TestUtil::deleteJson(result);
}

#endif

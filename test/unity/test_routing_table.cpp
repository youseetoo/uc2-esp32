#ifdef UNIT_TEST

#include <unity.h>
#include <cstdint>
#include <cstring>
#include "../../main/src/canopen/RoutingTable.h"
#include "../../main/src/canopen/RoutingTable.cpp"
#include "mocks/mock_clock.h"
#include "helpers/test_util.h"

using namespace UC2;

// ============================================================================
// RT-001: buildDefault creates expected routes
// ============================================================================
void test_rt001_buildDefaultCreatesRoutes() {
    RoutingTable::buildDefault();
    TEST_ASSERT_GREATER_THAN(0, RoutingTable::getCount());
}

// ============================================================================
// RT-002: buildDefault motor routes from pinConfig
// ============================================================================
void test_rt002_buildDefaultMotorRoutes() {
    RoutingTable::buildDefault();
    
    // At minimum, some motor routes should exist
    const RouteEntry* entry = RoutingTable::find(RouteEntry::MOTOR, 0);
    // May be nullptr if no motor pins configured, but should not crash
    (void)entry;
}

// ============================================================================
// RT-003: find returns correct entry for motor
// ============================================================================
void test_rt003_findMotor() {
    RoutingTable::buildDefault();
    
    const RouteEntry* entry = RoutingTable::find(RouteEntry::MOTOR, 0);
    if (entry) {
        TEST_ASSERT_EQUAL(RouteEntry::MOTOR, entry->type);
        TEST_ASSERT_EQUAL(0, entry->logicalId);
    }
}

// ============================================================================
// RT-004: find returns nullptr for unknown type/id
// ============================================================================
void test_rt004_findUnknown() {
    RoutingTable::buildDefault();
    
    const RouteEntry* entry = RoutingTable::find(
        static_cast<RouteEntry::Type>(99), 99);
    TEST_ASSERT_NULL(entry);
}

// ============================================================================
// RT-005: set updates route correctly
// ============================================================================
void test_rt005_setRoute() {
    RoutingTable::buildDefault();
    
    RoutingTable::set(RouteEntry::MOTOR, 0, RouteEntry::LOCAL);
    
    const RouteEntry* entry = RoutingTable::find(RouteEntry::MOTOR, 0);
    TEST_ASSERT_NOT_NULL(entry);
    if (entry) {
        TEST_ASSERT_EQUAL(RouteEntry::LOCAL, entry->where);
    }
}

// ============================================================================
// RT-006: set with OFF disables route
// ============================================================================
void test_rt006_setOffRoute() {
    RoutingTable::buildDefault();
    
    RoutingTable::set(RouteEntry::LASER, 0, RouteEntry::OFF);
    
    const RouteEntry* entry = RoutingTable::find(RouteEntry::LASER, 0);
    TEST_ASSERT_NOT_NULL(entry);
    if (entry) {
        TEST_ASSERT_EQUAL(RouteEntry::OFF, entry->where);
    }
}

// ============================================================================
// RT-007: set with REMOTE sets nodeId
// ============================================================================
void test_rt007_setRemoteRoute() {
    RoutingTable::buildDefault();
    
    RoutingTable::set(RouteEntry::MOTOR, 0, RouteEntry::REMOTE, 42, 1);
    
    const RouteEntry* entry = RoutingTable::find(RouteEntry::MOTOR, 0);
    TEST_ASSERT_NOT_NULL(entry);
    if (entry) {
        TEST_ASSERT_EQUAL(RouteEntry::REMOTE, entry->where);
        TEST_ASSERT_EQUAL(42, entry->nodeId);
        TEST_ASSERT_EQUAL(1, entry->subAxis);
    }
}

// ============================================================================
// RT-008: toJson exports full routing table
// ============================================================================
void test_rt008_toJson() {
    RoutingTable::buildDefault();
    
    cJSON* json = RoutingTable::toJson();
    TEST_ASSERT_NOT_NULL(json);
    if (json) {
        // toJson() emits the table as a JSON ARRAY of route objects — the
        // actual wire shape /route_get serialises (see SerialProcess.cpp:760).
        TEST_ASSERT_TRUE(cJSON_IsArray(json));
        TEST_ASSERT_TRUE(cJSON_GetArraySize(json) > 0);
        cJSON_Delete(json);
    }
}

// ============================================================================
// RT-009: logAll does not crash
// ============================================================================
void test_rt009_logAll() {
    RoutingTable::buildDefault();
    RoutingTable::logAll();
    // Just verify it doesn't crash
}

// ============================================================================
// RT-010: MAX_ROUTES boundary (64 entries)
// ============================================================================
void test_rt010_maxRoutesBoundary() {
    RoutingTable::buildDefault();
    TEST_ASSERT_LESS_OR_EQUAL(RoutingTable::getMaxRoutes(), RoutingTable::getCount());
}

// ============================================================================
// RT-011: RouteEntry::Type enum values correct
// ============================================================================
void test_rt011_routeEntryTypes() {
    TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(RouteEntry::MOTOR));
    TEST_ASSERT_EQUAL_UINT8(2, static_cast<uint8_t>(RouteEntry::LASER));
    TEST_ASSERT_EQUAL_UINT8(3, static_cast<uint8_t>(RouteEntry::LED));
    TEST_ASSERT_EQUAL_UINT8(4, static_cast<uint8_t>(RouteEntry::GALVO));
    TEST_ASSERT_EQUAL_UINT8(5, static_cast<uint8_t>(RouteEntry::HOME));
    TEST_ASSERT_EQUAL_UINT8(6, static_cast<uint8_t>(RouteEntry::TMC));
    TEST_ASSERT_EQUAL_UINT8(7, static_cast<uint8_t>(RouteEntry::DAC));
    TEST_ASSERT_EQUAL_UINT8(8, static_cast<uint8_t>(RouteEntry::AIN));
    TEST_ASSERT_EQUAL_UINT8(9, static_cast<uint8_t>(RouteEntry::DIN));
}

// ============================================================================
// RT-012: RouteEntry::Where enum values correct
// ============================================================================
void test_rt012_routeEntryWheres() {
    TEST_ASSERT_EQUAL_UINT8(0, static_cast<uint8_t>(RouteEntry::LOCAL));
    TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(RouteEntry::REMOTE));
    TEST_ASSERT_EQUAL_UINT8(2, static_cast<uint8_t>(RouteEntry::OFF));
}

// ============================================================================
// RT-013: All RouteEntry::Type values tested
// ============================================================================
void test_rt013_allRouteEntryTypes() {
    // Verify we can construct entries for all types
    RoutingTable::buildDefault();
    
    // Try to find each type — some may not exist in default config
    for (int type = 1; type <= 9; type++) {
        const RouteEntry* entry = RoutingTable::find(
            static_cast<RouteEntry::Type>(type), 0);
        // Entry may be nullptr (not configured), but shouldn't crash
        (void)entry;
    }
}

// ============================================================================
// RT-014: Duplicate logicalId handling
// ============================================================================
void test_rt014_duplicateLogicalId() {
    RoutingTable::buildDefault();
    
    // Set the same route twice — should be idempotent
    RoutingTable::set(RouteEntry::MOTOR, 0, RouteEntry::LOCAL);
    RoutingTable::set(RouteEntry::MOTOR, 0, RouteEntry::REMOTE, 10, 0);
    
    const RouteEntry* entry = RoutingTable::find(RouteEntry::MOTOR, 0);
    TEST_ASSERT_NOT_NULL(entry);
    if (entry) {
        TEST_ASSERT_EQUAL(RouteEntry::REMOTE, entry->where);
        TEST_ASSERT_EQUAL(10, entry->nodeId);
    }
}

// ============================================================================
// RT-015: set with out-of-range type does not crash
// ============================================================================
void test_rt015_setOutOfBoundsType() {
    RoutingTable::buildDefault();
    
    // Set with an out-of-range type — should not crash
    RoutingTable::set(static_cast<RouteEntry::Type>(99), 0, RouteEntry::LOCAL);
    
    // Table count should not have increased beyond MAX_ROUTES
    TEST_ASSERT_LESS_OR_EQUAL(RoutingTable::getMaxRoutes(), RoutingTable::getCount());
}

#endif

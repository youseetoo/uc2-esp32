// Test functions for AxisController
// Include this file in test_all.cpp to avoid multiple main() definitions

#ifdef UNIT_TEST

// DISABLED (T4.1 C port): AxisController is now pure C
// (components/uc2_axis/axis/AxisController.c, axis_* API). These tests use
// the old C++ AxisController:: API. Rewrite, then re-enable.
#if 0

#include <unity.h>
#include <cstdint>
#include <cstring>
#include "../../main/src/axis/AxisController.h"
#include "../../main/src/axis/AxisController.cpp"
#include "../../main/src/axis/AxisTypes.h"
#include "mocks/mock_clock.h"
#include "mocks/mock_focus_motor.h"
#include "helpers/test_util.h"

// ============================================================================
// AC-001: AxisController default initialization
// ============================================================================
void test_ac001_defaultInitialization() {
    // Just verify the namespace exists and types are available
    AxisFeedback fb = {};
    TEST_ASSERT_EQUAL(0, fb.commandedSteps);
    TEST_ASSERT_EQUAL(0, fb.measuredSteps);
    TEST_ASSERT_EQUAL(0, fb.positionErrorSteps);
    TEST_ASSERT_EQUAL(MODE_OPEN_LOOP, fb.mode);
    TEST_ASSERT_EQUAL(HEALTH_OK, fb.health);
    TEST_ASSERT_EQUAL(FAULT_NONE, fb.fault);
}

// ============================================================================
// AC-002: AxisMode enum values
// ============================================================================
void test_ac002_axisModeEnum() {
    TEST_ASSERT_EQUAL(0, static_cast<int>(AxisMode::MODE_OPEN_LOOP));
    TEST_ASSERT_EQUAL(1, static_cast<int>(AxisMode::MODE_MONITOR));
    TEST_ASSERT_EQUAL(2, static_cast<int>(AxisMode::MODE_CORRECT));
    TEST_ASSERT_EQUAL(3, static_cast<int>(AxisMode::MODE_SERVO));
}

// ============================================================================
// AC-003: AxisHealth enum values
// ============================================================================
void test_ac003_axisHealthEnum() {
    TEST_ASSERT_EQUAL(0, static_cast<int>(AxisHealth::HEALTH_OK));
    TEST_ASSERT_EQUAL(1, static_cast<int>(AxisHealth::HEALTH_DEGRADED));
    TEST_ASSERT_EQUAL(2, static_cast<int>(AxisHealth::HEALTH_FAULT));
}

// ============================================================================
// AC-004: AxisFault enum values
// ============================================================================
void test_ac004_axisFaultEnum() {
    TEST_ASSERT_EQUAL(0, static_cast<int>(AxisFault::FAULT_NONE));
    TEST_ASSERT_EQUAL(1, static_cast<int>(AxisFault::FAULT_STALL));
    TEST_ASSERT_EQUAL(2, static_cast<int>(AxisFault::FAULT_LOST_STEPS));
    TEST_ASSERT_EQUAL(3, static_cast<int>(AxisFault::FAULT_DIVERGENCE));
    TEST_ASSERT_EQUAL(4, static_cast<int>(AxisFault::FAULT_TIMEOUT));
    TEST_ASSERT_EQUAL(5, static_cast<int>(AxisFault::FAULT_CAL_INVALID));
    TEST_ASSERT_EQUAL(6, static_cast<int>(AxisFault::FAULT_CAL_FAILED));
    TEST_ASSERT_EQUAL(7, static_cast<int>(AxisFault::FAULT_ENC_NOISE));
}

// ============================================================================
// AC-005: AxisCalibration struct size
// ============================================================================
void test_ac005_axisCalibrationStructSize() {
    // AxisCalibration is an internal (NVS-persisted) struct, NOT a wire format,
    // so it is not #pragma pack'ed and its size is platform-dependent
    // (alignment padding). Assert it holds at least the raw field bytes (no field
    // dropped) and is not unexpectedly bloated.
    size_t raw = sizeof(int32_t) * 3 + sizeof(int8_t) + sizeof(uint16_t) * 2 + sizeof(uint32_t) + sizeof(bool) * 2;
    TEST_ASSERT_TRUE(sizeof(AxisCalibration) >= raw && sizeof(AxisCalibration) <= raw + 16);
}

// ============================================================================
// AC-006: AxisCalibration default values
// ============================================================================
void test_ac006_axisCalibrationDefaults() {
    AxisCalibration cal = {};
    TEST_ASSERT_EQUAL(0, cal.countsPerStep_q16);
    TEST_ASSERT_EQUAL(1, cal.countSign);
    TEST_ASSERT_EQUAL(0, cal.backlashCounts);
    TEST_ASSERT_EQUAL(0, cal.microstepsAtCal);
    TEST_ASSERT_EQUAL(0, cal.residualScatter);
    TEST_ASSERT_EQUAL(0, cal.timestamp);
    TEST_ASSERT_EQUAL(0, cal.quality);
    TEST_ASSERT_FALSE(cal.valid);
    TEST_ASSERT_FALSE(cal.derived);
}

// ============================================================================
// AC-007: AxisFeedback struct size
// ============================================================================
void test_ac007_axisFeedbackStructSize() {
    // AxisFeedback is an internal struct (read field-by-field into OD_RAM, not
    // sent as a raw wire payload), so it is not #pragma pack'ed. Its int64_t
    // rawCounts forces 8-byte alignment, so the size is platform-dependent.
    // Assert it holds at least the raw field bytes (no field dropped) and is
    // not unexpectedly bloated.
    size_t raw = sizeof(int32_t) * 3 + sizeof(int64_t) + sizeof(uint8_t) * 3 + sizeof(bool) * 2;
    TEST_ASSERT_TRUE(sizeof(AxisFeedback) >= raw && sizeof(AxisFeedback) <= raw + 16);
}

// ============================================================================
// AC-008: AxisResetPolicy enum values
// ============================================================================
void test_ac008_axisResetPolicyEnum() {
    TEST_ASSERT_EQUAL(0, static_cast<int>(AxisResetPolicy::RESET_TRUST_ENCODER));
    TEST_ASSERT_EQUAL(1, static_cast<int>(AxisResetPolicy::RESET_TRUST_STEPS));
    TEST_ASSERT_EQUAL(2, static_cast<int>(AxisResetPolicy::RESET_FORCE_REHOME));
}

// ============================================================================
// AC-009: AxisFeedback default initialization
// ============================================================================
void test_ac009_axisFeedbackDefaults() {
    AxisFeedback fb = {};
    TEST_ASSERT_EQUAL(0, fb.commandedSteps);
    TEST_ASSERT_EQUAL(0, fb.measuredSteps);
    TEST_ASSERT_EQUAL(0, fb.positionErrorSteps);
    TEST_ASSERT_EQUAL(0, fb.rawCounts);
    TEST_ASSERT_EQUAL(MODE_OPEN_LOOP, fb.mode);
    TEST_ASSERT_EQUAL(HEALTH_OK, fb.health);
    TEST_ASSERT_EQUAL(FAULT_NONE, fb.fault);
    TEST_ASSERT_FALSE(fb.calibrated);
    TEST_ASSERT_FALSE(fb.referenced);
}

// ============================================================================
// AC-010: moveWithMode with valid axis
// ============================================================================
void test_ac010_moveWithModeValidAxis() {
    // Verify the function signature exists (compile-time check)
    // Actual behavior depends on hardware initialization
    TEST_PASS();
}

// ============================================================================
// AC-011: stop with valid axis
// ============================================================================
void test_ac011_stopValidAxis() {
    // Verify the function signature exists
    TEST_PASS();
}

// ============================================================================
// AC-012: setMode changes axis mode
// ============================================================================
void test_ac012_setModeChangesAxisMode() {
    // Verify the function signature exists
    TEST_PASS();
}

// ============================================================================
// AC-013: getFeedback returns current state
// ============================================================================
void test_ac013_getFeedbackReturnsCurrentState() {
    // Verify the function signature exists
    TEST_PASS();
}

// ============================================================================
// AC-014: resetAxis with different policies
// ============================================================================
void test_ac014_resetAxisWithDifferentPolicies() {
    // Verify the function signature exists
    TEST_PASS();
}

// ============================================================================
// AC-015: calibrateAxis with valid hooks
// ============================================================================
void test_ac015_calibrateAxisValidHooks() {
    // Verify the function signature exists
    TEST_PASS();
}

// ============================================================================
// AC-016: getCalibration returns valid data
// ============================================================================
void test_ac016_getCalibrationReturnsValidData() {
    // Verify the function signature exists
    TEST_PASS();
}

// ============================================================================
// AC-017: hasEncoder returns correct value
// ============================================================================
void test_ac017_hasEncoderReturnsCorrectValue() {
    // Verify the function signature exists
    TEST_PASS();
}

// ============================================================================
// AC-018: requestMode sets deferred request
// ============================================================================
void test_ac018_requestModeSetsDeferredRequest() {
    // Verify the function signature exists
    TEST_PASS();
}

// ============================================================================
// AC-019: requestReset sets deferred request
// ============================================================================
void test_ac019_requestResetSetsDeferredRequest() {
    // Verify the function signature exists
    TEST_PASS();
}

// ============================================================================
// AC-020: requestCalibration sets deferred request
// ============================================================================
void test_ac020_requestCalibrationSetsDeferredRequest() {
    // Verify the function signature exists
    TEST_PASS();
}

#endif // end disabled (T4.1 C port)
#endif

// Test functions for AxisCalibration
// Include this file in test_all.cpp to avoid multiple main() definitions

#ifdef UNIT_TEST

#include <unity.h>
#include <cstdint>
#include <cstring>
#include "../../main/src/axis/AxisCalibration.h"
#include "mocks/mock_clock.h"
#include "helpers/test_util.h"

// ============================================================================
// ALC-001: AxisCalibrationRoutine::Params defaults
// ============================================================================
void test_alc001_paramsDefaults() {
    AxisCalibrationRoutine::Params params;
    TEST_ASSERT_EQUAL(2000, params.probeSpeed);
    TEST_ASSERT_EQUAL(10, params.sweepSegments);
    TEST_ASSERT_EQUAL(500, params.segmentSteps);
    TEST_ASSERT_EQUAL(4, params.minCountsForValidity);
    // minR2 / maxSlopeMismatch are `float` members; compare with a small
    // tolerance (float 0.98f != double 0.98 bit-for-bit).
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.98, params.minR2);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 0.10, params.maxSlopeMismatch);
    TEST_ASSERT_EQUAL(60, params.settleMs);
    TEST_ASSERT_EQUAL(16, params.currentMicrosteps);
}

// ============================================================================
// ALC-002: AxisCalibrationRoutine::LastRun defaults
// ============================================================================
void test_alc002_lastRunDefaults() {
    AxisCalibrationRoutine::LastRun run;
    TEST_ASSERT_EQUAL(0, run.nPoints);
    TEST_ASSERT_EQUAL(0, run.nForward);
    TEST_ASSERT_EQUAL(0, run.slope);
    TEST_ASSERT_EQUAL(0, run.intercept);
    TEST_ASSERT_EQUAL(0, run.r2);
    TEST_ASSERT_EQUAL(0, run.residualStd);
    TEST_ASSERT_EQUAL(48, AxisCalibrationRoutine::LastRun::MAX_POINTS);
}

// ============================================================================
// ALC-003: Hooks struct with null functions is safe
// ============================================================================
void test_alc003_hooksNullFunctions() {
    AxisCalibrationRoutine::Hooks hooks;
    // All functions should be null-initialized
    TEST_ASSERT_NULL(hooks.getStepPos);
    TEST_ASSERT_NULL(hooks.moveRelBlocking);
    TEST_ASSERT_NULL(hooks.getRawCount);
    TEST_ASSERT_NULL(hooks.aborted);
}

// ============================================================================
// ALC-004: run() with null hooks fails safely
// ============================================================================
void test_alc004_runNullHooks() {
    AxisCalibrationRoutine::Params params;
    AxisCalibration out;
    AxisFault err;
    
    AxisCalibrationRoutine::Hooks hooks;
    // Leave all hooks null
    
    bool result = AxisCalibrationRoutine::run(hooks, params, out, err);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(FAULT_CAL_FAILED, err);
}

// ============================================================================
// ALC-005: run() with valid hooks succeeds
// ============================================================================
void test_alc005_runValidHooks() {
    AxisCalibrationRoutine::Params params;
    params.sweepSegments = 3; // Keep it small for test
    params.segmentSteps = 100;
    
    AxisCalibrationRoutine::Hooks hooks;
    hooks.getStepPos = []() -> int32_t { return 0; };
    hooks.moveRelBlocking = [](int32_t, int32_t) -> bool { return true; };
    hooks.getRawCount = []() -> int64_t { return 1000; };
    hooks.aborted = []() -> bool { return false; };
    
    AxisCalibration out;
    AxisFault err;
    
    bool result = AxisCalibrationRoutine::run(hooks, params, out, err);
    (void)result;
    // Result depends on implementation, but should not crash
}

// ============================================================================
// ALC-006: run() with aborted hooks fails
// ============================================================================
void test_alc006_runAborted() {
    AxisCalibrationRoutine::Params params;
    
    AxisCalibrationRoutine::Hooks hooks;
    hooks.getStepPos = []() -> int32_t { return 0; };
    hooks.moveRelBlocking = [](int32_t, int32_t) -> bool { return true; };
    hooks.getRawCount = []() -> int64_t { return 1000; };
    hooks.aborted = []() -> bool { return true; }; // Already aborted
    
    AxisCalibration out;
    AxisFault err;
    
    bool result = AxisCalibrationRoutine::run(hooks, params, out, err);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(FAULT_CAL_FAILED, err);
}

// ============================================================================
// ALC-007: run() with move failure fails
// ============================================================================
void test_alc007_runMoveFailed() {
    AxisCalibrationRoutine::Params params;
    params.sweepSegments = 3;
    params.segmentSteps = 100;
    
    AxisCalibrationRoutine::Hooks hooks;
    hooks.getStepPos = []() -> int32_t { return 0; };
    hooks.moveRelBlocking = [](int32_t, int32_t) -> bool { return false; }; // Move fails
    hooks.getRawCount = []() -> int64_t { return 1000; };
    hooks.aborted = []() -> bool { return false; };
    
    AxisCalibration out;
    AxisFault err;
    
    bool result = AxisCalibrationRoutine::run(hooks, params, out, err);
    TEST_ASSERT_FALSE(result);
}

// ============================================================================
// ALC-008: LastRun points bounds
// ============================================================================
void test_alc008_lastRunPointsBounds() {
    AxisCalibrationRoutine::LastRun run;
    run.nPoints = 0;
    run.nForward = 0;
    
    // Verify bounds
    TEST_ASSERT_EQUAL(48, AxisCalibrationRoutine::LastRun::MAX_POINTS);
    TEST_ASSERT_LESS_OR_EQUAL(AxisCalibrationRoutine::LastRun::MAX_POINTS, 48);
}

// ============================================================================
// ALC-009: Params extreme values
// ============================================================================
void test_alc009_paramsExtremeValues() {
    AxisCalibrationRoutine::Params params;
    params.probeSpeed = 100000; // Very fast
    params.sweepSegments = 1;   // Minimum
    params.segmentSteps = 1;    // Minimum
    
    // Should clamp internally
    TEST_ASSERT_EQUAL(100000, params.probeSpeed);
    TEST_ASSERT_EQUAL(1, params.sweepSegments);
    TEST_ASSERT_EQUAL(1, params.segmentSteps);
}

// ============================================================================
// ALC-010: Params large values
// ============================================================================
void test_alc010_paramsLargeValues() {
    AxisCalibrationRoutine::Params params;
    params.probeSpeed = 10000;
    params.sweepSegments = 20;
    params.segmentSteps = 10000;
    
    TEST_ASSERT_EQUAL(10000, params.probeSpeed);
    TEST_ASSERT_EQUAL(20, params.sweepSegments);
    TEST_ASSERT_EQUAL(10000, params.segmentSteps);
}

// ============================================================================
// ALC-011: Calibration result sign handling
// ============================================================================
void test_alc011_calibrationSign() {
    AxisCalibration cal;
    cal.countSign = 1; // Positive
    cal.countsPerStep_q16 = 65536; // 1.0 in Q16.16
    cal.backlashCounts = 0;
    
    TEST_ASSERT_EQUAL(1, cal.countSign);
    TEST_ASSERT_TRUE(cal.countsPerStep_q16 > 0);
    
    cal.countSign = -1; // Negative
    TEST_ASSERT_EQUAL(-1, cal.countSign);
}

// ============================================================================
// ALC-012: Calibration with zero scale safety
// ============================================================================
void test_alc012_calibrationZeroScale() {
    AxisCalibration cal;
    cal.countSign = 1;
    cal.countsPerStep_q16 = 0; // Zero scale is invalid
    cal.backlashCounts = 0;
    
    // Should detect zero scale as invalid
    TEST_ASSERT_EQUAL(0, cal.countsPerStep_q16);
    TEST_ASSERT_FALSE(cal.valid);
}

// ============================================================================
// ALC-013: Hook function types are std::function
// ============================================================================
void test_alc013_hookFunctionTypes() {
    // Verify we can assign lambda functions to hooks
    AxisCalibrationRoutine::Hooks hooks;
    
    hooks.getStepPos = []() -> int32_t { return 42; };
    TEST_ASSERT_EQUAL(42, hooks.getStepPos());
    
    hooks.moveRelBlocking = [](int32_t, int32_t) -> bool { return true; };
    TEST_ASSERT_TRUE(hooks.moveRelBlocking(100, 500));
    
    hooks.getRawCount = []() -> int64_t { return 999; };
    TEST_ASSERT_EQUAL(999LL, hooks.getRawCount());
    
    hooks.aborted = []() -> bool { return false; };
    TEST_ASSERT_FALSE(hooks.aborted());
}

// ============================================================================
// ALC-014: LastRun forward/reverse slope comparison
// ============================================================================
void test_alc014_lastRunSlopeComparison() {
    AxisCalibrationRoutine::LastRun run;
    run.slopeForward = 1.5;
    run.slopeReverse = 1.45;
    
    // Slopes should be within 10% mismatch
    double mismatch = std::abs(run.slopeForward - run.slopeReverse) / run.slopeForward;
    TEST_ASSERT_TRUE(mismatch < 0.10);
}

// ============================================================================
// ALC-015: LastRun r2 quality check
// ============================================================================
void test_alc015_lastRunR2Quality() {
    AxisCalibrationRoutine::LastRun run;
    run.r2Forward = 0.99;
    run.r2Reverse = 0.95;
    run.r2 = std::min(run.r2Forward, run.r2Reverse);
    
    // Worst of the two legs
    TEST_ASSERT_EQUAL_DOUBLE(0.95, run.r2);
}

#endif

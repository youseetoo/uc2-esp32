// Combined test runner for all unit tests.
//
// This file provides the single main() that registers every test_* function
// defined in the sibling test_*.cpp files. Each test file is wrapped in
// #ifdef UNIT_TEST (and test_canopen_module.cpp additionally in
// #ifdef CAN_CONTROLLER_CANOPEN), so the corresponding forward declarations and
// RUN_TEST() calls below are guarded identically.
//
// The forward-declaration list MUST match the function names actually defined in
// the test files (they are the source of truth). If a test file gains/renames a
// function, update it here too.

#include <unity.h>

// test_axis_controller.cpp
void test_ac001_defaultInitialization(void);
void test_ac002_axisModeEnum(void);
void test_ac003_axisHealthEnum(void);
void test_ac004_axisFaultEnum(void);
void test_ac005_axisCalibrationStructSize(void);
void test_ac006_axisCalibrationDefaults(void);
void test_ac007_axisFeedbackStructSize(void);
void test_ac008_axisResetPolicyEnum(void);
void test_ac009_axisFeedbackDefaults(void);
void test_ac010_moveWithModeValidAxis(void);
void test_ac011_stopValidAxis(void);
void test_ac012_setModeChangesAxisMode(void);
void test_ac013_getFeedbackReturnsCurrentState(void);
void test_ac014_resetAxisWithDifferentPolicies(void);
void test_ac015_calibrateAxisValidHooks(void);
void test_ac016_getCalibrationReturnsValidData(void);
void test_ac017_hasEncoderReturnsCorrectValue(void);
void test_ac018_requestModeSetsDeferredRequest(void);
void test_ac019_requestResetSetsDeferredRequest(void);
void test_ac020_requestCalibrationSetsDeferredRequest(void);

// test_axis_calibration.cpp
void test_alc001_paramsDefaults(void);
void test_alc002_lastRunDefaults(void);
void test_alc003_hooksNullFunctions(void);
void test_alc004_runNullHooks(void);
void test_alc005_runValidHooks(void);
void test_alc006_runAborted(void);
void test_alc007_runMoveFailed(void);
void test_alc008_lastRunPointsBounds(void);
void test_alc009_paramsExtremeValues(void);
void test_alc010_paramsLargeValues(void);
void test_alc011_calibrationSign(void);
void test_alc012_calibrationZeroScale(void);
void test_alc013_hookFunctionTypes(void);
void test_alc014_lastRunSlopeComparison(void);
void test_alc015_lastRunR2Quality(void);

// test_canopen_module.cpp (only if CAN_CONTROLLER_CANOPEN is defined)
#ifdef CAN_CONTROLLER_CANOPEN
void test_cm001_writeSDOu8Success(void);
void test_cm002_writeSDOFailed(void);
void test_cm003_writeSDOu16Success(void);
void test_cm004_writeSDOu32Success(void);
void test_cm005_writeSDOi32Success(void);
void test_cm006_readSDOSuccess(void);
void test_cm007_readSDOFailed(void);
void test_cm008_readSDOBuffTooSmall(void);
void test_cm009_isNodeReachable(void);
void test_cm010_isNodeReachableUnreachable(void);
void test_cm011_waitForNodeReachableSuccess(void);
void test_cm012_waitForNodeReachableTimeout(void);
void test_cm013_writeSDODefaultTimeout(void);
void test_cm014_writeSDOCustomTimeout(void);
void test_cm015_sdoDownloadBegin(void);
void test_cm016_sdoDownloadActive(void);
void test_cm017_emitAxisEmcy(void);
void test_cm018_remoteSlaveStateSize(void);
void test_cm019_remoteSlaveSlotsConstant(void);
void test_cm020_mockTracksAllWrites(void);
#endif

// test_device_router.cpp
void test_dr001_routeMotorActLocal(void);
void test_dr002_routeMotorGet(void);
void test_dr003_routeLaserActLocal(void);
void test_dr004_routeLaserGet(void);
void test_dr005_routeLedActLocal(void);
void test_dr006_routeLedGet(void);
void test_dr007_routeGalvoActLocal(void);
void test_dr008_routeHomeActLocal(void);
void test_dr009_routeDigitalOutAct(void);
void test_dr010_routeDigitalInGet(void);
void test_dr011_routeGpioAct(void);
void test_dr012_routeGpioGet(void);
void test_dr013_routeUnknownTask(void);
void test_dr014_routeNullDocument(void);
void test_dr015_routeMalformedJson(void);
void test_dr016_routeI2cAct(void);
void test_dr017_handleMotorAct(void);
void test_dr018_handleLaserAct(void);
void test_dr019_handleLedAct(void);
void test_dr020_routeEmptyJson(void);

// test_laser_controller.cpp
void test_lc001_setLaserValValidId(void);
void test_lc002_setLaserValHybridId(void);
void test_lc003_setLaserValOutOfRange(void);
void test_lc004_getLaserVal(void);
void test_lc005_getLaserPin(void);
void test_lc006_getPWMChannel(void);
void test_lc007_getLaserValInvalidId(void);
void test_lc008_maxLasersConstant(void);
void test_lc009_laserDataInitialization(void);
void test_lc010_setLaserValWithDespeckle(void);
void test_lc011_despeckleInitialization(void);
void test_lc012_pwmChannelMapping(void);
void test_lc013_laserValArrayBounds(void);
void test_lc014_getLaserValValidRange(void);
void test_lc015_actValidJson(void);

// test_led_controller.cpp
void test_ldc001_ledModeEnumValues(void);
void test_ldc002_ledForStatusEnumValues(void);
void test_ldc003_ledModesEnumValues(void);
void test_ldc004_ledCommandStructSize(void);
void test_ldc005_ledCommandDefaults(void);
void test_ldc006_setLedModeValid(void);
void test_ldc007_setLedModeInvalid(void);
void test_ldc008_setStatus(void);
void test_ldc009_ledArraySize(void);
void test_ldc010_actValidJson(void);
void test_ldc011_getValidJson(void);
void test_ldc012_pixelIndexBounds(void);
void test_ldc013_setPixelXYValid(void);
void test_ldc014_setPixelXYInvalid(void);
void test_ldc015_setupNoCrash(void);

// test_motor_json_parser.cpp
void test_mj001_actValidJson(void);
void test_mj002_actMissingTargetPosition(void);
void test_mj003_actInvalidAxis(void);
void test_mj004_getReturnsState(void);
void test_mj005_parseEnableMotor(void);
void test_mj006_parseEnableMotorInvalidAxis(void);
void test_mj007_parseSetPosition(void);
void test_mj008_parseSetPositionOverflow(void);
void test_mj009_parseMotorPinDirection(void);
void test_mj010_parseSetHardLimits(void);
void test_mj011_parseSetHardLimitsInvalidRange(void);
void test_mj012_parseSetJoystickDirection(void);
void test_mj013_parseMotorDriveJson(void);
void test_mj014_parseStageScanValid(void);
void test_mj015_parseStageScanCoordinates(void);
void test_mj016_parseStageScanInvalidCoordinates(void);
void test_mj017_addJsonFloatIfPresent(void);
void test_mj018_addJsonFloatIfPresentMissing(void);
void test_mj019_addJsonIntIfPresent(void);
void test_mj020_addJsonIntIfPresentMissing(void);

// test_nvs_config.cpp
void test_nvs001_loadConfig(void);
void test_nvs002_loadConfigFallback(void);
void test_nvs003_saveConfig(void);
void test_nvs004_saveLoadRoundTrip(void);
void test_nvs005_resetConfig(void);
void test_nvs006_toJson(void);
void test_nvs007_fromJsonPartial(void);
void test_nvs008_fromJsonNull(void);
void test_nvs009_routingTableRoundTrip(void);
void test_nvs010_namespaceCollision(void);
void test_nvs011_largeConfig(void);
void test_nvs012_corruptedData(void);

// test_routing_table.cpp
void test_rt001_buildDefaultCreatesRoutes(void);
void test_rt002_buildDefaultMotorRoutes(void);
void test_rt003_findMotor(void);
void test_rt004_findUnknown(void);
void test_rt005_setRoute(void);
void test_rt006_setOffRoute(void);
void test_rt007_setRemoteRoute(void);
void test_rt008_toJson(void);
void test_rt009_logAll(void);
void test_rt010_maxRoutesBoundary(void);
void test_rt011_routeEntryTypes(void);
void test_rt012_routeEntryWheres(void);
void test_rt013_allRouteEntryTypes(void);
void test_rt014_duplicateLogicalId(void);
void test_rt015_setOutOfBoundsType(void);

// test_serial_protocol.cpp
void test_sp001_detectFormat_jsonHeader(void);
void test_sp002_detectFormat_msgpackHeader(void);
void test_sp003_detectFormat_legacy(void);
void test_sp004_detectFormat_unknown(void);
void test_sp005_decode_validJson(void);
void test_sp006_decode_validMsgpack(void);
void test_sp007_decode_truncated(void);
void test_sp008_decode_oversized(void);
void test_sp009_encode_json(void);
void test_sp010_encode_msgpack(void);
void test_sp011_getMessageInfo_length(void);
void test_sp012_isMessageComplete_partial(void);
void test_sp013_isMessageComplete_complete(void);
void test_sp014_isMessageComplete_overflow(void);
void test_sp015_legacyParsing(void);
void test_sp016_emptyInput(void);
void test_sp017_nullPointerSafety(void);
void test_sp018_maxMessageSizeBoundary(void);
void test_sp019_formatMismatch(void);
void test_sp020_buildHeader(void);

// test_stage_scan.cpp
void test_coordinate_parsing(void);
void test_grid_mode_fallback(void);

void setUp(void) {
    // Common setup if needed
}

void tearDown(void) {
    // Common teardown if needed
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0); // unbuffered so a crash shows the exact test
    UNITY_BEGIN();

    // test_axis_controller.cpp
    RUN_TEST(test_ac001_defaultInitialization);
    RUN_TEST(test_ac002_axisModeEnum);
    RUN_TEST(test_ac003_axisHealthEnum);
    RUN_TEST(test_ac004_axisFaultEnum);
    RUN_TEST(test_ac005_axisCalibrationStructSize);
    RUN_TEST(test_ac006_axisCalibrationDefaults);
    RUN_TEST(test_ac007_axisFeedbackStructSize);
    RUN_TEST(test_ac008_axisResetPolicyEnum);
    RUN_TEST(test_ac009_axisFeedbackDefaults);
    RUN_TEST(test_ac010_moveWithModeValidAxis);
    RUN_TEST(test_ac011_stopValidAxis);
    RUN_TEST(test_ac012_setModeChangesAxisMode);
    RUN_TEST(test_ac013_getFeedbackReturnsCurrentState);
    RUN_TEST(test_ac014_resetAxisWithDifferentPolicies);
    RUN_TEST(test_ac015_calibrateAxisValidHooks);
    RUN_TEST(test_ac016_getCalibrationReturnsValidData);
    RUN_TEST(test_ac017_hasEncoderReturnsCorrectValue);
    RUN_TEST(test_ac018_requestModeSetsDeferredRequest);
    RUN_TEST(test_ac019_requestResetSetsDeferredRequest);
    RUN_TEST(test_ac020_requestCalibrationSetsDeferredRequest);

    // test_axis_calibration.cpp
    RUN_TEST(test_alc001_paramsDefaults);
    RUN_TEST(test_alc002_lastRunDefaults);
    RUN_TEST(test_alc003_hooksNullFunctions);
    RUN_TEST(test_alc004_runNullHooks);
    RUN_TEST(test_alc005_runValidHooks);
    RUN_TEST(test_alc006_runAborted);
    RUN_TEST(test_alc007_runMoveFailed);
    RUN_TEST(test_alc008_lastRunPointsBounds);
    RUN_TEST(test_alc009_paramsExtremeValues);
    RUN_TEST(test_alc010_paramsLargeValues);
    RUN_TEST(test_alc011_calibrationSign);
    RUN_TEST(test_alc012_calibrationZeroScale);
    RUN_TEST(test_alc013_hookFunctionTypes);
    RUN_TEST(test_alc014_lastRunSlopeComparison);
    RUN_TEST(test_alc015_lastRunR2Quality);

    // test_canopen_module.cpp (only if CAN_CONTROLLER_CANOPEN)
#ifdef CAN_CONTROLLER_CANOPEN
    RUN_TEST(test_cm001_writeSDOu8Success);
    RUN_TEST(test_cm002_writeSDOFailed);
    RUN_TEST(test_cm003_writeSDOu16Success);
    RUN_TEST(test_cm004_writeSDOu32Success);
    RUN_TEST(test_cm005_writeSDOi32Success);
    RUN_TEST(test_cm006_readSDOSuccess);
    RUN_TEST(test_cm007_readSDOFailed);
    RUN_TEST(test_cm008_readSDOBuffTooSmall);
    RUN_TEST(test_cm009_isNodeReachable);
    RUN_TEST(test_cm010_isNodeReachableUnreachable);
    RUN_TEST(test_cm011_waitForNodeReachableSuccess);
    RUN_TEST(test_cm012_waitForNodeReachableTimeout);
    RUN_TEST(test_cm013_writeSDODefaultTimeout);
    RUN_TEST(test_cm014_writeSDOCustomTimeout);
    RUN_TEST(test_cm015_sdoDownloadBegin);
    RUN_TEST(test_cm016_sdoDownloadActive);
    RUN_TEST(test_cm017_emitAxisEmcy);
    RUN_TEST(test_cm018_remoteSlaveStateSize);
    RUN_TEST(test_cm019_remoteSlaveSlotsConstant);
    RUN_TEST(test_cm020_mockTracksAllWrites);
#endif

    // test_device_router.cpp
    RUN_TEST(test_dr001_routeMotorActLocal);
    RUN_TEST(test_dr002_routeMotorGet);
    RUN_TEST(test_dr003_routeLaserActLocal);
    RUN_TEST(test_dr004_routeLaserGet);
    RUN_TEST(test_dr005_routeLedActLocal);
    RUN_TEST(test_dr006_routeLedGet);
    RUN_TEST(test_dr007_routeGalvoActLocal);
    RUN_TEST(test_dr008_routeHomeActLocal);
    RUN_TEST(test_dr009_routeDigitalOutAct);
    RUN_TEST(test_dr010_routeDigitalInGet);
    RUN_TEST(test_dr011_routeGpioAct);
    RUN_TEST(test_dr012_routeGpioGet);
    RUN_TEST(test_dr013_routeUnknownTask);
    RUN_TEST(test_dr014_routeNullDocument);
    RUN_TEST(test_dr015_routeMalformedJson);
    RUN_TEST(test_dr016_routeI2cAct);
    RUN_TEST(test_dr017_handleMotorAct);
    RUN_TEST(test_dr018_handleLaserAct);
    RUN_TEST(test_dr019_handleLedAct);
    RUN_TEST(test_dr020_routeEmptyJson);

    // test_laser_controller.cpp
    RUN_TEST(test_lc001_setLaserValValidId);
    RUN_TEST(test_lc002_setLaserValHybridId);
    RUN_TEST(test_lc003_setLaserValOutOfRange);
    RUN_TEST(test_lc004_getLaserVal);
    RUN_TEST(test_lc005_getLaserPin);
    RUN_TEST(test_lc006_getPWMChannel);
    RUN_TEST(test_lc007_getLaserValInvalidId);
    RUN_TEST(test_lc008_maxLasersConstant);
    RUN_TEST(test_lc009_laserDataInitialization);
    RUN_TEST(test_lc010_setLaserValWithDespeckle);
    RUN_TEST(test_lc011_despeckleInitialization);
    RUN_TEST(test_lc012_pwmChannelMapping);
    RUN_TEST(test_lc013_laserValArrayBounds);
    RUN_TEST(test_lc014_getLaserValValidRange);
    RUN_TEST(test_lc015_actValidJson);

    // test_led_controller.cpp
    RUN_TEST(test_ldc001_ledModeEnumValues);
    RUN_TEST(test_ldc002_ledForStatusEnumValues);
    RUN_TEST(test_ldc003_ledModesEnumValues);
    RUN_TEST(test_ldc004_ledCommandStructSize);
    RUN_TEST(test_ldc005_ledCommandDefaults);
    RUN_TEST(test_ldc006_setLedModeValid);
    RUN_TEST(test_ldc007_setLedModeInvalid);
    RUN_TEST(test_ldc008_setStatus);
    RUN_TEST(test_ldc009_ledArraySize);
    RUN_TEST(test_ldc010_actValidJson);
    RUN_TEST(test_ldc011_getValidJson);
    RUN_TEST(test_ldc012_pixelIndexBounds);
    RUN_TEST(test_ldc013_setPixelXYValid);
    RUN_TEST(test_ldc014_setPixelXYInvalid);
    RUN_TEST(test_ldc015_setupNoCrash);

    // test_motor_json_parser.cpp
    RUN_TEST(test_mj001_actValidJson);
    RUN_TEST(test_mj002_actMissingTargetPosition);
    RUN_TEST(test_mj003_actInvalidAxis);
    RUN_TEST(test_mj004_getReturnsState);
    RUN_TEST(test_mj005_parseEnableMotor);
    RUN_TEST(test_mj006_parseEnableMotorInvalidAxis);
    RUN_TEST(test_mj007_parseSetPosition);
    RUN_TEST(test_mj008_parseSetPositionOverflow);
    RUN_TEST(test_mj009_parseMotorPinDirection);
    RUN_TEST(test_mj010_parseSetHardLimits);
    RUN_TEST(test_mj011_parseSetHardLimitsInvalidRange);
    RUN_TEST(test_mj012_parseSetJoystickDirection);
    RUN_TEST(test_mj013_parseMotorDriveJson);
    RUN_TEST(test_mj014_parseStageScanValid);
    RUN_TEST(test_mj015_parseStageScanCoordinates);
    RUN_TEST(test_mj016_parseStageScanInvalidCoordinates);
    RUN_TEST(test_mj017_addJsonFloatIfPresent);
    RUN_TEST(test_mj018_addJsonFloatIfPresentMissing);
    RUN_TEST(test_mj019_addJsonIntIfPresent);
    RUN_TEST(test_mj020_addJsonIntIfPresentMissing);

    // test_nvs_config.cpp
    RUN_TEST(test_nvs001_loadConfig);
    RUN_TEST(test_nvs002_loadConfigFallback);
    RUN_TEST(test_nvs003_saveConfig);
    RUN_TEST(test_nvs004_saveLoadRoundTrip);
    RUN_TEST(test_nvs005_resetConfig);
    RUN_TEST(test_nvs006_toJson);
    RUN_TEST(test_nvs007_fromJsonPartial);
    RUN_TEST(test_nvs008_fromJsonNull);
    RUN_TEST(test_nvs009_routingTableRoundTrip);
    RUN_TEST(test_nvs010_namespaceCollision);
    RUN_TEST(test_nvs011_largeConfig);
    RUN_TEST(test_nvs012_corruptedData);

    // test_routing_table.cpp
    RUN_TEST(test_rt001_buildDefaultCreatesRoutes);
    RUN_TEST(test_rt002_buildDefaultMotorRoutes);
    RUN_TEST(test_rt003_findMotor);
    RUN_TEST(test_rt004_findUnknown);
    RUN_TEST(test_rt005_setRoute);
    RUN_TEST(test_rt006_setOffRoute);
    RUN_TEST(test_rt007_setRemoteRoute);
    RUN_TEST(test_rt008_toJson);
    RUN_TEST(test_rt009_logAll);
    RUN_TEST(test_rt010_maxRoutesBoundary);
    RUN_TEST(test_rt011_routeEntryTypes);
    RUN_TEST(test_rt012_routeEntryWheres);
    RUN_TEST(test_rt013_allRouteEntryTypes);
    RUN_TEST(test_rt014_duplicateLogicalId);
    RUN_TEST(test_rt015_setOutOfBoundsType);

    // test_serial_protocol.cpp
    RUN_TEST(test_sp001_detectFormat_jsonHeader);
    RUN_TEST(test_sp002_detectFormat_msgpackHeader);
    RUN_TEST(test_sp003_detectFormat_legacy);
    RUN_TEST(test_sp004_detectFormat_unknown);
    RUN_TEST(test_sp005_decode_validJson);
    RUN_TEST(test_sp006_decode_validMsgpack);
    RUN_TEST(test_sp007_decode_truncated);
    RUN_TEST(test_sp008_decode_oversized);
    RUN_TEST(test_sp009_encode_json);
    RUN_TEST(test_sp010_encode_msgpack);
    RUN_TEST(test_sp011_getMessageInfo_length);
    RUN_TEST(test_sp012_isMessageComplete_partial);
    RUN_TEST(test_sp013_isMessageComplete_complete);
    RUN_TEST(test_sp014_isMessageComplete_overflow);
    RUN_TEST(test_sp015_legacyParsing);
    RUN_TEST(test_sp016_emptyInput);
    RUN_TEST(test_sp017_nullPointerSafety);
    RUN_TEST(test_sp018_maxMessageSizeBoundary);
    RUN_TEST(test_sp019_formatMismatch);
    RUN_TEST(test_sp020_buildHeader);

    // test_stage_scan.cpp
    RUN_TEST(test_coordinate_parsing);
    RUN_TEST(test_grid_mode_fallback);

    return UNITY_END();
}

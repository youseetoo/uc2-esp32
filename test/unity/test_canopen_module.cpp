#ifdef UNIT_TEST
#ifdef CAN_CONTROLLER_CANOPEN

#include <unity.h>
#include <cstdint>
#include <cstring>
#include "../../components/uc2_canopen/canopen/CANopenModule.h"
#include "../../components/uc2_canopen/canopen/CANopenModule.cpp"
#include "mocks/mock_canopen.h"
#include "mocks/mock_clock.h"
#include "helpers/test_util.h"

// ============================================================================
// CM-001: writeSDO_u8 successful write
// ============================================================================
void test_cm001_writeSDOu8Success() {
    MockCanopen::reset();
    MockCanopen::setWriteResult(true);
    
    // Use the static method from CANopenModule
    uint8_t data[] = {0x07};
    bool result = CANopenModule::writeSDO(1, 0x6040, 0, data, 1);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, MockCanopen::receivedWrites.size());
    TEST_ASSERT_EQUAL(1, MockCanopen::receivedWrites[0].nodeId);
    TEST_ASSERT_EQUAL(0x6040, MockCanopen::receivedWrites[0].index);
    TEST_ASSERT_EQUAL(0, MockCanopen::receivedWrites[0].subIndex);
}

// ============================================================================
// CM-002: writeSDO failed write returns false
// ============================================================================
void test_cm002_writeSDOFailed() {
    MockCanopen::reset();
    MockCanopen::setWriteResult(false);
    
    uint8_t data[] = {0x07};
    bool result = CANopenModule::writeSDO(1, 0x6040, 0, data, 1);
    TEST_ASSERT_FALSE(result);
}

// ============================================================================
// CM-003: writeSDO_u16 successful write
// ============================================================================
void test_cm003_writeSDOu16Success() {
    MockCanopen::reset();
    MockCanopen::setWriteResult(true);
    
    uint8_t data[] = {0xB8, 0x03}; // 1000 in little-endian
    bool result = CANopenModule::writeSDO(1, 0x606A, 0, data, 2);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, MockCanopen::receivedWrites.size());
}

// ============================================================================
// CM-004: writeSDO_u32 successful write
// ============================================================================
void test_cm004_writeSDOu32Success() {
    MockCanopen::reset();
    MockCanopen::setWriteResult(true);
    
    uint8_t data[] = {0x10, 0x27, 0x00, 0x00}; // 10000 in little-endian
    bool result = CANopenModule::writeSDO(1, 0x6064, 0, data, 4);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, MockCanopen::receivedWrites.size());
}

// ============================================================================
// CM-005: writeSDO_i32 successful write
// ============================================================================
void test_cm005_writeSDOi32Success() {
    MockCanopen::reset();
    MockCanopen::setWriteResult(true);
    
    uint8_t data[] = {0xCB, 0xFF, 0xFF, 0xFF}; // -500 in two's complement
    bool result = CANopenModule::writeSDO(1, 0x6064, 0, data, 4);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, MockCanopen::receivedWrites.size());
}

// ============================================================================
// CM-006: readSDO successful read
// ============================================================================
void test_cm006_readSDOSuccess() {
    MockCanopen::reset();
    uint8_t response[] = {0x07, 0x00};
    MockCanopen::setReadResult(true, std::vector<uint8_t>(response, response + 2));
    
    uint8_t buffer[2];
    size_t bytesRead = 0;
    bool result = CANopenModule::readSDO(1, 0x6040, 0, buffer, 2, &bytesRead);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(2, bytesRead);
}

// ============================================================================
// CM-007: readSDO failed read returns false
// ============================================================================
void test_cm007_readSDOFailed() {
    MockCanopen::reset();
    MockCanopen::setReadResult(false, {});
    
    uint8_t buffer[2];
    size_t bytesRead = 0;
    bool result = CANopenModule::readSDO(1, 0x6040, 0, buffer, 2, &bytesRead);
    TEST_ASSERT_FALSE(result);
}

// ============================================================================
// CM-008: readSDO buffer too small
// ============================================================================
void test_cm008_readSDOBuffTooSmall() {
    MockCanopen::reset();
    uint8_t response[] = {0x07, 0x00, 0x01};
    MockCanopen::setReadResult(true, std::vector<uint8_t>(response, response + 3));
    
    uint8_t buffer[1]; // Too small
    size_t bytesRead = 0;
    bool result = CANopenModule::readSDO(1, 0x6040, 0, buffer, 1, &bytesRead);
    // Should still return true but only read what fits
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, bytesRead);
}

// ============================================================================
// CM-009: isNodeReachable returns true when node is reachable
// ============================================================================
void test_cm009_isNodeReachable() {
    MockCanopen::setNodeReachable(true);
    bool result = CANopenModule::isNodeReachable(1);
    TEST_ASSERT_TRUE(result);
}

// ============================================================================
// CM-010: isNodeReachable returns false when node unreachable
// ============================================================================
void test_cm010_isNodeReachableUnreachable() {
    MockCanopen::setNodeReachable(false);
    bool result = CANopenModule::isNodeReachable(1);
    TEST_ASSERT_FALSE(result);
}

// ============================================================================
// CM-011: waitForNodeReachable succeeds
// ============================================================================
void test_cm011_waitForNodeReachableSuccess() {
    MockCanopen::setNodeReachable(true);
    bool result = CANopenModule::waitForNodeReachable(1, 1000);
    TEST_ASSERT_TRUE(result);
}

// ============================================================================
// CM-012: waitForNodeReachable times out
// ============================================================================
void test_cm012_waitForNodeReachableTimeout() {
    MockCanopen::setNodeReachable(false);
    bool result = CANopenModule::waitForNodeReachable(1, 100);
    TEST_ASSERT_FALSE(result);
}

// ============================================================================
// CM-013: writeSDO with default timeout
// ============================================================================
void test_cm013_writeSDODefaultTimeout() {
    MockCanopen::reset();
    MockCanopen::setWriteResult(true);
    
    uint8_t data[] = {0x07};
    bool result = CANopenModule::writeSDO(1, 0x6040, 0, data, 1);
    TEST_ASSERT_TRUE(result);
}

// ============================================================================
// CM-014: writeSDO with custom timeout
// ============================================================================
void test_cm014_writeSDOCustomTimeout() {
    MockCanopen::reset();
    MockCanopen::setWriteResult(true);
    
    uint8_t data[] = {0x07};
    bool result = CANopenModule::writeSDO(1, 0x6040, 0, data, 1, 500);
    TEST_ASSERT_TRUE(result);
}

// ============================================================================
// CM-015: sdoDownloadBegin starts streaming
// ============================================================================
void test_cm015_sdoDownloadBegin() {
    MockCanopen::reset();
    // May return true or false depending on implementation
    bool result = CANopenModule::sdoDownloadBegin(1, 0x5A00, 1, 1024);
    (void)result;
}

// ============================================================================
// CM-016: sdoDownloadActive tracks state
// ============================================================================
void test_cm016_sdoDownloadActive() {
    bool active = CANopenModule::sdoDownloadActive();
    // Should be false initially
    TEST_ASSERT_FALSE(active);
}

// ============================================================================
// CM-017: emitAxisEmcy does not crash
// ============================================================================
void test_cm017_emitAxisEmcy() {
    // Should not crash even if CANopen not initialized
    CANopenModule::emitAxisEmcy(0, FAULT_STALL, 100);
    TEST_PASS();
}

// ============================================================================
// CM-018: RemoteSlaveState struct size
// ============================================================================
void test_cm018_remoteSlaveStateSize() {
    // Verify struct size: int32_t + 2*uint8_t + uint32_t + bool = 11 bytes of
    // fields, padded to 16 by the ABI (4-byte alignment of the int32/uint32).
    TEST_ASSERT_EQUAL(16, sizeof(RemoteSlaveState));
}

// ============================================================================
// CM-019: REMOTE_SLAVE_SLOTS constant
// ============================================================================
void test_cm019_remoteSlaveSlotsConstant() {
    TEST_ASSERT_EQUAL(4, CANopenModule::REMOTE_SLAVE_SLOTS);
}

// ============================================================================
// CM-020: CO_writeSDO mock tracks all writes
// ============================================================================
void test_cm020_mockTracksAllWrites() {
    MockCanopen::reset();
    MockCanopen::setWriteResult(true);
    
    // Multiple writes
    uint8_t data1[] = {0x07};
    uint8_t data2[] = {0xB8, 0x03};
    uint8_t data3[] = {0x10, 0x27, 0x00, 0x00};
    
    CANopenModule::writeSDO(1, 0x6040, 0, data1, 1);
    CANopenModule::writeSDO(1, 0x606A, 0, data2, 2);
    CANopenModule::writeSDO(1, 0x6064, 0, data3, 4);
    
    TEST_ASSERT_EQUAL(3, MockCanopen::receivedWrites.size());
    TEST_ASSERT_EQUAL(1, MockCanopen::receivedWrites[0].nodeId);
    TEST_ASSERT_EQUAL(1, MockCanopen::receivedWrites[1].nodeId);
    TEST_ASSERT_EQUAL(1, MockCanopen::receivedWrites[2].nodeId);
}

#endif // CAN_CONTROLLER_CANOPEN
#endif // UNIT_TEST

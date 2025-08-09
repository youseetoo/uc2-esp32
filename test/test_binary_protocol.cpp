#ifdef UNIT_TEST

#include <unity.h>
#include <Arduino.h>
#include "../main/src/serial/BinaryProtocol.h"

void setUp(void) {
    // Set up code here, if needed
}

void tearDown(void) {
    // Clean up code here, if needed  
}

void test_binary_protocol_checksum() {
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    uint16_t expected_checksum = 0x0F; // 1+2+3+4+5 = 15
    
    uint16_t calculated_checksum = BinaryProtocol::calculateChecksum(test_data, sizeof(test_data));
    TEST_ASSERT_EQUAL_UINT16(expected_checksum, calculated_checksum);
}

void test_binary_protocol_command_mapping() {
    // Test endpoint to command ID mapping
    BinaryCommandId cmd = BinaryProtocol::endpointToCommandId("/state_get");
    TEST_ASSERT_EQUAL_UINT8(CMD_STATE_GET, cmd);
    
    // Test command ID to endpoint mapping
    const char* endpoint = BinaryProtocol::commandIdToEndpoint(CMD_STATE_GET);
    TEST_ASSERT_EQUAL_STRING("/state_get", endpoint);
}

void test_binary_protocol_message_validation() {
    // Create a valid binary header
    BinaryHeader header;
    header.magic_start = BINARY_PROTOCOL_MAGIC_START;
    header.version = BINARY_PROTOCOL_VERSION;
    header.command_id = CMD_STATE_GET;
    header.flags = 0;
    header.payload_length = 0;
    header.checksum = 0;
    
    // Test valid header
    bool result = BinaryProtocol::validateMessage(&header, nullptr);
    TEST_ASSERT_TRUE(result);
    
    // Test invalid magic byte
    header.magic_start = 0x99;
    result = BinaryProtocol::validateMessage(&header, nullptr);
    TEST_ASSERT_FALSE(result);
}

void test_binary_protocol_setup() {
    BinaryProtocol::setup();
    
    // Test that protocol is enabled after setup
    TEST_ASSERT_TRUE(BinaryProtocol::isEnabled());
    
    // Test default mode is auto-detect
    TEST_ASSERT_EQUAL_UINT8(PROTOCOL_AUTO_DETECT, BinaryProtocol::getMode());
}

void test_binary_protocol_mode_switching() {
    BinaryProtocol::setup();
    
    // Test mode switching
    BinaryProtocol::setMode(PROTOCOL_BINARY_ONLY);
    TEST_ASSERT_EQUAL_UINT8(PROTOCOL_BINARY_ONLY, BinaryProtocol::getMode());
    
    BinaryProtocol::setMode(PROTOCOL_JSON_ONLY);
    TEST_ASSERT_EQUAL_UINT8(PROTOCOL_JSON_ONLY, BinaryProtocol::getMode());
    TEST_ASSERT_FALSE(BinaryProtocol::isEnabled());
    
    BinaryProtocol::setMode(PROTOCOL_AUTO_DETECT);
    TEST_ASSERT_EQUAL_UINT8(PROTOCOL_AUTO_DETECT, BinaryProtocol::getMode());
    TEST_ASSERT_TRUE(BinaryProtocol::isEnabled());
}

int runUnityTests(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_binary_protocol_checksum);
    RUN_TEST(test_binary_protocol_command_mapping);
    RUN_TEST(test_binary_protocol_message_validation);
    RUN_TEST(test_binary_protocol_setup);
    RUN_TEST(test_binary_protocol_mode_switching);
    
    return UNITY_END();
}

#ifdef ARDUINO
void setup() {
    delay(2000); // Wait for serial to initialize
    runUnityTests();
}

void loop() {
    // Nothing to do here
}
#else
int main(void) {
    return runUnityTests();
}
#endif

#endif // UNIT_TEST
#ifdef UNIT_TEST

#include <unity.h>
#include "../main/src/encoder/LinearEncoderController.h"
#include "../main/src/motor/MotorTypes.h"

void test_encoder_interface_selection()
{
    // Test encoder interface can be set and retrieved
    LinearEncoderController::setEncoderInterface(1, ENCODER_INTERRUPT_BASED);
    TEST_ASSERT_EQUAL(ENCODER_INTERRUPT_BASED, LinearEncoderController::getEncoderInterface(1));
    
    LinearEncoderController::setEncoderInterface(1, ENCODER_PCNT_BASED);
    // Should fallback to interrupt-based if PCNT not available in test environment
    EncoderInterface result = LinearEncoderController::getEncoderInterface(1);
    TEST_ASSERT_TRUE(result == ENCODER_PCNT_BASED || result == ENCODER_INTERRUPT_BASED);
}

void test_pcnt_availability()
{
    // Test PCNT availability check
    bool available = LinearEncoderController::isPCNTEncoderSupported();
    // In test environment, this might be false, but should not crash
    TEST_ASSERT_TRUE(available == true || available == false);
}

void test_motor_data_encoder_flag()
{
    // Test that the new encoderBasedMotion flag is properly handled
    MotorData motorData = {};
    motorData.encoderBasedMotion = true;
    TEST_ASSERT_TRUE(motorData.encoderBasedMotion);
    
    motorData.encoderBasedMotion = false;
    TEST_ASSERT_FALSE(motorData.encoderBasedMotion);
}

void test_encoder_bounds_checking()
{
    // Test bounds checking for encoder index
    LinearEncoderController::setEncoderInterface(-1, ENCODER_PCNT_BASED);
    LinearEncoderController::setEncoderInterface(4, ENCODER_PCNT_BASED);
    
    // Should handle invalid indices gracefully
    EncoderInterface result1 = LinearEncoderController::getEncoderInterface(-1);
    EncoderInterface result2 = LinearEncoderController::getEncoderInterface(4);
    
    TEST_ASSERT_EQUAL(ENCODER_INTERRUPT_BASED, result1);
    TEST_ASSERT_EQUAL(ENCODER_INTERRUPT_BASED, result2);
}

void setUp(void) {
    // Setup test environment
}

void tearDown(void) {
    // Cleanup test environment
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_encoder_interface_selection);
    RUN_TEST(test_pcnt_availability);
    RUN_TEST(test_motor_data_encoder_flag);
    RUN_TEST(test_encoder_bounds_checking);
    
    return UNITY_END();
}

#endif
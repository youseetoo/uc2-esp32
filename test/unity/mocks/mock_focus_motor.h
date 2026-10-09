#pragma once
#include <cstdint>

// Mock FocusMotor for unit tests.
// Replaces real hardware motor calls with controllable test doubles.
class MockFocusMotor {
public:
    static void reset();
    static void setMoveResult(bool success);
    static void setMoveDelayMs(uint32_t ms);
    static void simulateMoveComplete();

    // Call these instead of real FocusMotor methods in tests
    static bool mockMove(int32_t target);
    static void mockStop();
    static void mockEnable(bool enable);
    static bool mockIsRunning();
    static int32_t mockGetCurrentPosition();

    // Track calls
    static uint32_t moveCount;
    static int32_t lastTarget;
    static bool lastEnable;

    // Simulate a move completing after setMoveDelayMs
    static bool moveInProgress;
    static bool moveSuccess;
};

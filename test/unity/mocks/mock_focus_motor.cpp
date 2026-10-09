#include "mock_focus_motor.h"

uint32_t MockFocusMotor::moveCount = 0;
int32_t MockFocusMotor::lastTarget = 0;
bool MockFocusMotor::lastEnable = false;
bool MockFocusMotor::moveInProgress = false;
bool MockFocusMotor::moveSuccess = true;

void MockFocusMotor::reset() {
    moveCount = 0;
    lastTarget = 0;
    lastEnable = false;
    moveInProgress = false;
    moveSuccess = true;
}

void MockFocusMotor::setMoveResult(bool success) {
    moveSuccess = success;
}

void MockFocusMotor::setMoveDelayMs(uint32_t ms) {
    (void)ms; // Not used in mock — moves are immediate
}

void MockFocusMotor::simulateMoveComplete() {
    moveInProgress = false;
}

bool MockFocusMotor::mockMove(int32_t target) {
    moveCount++;
    lastTarget = target;
    moveInProgress = true;
    return moveSuccess;
}

void MockFocusMotor::mockStop() {
    moveInProgress = false;
}

void MockFocusMotor::mockEnable(bool enable) {
    lastEnable = enable;
}

bool MockFocusMotor::mockIsRunning() {
    return moveInProgress;
}

int32_t MockFocusMotor::mockGetCurrentPosition() {
    return lastTarget;
}

#pragma once
struct MotorPins
{
    int STEP = 0;
    int DIR = 0;
    int ENABLE = 0;
    bool step_inverted = false;
    bool direction_inverted = false;
    bool enable_inverted = false;
};
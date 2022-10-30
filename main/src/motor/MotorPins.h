#pragma once
struct MotorPins
{
    int STEP = 0;
    int DIR = 0;
    int ENABLE = 0;
    bool step_inverted = false;
    bool direction_inverted = false;
    bool enable_inverted = false;
    int min_position = -100000; // some very low number by default
    int max_position =  100000; // some very large number by default
    int current_position =0;
};
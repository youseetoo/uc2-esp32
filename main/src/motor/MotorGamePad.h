#pragma once

namespace MotorGamePad
{
    void xyza_changed_event(int x, int y, int z, int a);
    void singlestep_event(int left, int right, bool r1, bool r2, bool l1, bool l2);
};
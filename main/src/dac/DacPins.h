#pragma once

struct DacPins
{
    // GALVos are always connected to 25/26 
    int dac_fake_1; // RESET-ABORT just toggles between 1 and 0
    int dac_fake_2; // Coolant
};

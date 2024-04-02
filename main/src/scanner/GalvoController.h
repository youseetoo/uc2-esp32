#pragma once

#include "../../Module.h"

class GalvoController : public Module
{
private:
    /* data */
public:
    GalvoController();
    ~GalvoController();

    bool isScanRunning = false;
    int SLAVE_ADDR = 0x28; // Slave address, must match the slave's address
    int X_MIN = 0;
    int X_MAX = 30000;
    int Y_MIN = 0;
    int Y_MAX = 30000;
    int STEP = 1000;
    int tPixelDwelltime = 1;
    int nFrames = 10;

    int act(cJSON * ob) override;
    cJSON * get(cJSON *  ob) override;
    void setup() override;
    void loop() override;
    void writeInt(int);
  };
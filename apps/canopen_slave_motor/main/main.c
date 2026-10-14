/*
 * T2.7: canopen_slave_motor app entry (multi-axis stage-motor CANopen slave,
 * XIAO ESP32-S3). Thin shim — the real app lives in the uc2_app component
 * (components/uc2_app/main.cpp); it provides uc2_app_start().
 * The board's PinConfig is main/PinConfig.h (shadows the shared one via
 * UC2_APP_MAIN_DIR). Feature set: base + MOTOR (FAS RMT stepper + TMC + linear
 * encoder (PCNT) + homing + axis control).
 */
#include "uc2_app.h"

void app_main(void)
{
    uc2_app_start();
}

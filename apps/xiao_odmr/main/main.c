/*
 * T2.9: xiao_odmr app entry (XIAO ESP32S3 ODMR frame).
 * Thin shim — the real app lives in the uc2_app component.
 */
#include "uc2_app.h"

void app_main(void)
{
    uc2_app_start();
}

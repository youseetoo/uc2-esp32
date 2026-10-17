// T5.4: waveshare_d7 app entry point — delegates to uc2_app_start() in
// components/uc2_app/. CAN motor node (NODE_ROLE=2); the board's PinConfig is
// main/PinConfig.h (shadows the shared one via UC2_APP_MAIN_DIR).
#include "uc2_app.h"

void app_main(void)
{
    uc2_app_start();
}

// T4.5: C++ shim for the PSController path in BtController.
// Keeps PSController (external C++ library) usage separate from the
// pure C BtController.c implementation.

#include "PSController.h"
#include "esp_log.h"
#include "PinConfig.h"
#include "ConfigController.h"

static PSController *psx = nullptr;

extern "C" {

bool psx_is_connected(void)
{
    return psx != nullptr && psx->isConnected();
}

void psx_get_button_state(int *cross, int *circle, int *triangle, int *square,
                          int *options, int *share, int *up, int *down,
                          int *left, int *right, int *l1, int *l2,
                          int *r1, int *r2)
{
    if (psx == nullptr) return;
    *cross = psx->event.button_down.cross;
    *circle = psx->event.button_down.circle;
    *triangle = psx->event.button_down.triangle;
    *square = psx->event.button_down.square;
    *options = psx->event.button_down.options;
    *share = psx->event.button_down.share;
    *up = psx->event.button_down.up;
    *down = psx->event.button_down.down;
    *left = psx->event.button_down.left;
    *right = psx->event.button_down.right;
    *l1 = psx->event.button_down.l1;
    *l2 = psx->event.button_down.l2;
    *r1 = psx->event.button_down.r1;
    *r2 = psx->event.button_down.r2;
}

void psx_get_analog_sticks(int *lx, int *ly, int *rx, int *ry)
{
    if (psx == nullptr) return;
    *lx = psx->state.analog.stick.lx;
    *ly = psx->state.analog.stick.ly;
    *rx = psx->state.analog.stick.rx;
    *ry = psx->state.analog.stick.ry;
}

void psx_set_player(int player)
{
    if (psx == nullptr) return;
    psx->setPlayer(player);
}

void psx_set_rumble(float intensity, int duration)
{
    if (psx == nullptr) return;
    psx->setRumble(intensity, duration);
}

void psx_disconnect(void)
{
    if (psx != nullptr && psx->isConnected())
    {
        delete psx;
        psx = nullptr;
        log_i("PSX controller disconnected");
    }
}

bool psx_init(const char *mac, int type)
{
    log_i("start psx advertising with mac: %s type:%i", mac, type);
    config_psx_mac_set((char *)mac);
    config_psx_type_set(type);

    // Select the type of wifi controller - either PS3 or PS4
    if (type == 1)
    {
        log_d("Connecting to PS3");
        psx = new PSController(nullptr, PSController::kPS3);
    }
    else if (type == 2)
    {
        log_d("Connecting to PS4");
        psx = new PSController(nullptr, PSController::kPS4);
    }
    bool returnBluetooth = psx->startListening(mac);
    log_d("Return Bluetooth %i", returnBluetooth);
    return returnBluetooth;
}

} // extern "C"

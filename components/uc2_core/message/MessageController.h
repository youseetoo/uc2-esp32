#include <PinConfig.h>
#pragma once
#include "cJSON.h"

namespace MessageController
{
    int act(cJSON * ob);
    cJSON * get(cJSON *  ob);
    void sendMesageSerial(int key, int value);
    void setup();
    void loop(); // Called periodically to check for long-press timeouts
    
    // Start homing with parameters stored in preferences
    void startHomingWithStoredParams(int axis);
    
    // Button event handlers - short press for normal action, long press (5s) for homing
    void triangle_changed_event(int pressed);  // Axis A (short: release hard limit, long: home A)
    void cross_changed_event(int pressed);     // Axis X (short: normal, long: home X)
    void circle_changed_event(int pressed);    // Axis Y (short: normal, long: home Y)
    void square_changed_event(int pressed);    // Axis Z (short: normal, long: home Z)
};

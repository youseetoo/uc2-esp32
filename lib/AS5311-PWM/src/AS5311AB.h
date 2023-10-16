#ifndef AS5311AB_H
#define AS5311AB_H

#include "Arduino.h"
#include "freertos/queue.h"

class AS5311AB
{
public:
    AS5311AB();
    void begin(int pinA, int pinB);
    int getPosition();

private:
    int _pinA;
    int _pinB;
    volatile int _encoderPos;
    QueueHandle_t _encoderQueue;

    void _handleAChange();
    void _handleBChange();
    static void _processEncoderData(void *parameter);
};

#endif

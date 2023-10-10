#include "AS5311AB.h"

volatile int AS5311AB::_encoderPos = 0;
QueueHandle_t AS5311AB::_encoderQueue = nullptr;
int AS5311AB::_pinA = 0;
int AS5311AB::_pinB = 0;

AS5311AB::AS5311AB(int pinA, int pinB)
{
    log_i("STarting encoder on pins %i and %i", pinA, pinB);
    AS5311AB::_pinA = pinA;
    AS5311AB::_pinB = pinB;
}

void AS5311AB::begin()
{
    pinMode(_pinA, INPUT_PULLUP);
    pinMode(_pinB, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(_pinA), _handleAChange, CHANGE);
    attachInterrupt(digitalPinToInterrupt(_pinB), _handleBChange, CHANGE);

    _encoderQueue = xQueueCreate(10, sizeof(int));
    xTaskCreate(_processEncoderData, "ProcessEncoder", 2048, NULL, 2, NULL);
    log_d("Encoder started");
}

int AS5311AB::getPosition()
{
    /*int pos;
    xQueueReceive(_encoderQueue, &pos, 0); // Non-blocking read
    return pos;
    */
    return AS5311AB::_encoderPos;
}

void IRAM_ATTR AS5311AB::_handleAChange()
{
    if (digitalRead(digitalPinToInterrupt(AS5311AB::_pinA)) == digitalRead(digitalPinToInterrupt(AS5311AB::_pinB)))
    {
        AS5311AB::_encoderPos++;
        int ret = 1;
        xQueueSendFromISR(_encoderQueue, &ret, NULL);
    }
    else
    {
        AS5311AB::_encoderPos--;
        int ret = -1;
        xQueueSendFromISR(_encoderQueue, &ret, NULL);
    }
    // now we want to push the value to the queue
    
}

void IRAM_ATTR AS5311AB::_handleBChange()
{
    if (digitalRead(digitalPinToInterrupt(AS5311AB::_pinB)) == digitalRead(digitalPinToInterrupt(AS5311AB::_pinA)))
    {
        AS5311AB::_encoderPos--;
        int ret = -1;
        xQueueSendFromISR(_encoderQueue, &ret, NULL);
    }
    else
    {
        AS5311AB::_encoderPos++;
        int ret = 1;
        xQueueSendFromISR(_encoderQueue, &ret, NULL);
    }

}

void AS5311AB::_processEncoderData(void *parameter)
{
    int position;
    int currentPosition = 0;
    for (;;)
    {
        if (xQueueReceive(_encoderQueue, &position, portMAX_DELAY))
        {
            currentPosition += position;
            // Handle your data here, ensure you are not in an ISR when using certain functions (like Serial).
            Serial.println(currentPosition);
        }
    }
}

#include "AS5311AB.h"


AS5311AB::AS5311AB()
{

}

void AS5311AB::begin(int pinA, int pinB)
{
    log_i("STarting encoder on pins %i and %i", pinA, pinB);
    _pinA = pinA;
    _pinB = pinB;
    _encoderPos = 0;

    pinMode(_pinA, INPUT_PULLUP);
    pinMode(_pinB, INPUT_PULLUP);

attachInterrupt(digitalPinToInterrupt(_pinA), reinterpret_cast<void (*)(void)>(+[](void* instance) {
    static_cast<AS5311AB*>(instance)->_handleAChange();
}), CHANGE);
attachInterrupt(digitalPinToInterrupt(_pinB), reinterpret_cast<void (*)(void)>(+[](void* instance) {
    static_cast<AS5311AB*>(instance)->_handleBChange();
}), CHANGE);

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

void AS5311AB::_handleAChange()
{
    
    int aVal = digitalRead(_pinA);
    int bVal = digitalRead(_pinB);
    if (aVal == HIGH)
    {
        if (bVal == LOW)
        {
            _encoderPos++;
        }
        else
        {
            _encoderPos--;
        }
    }
    else
    {
        if (bVal == HIGH)
        {
            _encoderPos++;
        }
        else
        {
            _encoderPos--;
        }
    }
    
}

void AS5311AB::_handleBChange()
{
    //if(_pinA == nullptr || _pinB == nullptr) return;
    
    int aVal = digitalRead(_pinA);
    int bVal = digitalRead(_pinB);
    
    Serial.println(aVal);
    Serial.println(bVal);
    Serial.println(_encoderPos);
    if (bVal == HIGH)
    {
        if (aVal == HIGH)
        {
            _encoderPos++;
        }
        else
        {
            _encoderPos--;
        }
    }
    else
    {
        if (aVal == LOW)
        {
            _encoderPos++;
        }
        else
        {
            _encoderPos--;
        }
    }
    
}

void AS5311AB::_processEncoderData(void *parameter)
{
    int position;
    int currentPosition = 0;
    for (;;)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS); //FIXME: Not working yet
        /*
        if (xQueueReceive(_encoderQueue, &position, portMAX_DELAY))
        {
            // Need to return this to something outside the queue?
            currentPosition += position;
            //Serial.println(currentPosition);
        }
        */
    }
}

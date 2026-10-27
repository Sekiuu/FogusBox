#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

class Button {
  private:
    uint8_t pin;
    bool lastState;
    bool currentState;
    bool pressedEvent;

  public:
    Button(uint8_t p);
    void begin();
    void update();
    bool isButtonPress();
    bool isButtonPressed();
};

#endif
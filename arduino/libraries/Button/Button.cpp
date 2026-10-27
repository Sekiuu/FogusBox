#include "Button.h"

Button::Button(uint8_t p) : pin(p), lastState(HIGH), currentState(HIGH), pressedEvent(false) {}

void Button::begin() {
  pinMode(pin, INPUT_PULLUP);
  lastState = digitalRead(pin);
  currentState = lastState;
}

void Button::update() {
  lastState = currentState;
  currentState = digitalRead(pin);
  
  if (lastState == HIGH && currentState == LOW) {
    pressedEvent = true;
  } else {
    pressedEvent = false;
  }
}

bool Button::isButtonPress() {
  return (currentState == LOW);
}

bool Button::isButtonPressed() {
  return pressedEvent;
}
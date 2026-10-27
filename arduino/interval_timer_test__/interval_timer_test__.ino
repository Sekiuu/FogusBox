#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "Button.h"
#include "IntervalTimer.h"

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int BUTTON_A_PIN = 2;
const int BUTTON_B_PIN = 4;
const unsigned long TICK_TIME = 100;
const unsigned long INTERVALS[] = { 5000, 10000 };
const int TOTAL_INTERVALS = sizeof(INTERVALS) / sizeof(INTERVALS[0]);

enum TimerState {
  FOCUS,
  BREAK
};

class MyIntervalTimer : public IntervalTimer {
public:
  // Public state variable accessible from outside the class
  TimerState currentTimerState;

  MyIntervalTimer(const unsigned long* intervals, int count)
    : IntervalTimer(intervals, count), currentTimerState(FOCUS) {}

  void onTimerLoop() override {
    // Automatically determine state based on current interval index (Even = Focus, Odd = Break)
    currentTimerState = (getCurrentIntervalIndex() % 2 == 0) ? FOCUS : BREAK;

    // --- Line 0: Display State & Remaining Time ---
    lcd.setCursor(0, 0);
    if (currentTimerState == FOCUS) {
      lcd.print("Focus: ");
    } else {
      lcd.print("Break: ");
    }

    unsigned long remaining = (getCurrentIntervalDuration() - getElapsedInCurrent()) / 1000;
    lcd.print(remaining);
    lcd.print("s         ");  // Spaces to clear old characters

    // --- Line 1: Interval Progress ---
    lcd.setCursor(0, 1);
    lcd.print("Int: ");
    lcd.print(getCurrentIntervalIndex() + 1);
    lcd.print("/");
    lcd.print(TOTAL_INTERVALS);
    lcd.print("     ");
  }

  void onFinishInterval() override {
    //if current index >= last index mean finish timer
    if (isFnishAllInterval()) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Interval Done!");
      delay(800);
    }
  }
};

Button myButtonA(BUTTON_A_PIN);
Button myButtonB(BUTTON_B_PIN);
MyIntervalTimer myTimer(INTERVALS, TOTAL_INTERVALS);

void setup() {
  Serial.begin(115200);
  myButtonA.begin();
  myButtonB.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Timer Ready");
  lcd.setCursor(0, 1);
  lcd.print("Press Btn Start");
}

void loop() {
  myButtonA.update();
  myButtonB.update();
  if (myButtonB.isButtonPressed()) {
    myTimer.next();
    Serial.print("button B is Pressed\n");
  } else if (myButtonA.isButtonPressed()) {
    Serial.print("button A is Pressed\n");
    if (myTimer.isTimerRunning()) {
      myTimer.stop();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Timer Stopped");
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Timer Started");
      myTimer.start();
    }
  }

  myTimer.timerLoop(TICK_TIME);
  delay(TICK_TIME);
}
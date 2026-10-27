#ifndef INTERVAL_TIMER_H
#define INTERVAL_TIMER_H

#include <Arduino.h>

class IntervalTimer {
  private:
    const unsigned long* intervalArray;
    int numIntervals;
    int currentIndex;
    unsigned long currentElapsed;
    bool running;

  public:
    IntervalTimer(const unsigned long* intervals, int count);
    void start();
    void stop();
    void next();
    void reset();
    bool isFnishAllInterval();
    bool isTimerRunning();
    int getCurrentIntervalIndex();
    unsigned long getElapsedInCurrent();
    unsigned long getCurrentIntervalDuration();
    void timerLoop(unsigned long tickDuration);

    virtual void onTimerLoop() {}
    virtual void onFinishInterval() {}
};

#endif
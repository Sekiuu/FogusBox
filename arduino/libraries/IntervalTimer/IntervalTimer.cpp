#include "IntervalTimer.h"
/*
currentElapsed = เวลาที่ผ่านไปในปัจจุบัน.
intervalArray = ระยะเวลาที่ต้องจับของแต่ละช่วงโดยเก็บเป็น Array ex.{10000,20000} = ช่วงแรกจับเวลา 10 วิ , ช่วงสองจับเวลา 20 วิ
numIntervals = index สุดท้ายของ interval Array เอาไว้เช็คว่าจับเวลาถึงช่วงสุดท้ายรียัง
*/
IntervalTimer::IntervalTimer(const unsigned long *intervals, int count)
    : intervalArray(intervals), numIntervals(count), currentIndex(0), currentElapsed(0), running(false) {}

void IntervalTimer::start()
{
  running = true;
}

void IntervalTimer::stop()
{
  running = false;
}

void IntervalTimer::next()
{
  currentIndex++;
  currentElapsed = 0;
  onFinishInterval();

  if (isFnishAllInterval())
  {
    stop();
    currentIndex = 0;
  }
}

void IntervalTimer::reset()
{
  running = false;
  currentIndex = 0;
  currentElapsed = 0;
}
bool IntervalTimer::isFnishAllInterval()
{
  return currentIndex >= numIntervals;
}
bool IntervalTimer::isTimerRunning()
{
  return running;
}

int IntervalTimer::getCurrentIntervalIndex()
{
  return currentIndex;
}

unsigned long IntervalTimer::getElapsedInCurrent()
{
  return currentElapsed;
}

unsigned long IntervalTimer::getCurrentIntervalDuration()
{
  if (currentIndex < numIntervals)
  {
    return intervalArray[currentIndex];
  }
  return 0;
}

void IntervalTimer::timerLoop(unsigned long tickDuration)
{
  if (!running)
    return;

  currentElapsed += tickDuration;
  onTimerLoop();

  if (currentElapsed >= intervalArray[currentIndex])
  {
    next();
  }
}
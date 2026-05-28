#ifndef EINK_POWER_H
#define EINK_POWER_H

#include <Arduino.h>

// Enter deep sleep for specified minutes, with timer wake + GPIO3 button wake
void enterDeepSleep(int minutes);

// Check wakeup reason: true = timer, false = button/other
bool isTimerWakeup();

#endif // EINK_POWER_H

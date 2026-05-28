#ifndef LED_CONTROL_H
#define LED_CONTROL_H

#include "config.h"

void ledControlInit();

// Long-running visible activity control. Calls may overlap: the LED turns on
// when the count goes 0->1 and turns off only after the final matching End.
// Use this for HTTP, e-ink refresh, warning holds, and any future activity that
// must not be interrupted by another module finishing earlier.
void ledActivityBegin();
void ledActivityEnd();

// Emergency cleanup for sleep/restart paths. This intentionally discards the
// activity count because the MCU is leaving the current execution context.
void ledForceOff();

// Short visual hints. If a long-running activity is already active these only
// wait for the blink duration and preserve the steady-on state.
void ledBlink(int ms);
void ledBlinkN(int count, int ms);

// Idle-state LED writes for WiFi connecting and portal animation. They are no-op
// while an activity is active, which prevents idle blinking from fighting HTTP or
// display refresh steady-on behavior.
void ledToggleIdle();
void ledOffIfIdle();
void ledSetIdle(bool on);

class LedActivityGuard {
public:
    LedActivityGuard() { ledActivityBegin(); }
    ~LedActivityGuard() { ledActivityEnd(); }
    LedActivityGuard(const LedActivityGuard &) = delete;
    LedActivityGuard &operator=(const LedActivityGuard &) = delete;
};

using HttpLedGuard = LedActivityGuard;

#endif // LED_CONTROL_H

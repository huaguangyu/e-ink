#ifndef EINK_BUTTON_H
#define EINK_BUTTON_H

#include <Arduino.h>

enum class ButtonEvent {
    NONE,
    SHORT_PRESS,
    LONG_PRESS,
    VERY_LONG_PRESS,
};

// Initialize button GPIO
void buttonInit();

// Poll button state, returns event when detected
ButtonEvent buttonPoll();

// Check if button is currently held during boot (for portal override)
bool buttonHeldAtBoot();

// Ignore button until released (use after entering portal)
void buttonIgnoreUntilRelease();

// Lock button for specified ms (prevent rapid page switching)
void buttonLock(unsigned long ms);

// Check if button is currently locked
bool buttonIsLocked();

#endif // EINK_BUTTON_H

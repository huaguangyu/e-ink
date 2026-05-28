#include "button.h"
#include "config.h"

static unsigned long btnPressStart = 0;
static bool ignoreUntilRelease = false;
static unsigned long lockUntil = 0;

void buttonLock(unsigned long ms) {
    lockUntil = millis() + ms;
}

bool buttonIsLocked() {
    return millis() < lockUntil;
}

void buttonInit() {
    pinMode(PIN_CFG_BTN, INPUT_PULLUP);
}

void buttonIgnoreUntilRelease() {
    ignoreUntilRelease = true;
}

ButtonEvent buttonPoll() {
    if (millis() < lockUntil) return ButtonEvent::NONE;

    bool isPressed = (digitalRead(PIN_CFG_BTN) == LOW);

    if (ignoreUntilRelease) {
        // Used after entering portal from a held key. Without this, the same
        // physical press could immediately be interpreted as a long-press action.
        if (!isPressed) ignoreUntilRelease = false;
        btnPressStart = 0;
        return ButtonEvent::NONE;
    }

    if (isPressed) {
        if (btnPressStart == 0) {
            btnPressStart = millis();
        } else {
            unsigned long holdTime = millis() - btnPressStart;
            if (holdTime >= 8000ULL) {
                // 8 seconds hold -> enter portal mode during runtime (fires instantly)
                btnPressStart = 0;
                ignoreUntilRelease = true;
                return ButtonEvent::VERY_LONG_PRESS;
            } else if (holdTime >= 2000ULL) {
                // 2 seconds hold -> trigger refresh (fires instantly)
                static unsigned long lastFiredPressStart = 0;
                if (lastFiredPressStart != btnPressStart) {
                    lastFiredPressStart = btnPressStart;
                    return ButtonEvent::LONG_PRESS;
                }
            }
        }
    } else {
        if (btnPressStart != 0) {
            unsigned long pressDuration = millis() - btnPressStart;
            btnPressStart = 0;
            if (pressDuration >= (unsigned long)SHORT_PRESS_MIN_MS &&
                pressDuration < 1200ULL) {
                return ButtonEvent::SHORT_PRESS;
            }
        }
    }
    return ButtonEvent::NONE;
}

bool buttonHeldAtBoot() {
    if (digitalRead(PIN_CFG_BTN) == LOW) {
        // Debounce boot-held detection; GPIO3 can be low briefly during wake edge.
        delay(400);
        return (digitalRead(PIN_CFG_BTN) == LOW);
    }
    return false;
}

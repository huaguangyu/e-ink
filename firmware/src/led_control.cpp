#include "led_control.h"

// Reference count for steady-on LED activity. ESP32 Arduino loop is single-core
// for this firmware path, so a simple counter is sufficient here.
static int activeCount = 0;

void ledControlInit() {
    pinMode(PIN_LED, OUTPUT);
    activeCount = 0;
    digitalWrite(PIN_LED, LOW);
}

void ledActivityBegin() {
    if (activeCount++ == 0) digitalWrite(PIN_LED, HIGH);
}

void ledActivityEnd() {
    // Extra End calls are ignored defensively; paired RAII guards should normally
    // keep the counter balanced.
    if (activeCount > 0 && --activeCount == 0) digitalWrite(PIN_LED, LOW);
}

void ledForceOff() {
    activeCount = 0;
    digitalWrite(PIN_LED, LOW);
}

void ledBlink(int ms) {
    if (activeCount > 0) {
        // Preserve the meaning of steady-on: ongoing HTTP/display work takes
        // precedence over decorative blink feedback.
        delay(ms * 2);
        return;
    }
    digitalWrite(PIN_LED, HIGH);
    delay(ms);
    digitalWrite(PIN_LED, LOW);
    delay(ms);
}

void ledBlinkN(int count, int ms) {
    for (int i = 0; i < count; i++) ledBlink(ms);
}

void ledToggleIdle() {
    if (activeCount > 0) {
        digitalWrite(PIN_LED, HIGH);
        return;
    }
    digitalWrite(PIN_LED, !digitalRead(PIN_LED));
}

void ledOffIfIdle() {
    if (activeCount == 0) digitalWrite(PIN_LED, LOW);
}

void ledSetIdle(bool on) {
    if (activeCount == 0) digitalWrite(PIN_LED, on ? HIGH : LOW);
}

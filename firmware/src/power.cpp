#include "power.h"
#include "config.h"
#include "log.h"
#include <esp_sleep.h>

void enterDeepSleep(int minutes) {
    LOGI("Deep sleep for %d min", minutes);
    Serial.flush();

    // Configure both wake sources every time because deep sleep resets the CPU.
    // GPIO3 low wakes the chip for manual interaction; timer keeps automatic quota
    // refreshes running on the configured interval.
    pinMode(PIN_CFG_BTN, INPUT_PULLUP);
    esp_deep_sleep_enable_gpio_wakeup(BIT(PIN_CFG_BTN), ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_sleep_enable_timer_wakeup((uint64_t)minutes * 60ULL * 1000000ULL);
    esp_deep_sleep_start();
}

bool isTimerWakeup() {
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
}

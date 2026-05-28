#ifndef EINK_CONFIG_H
#define EINK_CONFIG_H

#include <Arduino.h>

// ========== Pin definitions ==========
#define PIN_EPD_MOSI   6
#define PIN_EPD_SCK    4
#define PIN_EPD_CS     7
#define PIN_EPD_DC     1
#define PIN_EPD_RST    2
#define PIN_EPD_BUSY   10

#define PIN_BAT_ADC    0
#define PIN_CFG_BTN    3
#define PIN_LED        5

// ========== Display constants ==========
#ifndef EPD_WIDTH
#define EPD_WIDTH  400
#endif
#ifndef EPD_HEIGHT
#define EPD_HEIGHT 300
#endif

static const int W = EPD_WIDTH;
static const int H = EPD_HEIGHT;
static const int ROW_BYTES   = W / 8;
static const int ROW_STRIDE  = (ROW_BYTES + 3) & ~3;
static const int IMG_BUF_LEN = ROW_BYTES * H;

// ========== Remote URLs ==========
#define CONSOLE_TIMEOUT 8000    // ms

// ========== Timing ==========
static const int WIFI_TIMEOUT    = 15000;   // ms
static const int HTTP_TIMEOUT    = 30000;   // ms
static const int CFG_BTN_HOLD_MS = 10000;   // ms
static const int CFG_BTN_SHORT_MAX_MS = 3000; // ms
static const int SHORT_PRESS_MIN_MS = 50;   // ms
static const int DEFAULT_SLEEP_MIN = 5;     // minutes (demo: short interval)
static const int MAX_RETRY_COUNT  = 5;
static const int RETRY_DELAYS[]   = {5, 15, 30, 60, 120}; // seconds

// ========== Timezone ==========
#define NTP_UTC_OFFSET  (8 * 3600)  // UTC+8

// ========== AP name prefix ==========
#define AP_NAME_PREFIX  "Elnk"

// ========== Interactive timeout ==========
#define INTERACTIVE_TIMEOUT 30000  // ms

#endif // EINK_CONFIG_H
 

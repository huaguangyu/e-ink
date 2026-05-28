#include "display_gxepd2.h"

#include <SPI.h>
#include "../config.h"
#include "../led_control.h"
#include "../log.h"

#if defined(DISPLAY_WFT0420CZ15)
static EInkDisplay display(GxEPD2_420c(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
static const DisplayCaps caps = {400, 300, true, false, false, "WFT0420CZ15/GDEW042Z15"};
#elif defined(DISPLAY_GDEY042T81)
static EInkDisplay display(GxEPD2_420_GDEY042T81(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
static const DisplayCaps caps = {400, 300, false, true, true, "GDEY042T81"};
#endif

void displayBegin() {
    SPI.begin(PIN_EPD_SCK, -1, PIN_EPD_MOSI, PIN_EPD_CS);
    display.init(115200, true, 2, false);
    display.setRotation(0);
    display.setTextWrap(false);
    LOGI("Display: %s %dx%d red=%d partial=%d fastPartial=%d",
                  caps.model, caps.width, caps.height,
                  caps.hasRed, caps.supportsPartial, caps.supportsFastPartial);
}

void displayPrepareFull() {
    ledActivityBegin(); // 凡刷新必亮灯，和 HTTP 共用引用计数避免互相抢灯
    display.setFullWindow();
    display.firstPage();
}

void displayPreparePartial(int x, int y, int w, int h) {
    if (!caps.supportsPartial) {
        displayPrepareFull();
        return;
    }
    ledActivityBegin(); // 局刷亦同步亮灯
    display.setPartialWindow(x, y, w, h);
    display.firstPage();
}

bool displayNextPage() {
    bool hasNext = display.nextPage();
    if (!hasNext) {
        ledActivityEnd(); // 绘制传输彻底结束，若无其他活动则自动关闭 LED
    }
    return hasNext;
}

void displayClear(InkColor color) {
    display.fillScreen(displayColor(color));
}

void displayFullRefresh() {
    display.refresh(false);
}

void displayPartialRefresh(int x, int y, int w, int h) {
    if (caps.supportsPartial) display.refresh(x, y, w, h);
    else display.refresh(false);
}

void displaySleep() {
    display.hibernate();
}

uint16_t displayColor(InkColor color) {
    switch (color) {
    case InkColor::Black:
        return GxEPD_BLACK;
    case InkColor::Red:
        return caps.hasRed ? GxEPD_RED : GxEPD_BLACK;
    case InkColor::White:
    default:
        return GxEPD_WHITE;
    }
}

const DisplayCaps &displayCaps() {
    return caps;
}

EInkDisplay &displayCanvas() {
    return display;
}

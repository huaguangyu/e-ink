#ifndef EINK_DISPLAY_GXEPD2_H
#define EINK_DISPLAY_GXEPD2_H

#include <Adafruit_GFX.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>

#include "../config.h"
#include "display.h"

#if defined(DISPLAY_WFT0420CZ15)
using EInkDisplay = GxEPD2_3C<GxEPD2_420c, GxEPD2_420c::HEIGHT>;
#elif defined(DISPLAY_GDEY042T81)
using EInkDisplay = GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT>;
#else
#error "Select a display with DISPLAY_WFT0420CZ15 or DISPLAY_GDEY042T81"
#endif

// Thin GxEPD2 facade used by UI and state-machine code. Keeping all display I/O
// here centralizes SPI pins, panel selection, logical color mapping, and LED
// activity hooks for refresh operations.
void displayBegin();

// Prepare a paged full refresh. Must be followed by drawing in a do/while loop
// that calls displayNextPage() until it returns false.
void displayPrepareFull();

// Prepare a paged partial refresh when the selected panel supports it; otherwise
// safely falls back to full refresh.
void displayPreparePartial(int x, int y, int w, int h);

// Advance the GxEPD2 page loop. Returns true while more pages are required.
bool displayNextPage();
void displayClear(InkColor color = InkColor::White);
void displayFullRefresh();
void displayPartialRefresh(int x, int y, int w, int h);

// Hibernate the panel controller. The e-ink image remains visible with no power.
void displaySleep();
uint16_t displayColor(InkColor color);
const DisplayCaps &displayCaps();
EInkDisplay &displayCanvas();

#endif // EINK_DISPLAY_GXEPD2_H

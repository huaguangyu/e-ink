#ifndef EINK_WIDGETS_GX_H
#define EINK_WIDGETS_GX_H

#include <Arduino.h>
#include <Adafruit_GFX.h>

#include "../display/display.h"

int textWidthGx(Adafruit_GFX &d, const GFXfont *font, const char *text);

// Copy src into dst and append an ellipsis-like suffix when needed so text fits
// within maxW. All drawing code should use this before placing dynamic strings.
void fitTextGx(Adafruit_GFX &d, const GFXfont *font, const char *src,
               char *dst, int dstLen, int maxW);

// Text helpers use topY instead of baseline Y. This keeps page layout readable
// because UI code can align visual boxes without manually applying font bounds.
void drawTextGx(Adafruit_GFX &d, const GFXfont *font, const char *text,
                int x, int topY, uint16_t color);
void drawTextRightGx(Adafruit_GFX &d, const GFXfont *font, const char *text,
                     int rightX, int topY, uint16_t color);
void drawTextCenteredGx(Adafruit_GFX &d, const GFXfont *font, const char *text,
                        int centerX, int topY, int maxW, uint16_t color);

// Mixed date helpers draw only the small supported Chinese date vocabulary from
// cn_12.h and fall back to the ASCII font for numbers/separators.
void drawMixedDateCnGx(Adafruit_GFX &d, const GFXfont *asciiFont, const char *text, int x, int topY, uint16_t color);
int mixedDateWidthGx(Adafruit_GFX &d, const GFXfont *asciiFont, const char *text);

void drawDividerGx(Adafruit_GFX &d, int x, int y, int w, uint16_t color);
void drawProgressBarGx(Adafruit_GFX &d, int x, int y, int w, int h, int pct,
                       bool emphasis, uint16_t fg, uint16_t accent, uint16_t bg);
void drawCardGx(Adafruit_GFX &d, int x, int y, int w, int h, uint16_t color);

#endif // EINK_WIDGETS_GX_H

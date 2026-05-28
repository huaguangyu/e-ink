#include "widgets_gx.h"

#include "../fonts/cn_12.h"

int textWidthGx(Adafruit_GFX &d, const GFXfont *font, const char *text) {
    if (!text || !text[0]) return 0;
    int16_t x1, y1;
    uint16_t w, h;
    d.setFont(font);
    d.getTextBounds((char *)text, 0, 0, &x1, &y1, &w, &h);
    return (int)w;
}

void fitTextGx(Adafruit_GFX &d, const GFXfont *font, const char *src,
               char *dst, int dstLen, int maxW) {
    if (dstLen <= 0) return;
    snprintf(dst, dstLen, "%s", src ? src : "");
    if (textWidthGx(d, font, dst) <= maxW) return;

    int n = strlen(dst);
    while (n > 1) {
        dst[n - 1] = '\0';
        dst[n - 2] = '~';
        if (textWidthGx(d, font, dst) <= maxW) return;
        n--;
    }
    snprintf(dst, dstLen, "~");
}

void drawTextGx(Adafruit_GFX &d, const GFXfont *font, const char *text,
                int x, int topY, uint16_t color) {
    if (!text) return;
    int16_t x1, y1;
    uint16_t w, h;
    d.setFont(font);
    d.setTextColor(color);
    if (!font) {
        d.setCursor(x, topY);
        d.print(text);
        return;
    }
    d.getTextBounds((char *)text, x, topY, &x1, &y1, &w, &h);
    // topY is desired text-top; getTextBounds treats y param as baseline,
    // so y1 = topY + minYOffset. Real baseline = topY - minYOffset = 2*topY - y1.
    d.setCursor(x, topY + (topY - y1));
    d.print(text);
}

void drawTextRightGx(Adafruit_GFX &d, const GFXfont *font, const char *text,
                     int rightX, int topY, uint16_t color) {
    drawTextGx(d, font, text, rightX - textWidthGx(d, font, text), topY, color);
}

void drawTextCenteredGx(Adafruit_GFX &d, const GFXfont *font, const char *text,
                        int centerX, int topY, int maxW, uint16_t color) {
    char fitted[80];
    fitTextGx(d, font, text, fitted, sizeof(fitted), maxW);
    int x = centerX - textWidthGx(d, font, fitted) / 2;
    drawTextGx(d, font, fitted, x, topY, color);
}

static bool nextUtf8Char(const char *s, int &i, char out[4]) {
    if ((uint8_t)s[i] < 0x80) {
        out[0] = s[i++];
        out[1] = 0;
        return false;
    }
    out[0] = s[i++];
    out[1] = s[i++];
    out[2] = s[i++];
    out[3] = 0;
    return true;
}

static int cnIndex(const char *utf8) {
    const char *p = Cn12Chars;
    for (int idx = 0; *p; idx++, p += 3) {
        if (strncmp(p, utf8, 3) == 0) return idx;
    }
    return -1;
}

static int glyphAdvance(const GFXglyph &glyph) {
    return pgm_read_byte(&glyph.xAdvance);
}

static int charAdvanceGFX(const GFXfont *font, char c) {
    if (!font) return 6;
    uint8_t ch = (uint8_t)c;
    if (ch < font->first || ch > font->last) return 0;
    return pgm_read_byte(&font->glyph[ch - font->first].xAdvance);
}

static void drawCnGlyph(Adafruit_GFX &d, int idx, int x, int baseline, uint16_t color) {
    if (idx < 0) return;
    GFXglyph glyph;
    memcpy_P(&glyph, &Cn12Glyphs[idx], sizeof(GFXglyph));
    const uint8_t *bitmap = Cn12Bitmaps + glyph.bitmapOffset;
    int bytesPerRow = (glyph.width + 7) / 8;
    for (int yy = 0; yy < glyph.height; yy++) {
        for (int xx = 0; xx < glyph.width; xx++) {
            uint8_t b = pgm_read_byte(bitmap + yy * bytesPerRow + xx / 8);
            if (b & (0x80 >> (xx & 7))) {
                d.drawPixel(x + glyph.xOffset + xx, baseline + glyph.yOffset + yy, color);
            }
        }
    }
}

int mixedDateWidthGx(Adafruit_GFX &d, const GFXfont *asciiFont, const char *text) {
    int w = 0;
    for (int i = 0; text && text[i]; ) {
        char one[4];
        bool isCn = nextUtf8Char(text, i, one);
        if (isCn) {
            int idx = cnIndex(one);
            if (idx >= 0) w += glyphAdvance(Cn12Glyphs[idx]);
        } else {
            w += charAdvanceGFX(asciiFont, one[0]);
        }
    }
    return w;
}

void drawMixedDateCnGx(Adafruit_GFX &d, const GFXfont *asciiFont,
                       const char *text, int x, int topY, uint16_t color) {
    // cn_12 max ascent = 10px (from glyph yOffsets: -9 to -10)
    // InterRegular11pt max ascent = 9px (digits yOffset: -8 to -9)
    // Use the larger (10) so both fit above the shared baseline.
    int baseline = topY + 10;
    int cx = x;
    for (int i = 0; text && text[i]; ) {
        char one[4];
        bool isCn = nextUtf8Char(text, i, one);
        if (isCn) {
            int idx = cnIndex(one);
            drawCnGlyph(d, idx, cx, baseline, color);
            if (idx >= 0) cx += glyphAdvance(Cn12Glyphs[idx]);
        } else {
            d.setFont(asciiFont);
            d.setTextColor(color);
            d.setCursor(cx, baseline);  // GFXfont cursor Y = baseline
            d.print(one);
            cx = d.getCursorX();  // advance by xAdvance
        }
    }
}

void drawDividerGx(Adafruit_GFX &d, int x, int y, int w, uint16_t color) {
    for (int px = x; px < x + w; px += 5) d.drawFastHLine(px, y, 2, color);
}

void drawProgressBarGx(Adafruit_GFX &d, int x, int y, int w, int h, int pct,
                       bool emphasis, uint16_t fg, uint16_t accent, uint16_t bg) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    d.drawRoundRect(x, y, w, h, 3, fg);
    d.fillRoundRect(x + 2, y + 2, w - 4, h - 4, 2, bg);
    int fillW = (w - 4) * pct / 100;
    if (fillW > 0) d.fillRoundRect(x + 2, y + 2, fillW, h - 4, 2, emphasis ? accent : fg);
}

void drawCardGx(Adafruit_GFX &d, int x, int y, int w, int h, uint16_t color) {
    d.drawRoundRect(x, y, w, h, 4, color);
}

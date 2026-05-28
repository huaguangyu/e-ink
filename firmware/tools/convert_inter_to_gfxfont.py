#!/usr/bin/env python3
"""Convert Inter TTF files to Adafruit_GFX GFXfont headers."""

from __future__ import annotations

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
FONT_DIR = ROOT / "tools" / "fonts" / "Inter"
OUT_DIR = ROOT / "src" / "fonts"
ASCII_FIRST = 0x20
ASCII_LAST = 0x7E
CHARS = [chr(c) for c in range(ASCII_FIRST, ASCII_LAST + 1)]

SPECS = [
    ("InterRegular11pt7b", "inter_regular_11.h", "Inter-18pt-Regular.ttf", 11),
    ("InterRegular14pt7b", "inter_regular_14.h", "Inter-24pt-Regular.ttf", 14),
    ("InterSemiBold18pt7b", "inter_semibold_18.h", "Inter-24pt-SemiBold.ttf", 18),
    ("InterBold28pt7b", "inter_bold_28.h", "Inter-24pt-Bold.ttf", 28),
]


def pack_glyph(font: ImageFont.FreeTypeFont, ch: str) -> tuple[dict, list[int]]:
    x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
    w = max(0, x1 - x0)
    h = max(0, y1 - y0)
    advance = max(1, int(round(font.getlength(ch))))
    if w == 0 or h == 0:
        return {"width": 0, "height": 0, "xAdvance": advance, "xOffset": 0, "yOffset": 0}, []

    img = Image.new("L", (w, h), 0)
    draw = ImageDraw.Draw(img)
    draw.text((-x0, -y0), ch, font=font, fill=255, anchor="ls")

    data: list[int] = []
    pix = img.load()
    # Adafruit_GFX reads GFXfont glyph pixels as one continuous bit stream.
    # Do not pad every row to a byte boundary here, or rows after the first
    # become bit-shifted and text appears smeared on screen.
    byte = 0
    bit_count = 0
    for y in range(h):
        for x in range(w):
            if pix[x, y] >= 96:
                byte |= 0x80 >> bit_count
            bit_count += 1
            if bit_count == 8:
                data.append(byte)
                byte = 0
                bit_count = 0
    if bit_count:
        data.append(byte)

    return {
        "width": w,
        "height": h,
        "xAdvance": advance,
        "xOffset": x0,
        "yOffset": y0,
    }, data


def write_font(symbol: str, out_file: str, font_file: str, size: int) -> int:
    font = ImageFont.truetype(str(FONT_DIR / font_file), size=size)
    ascent, descent = font.getmetrics()
    y_advance = ascent + descent

    bitmap: list[int] = []
    glyphs: list[dict] = []
    for ch in CHARS:
        offset = len(bitmap)
        meta, data = pack_glyph(font, ch)
        meta["bitmapOffset"] = offset
        glyphs.append(meta)
        bitmap.extend(data)

    guard = f"EINK_FONT_{symbol.upper()}_H"
    lines = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        "#include <Arduino.h>",
        "#include <Adafruit_GFX.h>",
        "",
        f"const uint8_t {symbol}Bitmaps[] PROGMEM = {{",
    ]
    for i in range(0, len(bitmap), 16):
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in bitmap[i : i + 16]) + ",")
    lines.extend(["};", "", f"const GFXglyph {symbol}Glyphs[] PROGMEM = {{"])
    for g in glyphs:
        lines.append(
            "    "
            f"{{{g['bitmapOffset']}, {g['width']}, {g['height']}, "
            f"{g['xAdvance']}, {g['xOffset']}, {g['yOffset']}}},"
        )
    lines.extend([
        "};",
        "",
        f"const GFXfont {symbol} PROGMEM = {{",
        f"    (uint8_t *){symbol}Bitmaps,",
        f"    (GFXglyph *){symbol}Glyphs,",
        f"    0x{ASCII_FIRST:02X}, 0x{ASCII_LAST:02X}, {y_advance}",
        "};",
        "",
        f"#endif // {guard}",
        "",
    ])

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (OUT_DIR / out_file).write_text("\n".join(lines), encoding="utf-8")
    return len(bitmap)


def main() -> None:
    total = 0
    for spec in SPECS:
        total += write_font(*spec)
    print(f"Wrote {len(SPECS)} GFXfont headers to {OUT_DIR.relative_to(ROOT)} ({total} bitmap bytes)")


if __name__ == "__main__":
    main()

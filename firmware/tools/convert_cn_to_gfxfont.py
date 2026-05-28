#!/usr/bin/env python3
"""Convert the small Chinese date character set to an Adafruit_GFX font."""

from __future__ import annotations

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
FONT_FILE = ROOT / "tools" / "fonts" / "STHeiti Medium.ttc"
OUT_FILE = ROOT / "src" / "fonts" / "cn_12.h"
CHARS = "周一二三四五六日月凌晨清上午中下晚"
FIRST = 0xE000


def pack_glyph(font: ImageFont.FreeTypeFont, ch: str) -> tuple[dict, list[int]]:
    x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
    w = max(0, x1 - x0)
    h = max(0, y1 - y0)
    advance = max(1, int(round(font.getlength(ch))))
    img = Image.new("L", (w, h), 0)
    ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255, anchor="ls")
    data: list[int] = []
    pix = img.load()
    for y in range(h):
        for bx in range((w + 7) // 8):
            byte = 0
            for bit in range(8):
                x = bx * 8 + bit
                if x < w and pix[x, y] >= 96:
                    byte |= 0x80 >> bit
            data.append(byte)
    return {
        "width": w,
        "height": h,
        "xAdvance": advance,
        "xOffset": x0,
        "yOffset": y0,
    }, data


def main() -> None:
    font = ImageFont.truetype(str(FONT_FILE), size=12)
    ascent, descent = font.getmetrics()
    bitmap: list[int] = []
    glyphs: list[dict] = []
    for ch in CHARS:
        offset = len(bitmap)
        meta, data = pack_glyph(font, ch)
        meta["bitmapOffset"] = offset
        glyphs.append(meta)
        bitmap.extend(data)

    lines = [
        "#ifndef EINK_FONT_CN_12_H",
        "#define EINK_FONT_CN_12_H",
        "",
        "#include <Arduino.h>",
        "#include <Adafruit_GFX.h>",
        "",
        f"static const char *const Cn12Chars = \"{CHARS}\";",
        f"static const uint16_t Cn12First = 0x{FIRST:04X};",
        "",
        "const uint8_t Cn12Bitmaps[] PROGMEM = {",
    ]
    for i in range(0, len(bitmap), 16):
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in bitmap[i : i + 16]) + ",")
    lines.extend(["};", "", "const GFXglyph Cn12Glyphs[] PROGMEM = {"])
    for g in glyphs:
        lines.append(
            "    "
            f"{{{g['bitmapOffset']}, {g['width']}, {g['height']}, "
            f"{g['xAdvance']}, {g['xOffset']}, {g['yOffset']}}},"
        )
    lines.extend([
        "};",
        "",
        "const GFXfont Cn12 PROGMEM = {",
        "    (uint8_t *)Cn12Bitmaps,",
        "    (GFXglyph *)Cn12Glyphs,",
        f"    0x{FIRST:04X}, 0x{FIRST + len(CHARS) - 1:04X}, {ascent + descent}",
        "};",
        "",
        "#endif // EINK_FONT_CN_12_H",
        "",
    ])
    OUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    OUT_FILE.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {OUT_FILE.relative_to(ROOT)}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Render exactly what the TTGO panel will show, without the hardware.

It deliberately mirrors the firmware rather than approximating it:

  * glyphs come from TFT_eSPI's own Fonts/glcdfont.c (font 1 = Adafruit GLCD
    5x7 in 6x8 cells), so the pixels are the real ones;
  * the wrap is the same greedy algorithm as wrapText() in the .ino, using the
    same 6px advance and TEXT_MAX_W budget;
  * the band geometry (bottom-anchored, LINE_H 10, +8 padding, 3px bottom
    margin, accent rule on top) is copied from the constants in the .ino.

Sanity-check the layout here instead of squinting at a 1.14" panel.
"""

import argparse
import os
import re
import sys

from PIL import Image, ImageDraw

# ---- must match esp32-vibe-companion.ino -----------------------------------
SPRITE_W, SPRITE_H = 135, 240
FONT_ID, LINE_H = 1, 10
TEXT_PAD_X = 5
TEXT_MAX_W = SPRITE_W - 2 * TEXT_PAD_X
BAND_BOTTOM = 3
MAX_LINES = 6
ADVANCE = 6          # GLCD: cwidth = 6 (TFT_eSPI.cpp)
CELL_H = 8           # GLCD: cheight = 8
BAND_BG = (16, 16, 16)
BAND_ACCENT = (56, 189, 248)
TEXT_COLOR = (255, 255, 255)

DEFAULT_GLCDFONT = os.path.expanduser("~/Arduino/libraries/TFT_eSPI/Fonts/glcdfont.c")


def load_glcd_font(path):
    """Parse the 256x5 byte column-major glyph table from glcdfont.c."""
    src = open(path).read()
    body = src[src.index("{"):]
    vals = [int(v, 0) for v in re.findall(r"0x[0-9A-Fa-f]{2}", body)]
    if len(vals) < 256 * 5:
        sys.exit(f"glcdfont.c: only found {len(vals)} bytes, expected {256*5}")
    return [vals[i * 5:i * 5 + 5] for i in range(256)]


def draw_glyph(draw, font, ch, x, y, color):
    """GLCD glyph: 5 columns, bit N of each byte = row N. 7 rows used."""
    code = ord(ch)
    if code < 32 or code > 127:
        code = ord("?")
    cols = font[code]
    for cx in range(5):
        bits = cols[cx]
        for ry in range(7):
            if bits & (1 << ry):
                draw.point((x + cx, y + ry), fill=color)


def text_width(s):
    return ADVANCE * len(s)          # exactly what tft.textWidth() returns for font 1


def wrap_text(text, max_w=TEXT_MAX_W, max_lines=MAX_LINES):
    """Identical to wrapText() in the firmware (same greedy rule)."""
    lines, line = [], ""
    for word in text.split():
        candidate = word if not line else f"{line} {word}"
        if text_width(candidate) <= max_w:
            line = candidate
        else:
            if len(lines) < max_lines:
                lines.append(line)
            line = word
    if line and len(lines) < max_lines:
        lines.append(line)
    return lines


def render(sprite_png, text, out_png):
    img = Image.open(sprite_png).convert("RGB").resize((SPRITE_W, SPRITE_H))
    draw = ImageDraw.Draw(img)
    font = load_glcd_font(DEFAULT_GLCDFONT)

    lines = wrap_text(text) if text else []
    band_h = (len(lines) * LINE_H) + 8 if lines else 0
    band_y = SPRITE_H - band_h - BAND_BOTTOM

    if lines:
        draw.rectangle([0, band_y, SPRITE_W - 1, SPRITE_H - BAND_BOTTOM - 1], fill=BAND_BG)
        draw.line([0, band_y, SPRITE_W - 1, band_y], fill=BAND_ACCENT)
        for i, line in enumerate(lines):
            y = band_y + 4 + i * LINE_H
            for j, ch in enumerate(line):
                draw_glyph(draw, font, ch, TEXT_PAD_X + j * ADVANCE, y, TEXT_COLOR)

    img = img.resize((SPRITE_W * 3, SPRITE_H * 3), Image.NEAREST)
    img.save(out_png)
    return lines, band_y, band_h


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(here, ".."))
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(root, "preview", "screen_mockup.png"))
    args = ap.parse_args()

    if not os.path.exists(DEFAULT_GLCDFONT):
        sys.exit(f"glcdfont.c not found at {DEFAULT_GLCDFONT}")

    samples = [
        ("happy", "Hello. What do you need?"),
        ("sleeping", "Good night. I'll idle."),
        ("confident", "The clock reads 3:06pm."),
        ("no_internet", "Connection lost. Retrying every 10 seconds."),
    ]

    print(f"{'MOOD':<12} {'LINES':>5} {'BAND_Y':>6} {'BAND_H':>6}  WRAP")
    for mood, text in samples:
        sprite = os.path.join(root, "preview", f"{mood}.png")
        if not os.path.exists(sprite):
            print(f"  skip {mood}: {sprite} missing (run convert_images.py)")
            continue
        lines, band_y, band_h = render(sprite, text, os.path.join(root, "preview", f"mock_{mood}.png"))
        print(f"{mood:<12} {len(lines):>5} {band_y:>6} {band_h:>6}  {' / '.join(lines)}")

    # contact sheet of the four mockups
    cells = [Image.open(os.path.join(root, "preview", f"mock_{m}.png"))
             for m, _ in samples if os.path.exists(os.path.join(root, "preview", f"mock_{m}.png"))]
    if cells:
        w, h = cells[0].size
        sheet = Image.new("RGB", (w * len(cells), h), (8, 8, 8))
        for i, c in enumerate(cells):
            sheet.paste(c, (i * w, 0))
        sheet.save(args.out)
        print(f"\nwrote {args.out}  ({sheet.size[0]}x{sheet.size[1]}, 3x nearest-neighbour zoom)")

    # longest response in the whole table decides the worst-case band height
    # Only the TEXT field of a rule: { "keyword", MOOD_X, "text" }
    rule_re = re.compile(r'\{\s*(?:nullptr|"[^"]*")\s*,\s*MOOD_\w+\s*,\s*"([^"]*)"\s*\}')
    worst, worst_len = "", 0
    for m in rule_re.finditer(open(os.path.join(root, "responses.h")).read()):
        s = m.group(1)
        if len(s) > worst_len:
            worst, worst_len = s, len(s)
    wl = wrap_text(worst)
    print(f"\nlongest line in responses.h ({worst_len} chars) -> {len(wl)} lines, "
          f"band {len(wl)*LINE_H+8}px at y={SPRITE_H-(len(wl)*LINE_H+8)-BAND_BOTTOM}")
    print(f"  {' / '.join(wl)}")
    print(f"worst-case band top: {SPRITE_H-(len(wl)*LINE_H+8)-BAND_BOTTOM}  "
          f"(face region y<150 untouched: "
          f"{'YES' if SPRITE_H-(len(wl)*LINE_H+8)-BAND_BOTTOM > 150 else 'NO'})")


if __name__ == "__main__":
    main()

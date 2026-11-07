#!/usr/bin/env python3
"""Generate the embedded bitmap fonts used by the PicoCalc FX firmware.

Sources (both licence-compatible with this GPL-3.0 project):
  * Spleen 6x12 (BSD-2-Clause, Frederic Cambus)  -> font::Small
  * GNU Unifont 8x16 (GPL-2.0-or-later)           -> font::Medium

Usage: python3 tools/gen_fonts.py SPLEEN_6x12.bdf UNIFONT.otf > src/font_data.cpp
Glyphs are stored row-major, one byte per row, MSB = leftmost pixel.
"""
import sys

# Code points required by the UI and the math renderer.
def wanted():
    cps = list(range(0x20, 0x7F))
    cps += [0xB0, 0xB1, 0xB2, 0xB3, 0xB5, 0xB7, 0xB9, 0xBC, 0xBD, 0xBE, 0xD7, 0xF7,
            0xE0, 0xE8, 0xE9, 0xEC, 0xF2, 0xF9, 0xC0, 0xC8, 0xC9, 0xAC, 0xAB, 0xBB]
    cps += list(range(0x391, 0x3A2)) + list(range(0x3A3, 0x3AA))      # Greek capitals
    cps += list(range(0x3B1, 0x3CA))                                  # Greek small
    cps += [0x2070, 0x207F, 0x2074, 0x2075, 0x2076, 0x2077, 0x2078, 0x2079, 0x207A, 0x207B,
            0x2080, 0x2081, 0x2082, 0x2083, 0x2084, 0x2085, 0x2086, 0x2087, 0x2088,
            0x2089, 0x2090, 0x2091]                                   # super/subscripts
    cps += [0x2190, 0x2191, 0x2192, 0x2193, 0x2194, 0x21D2, 0x21D4,   # arrows
            0x2202, 0x2206, 0x2207, 0x2208, 0x2211, 0x2212, 0x221A, 0x221B, 0x221C,
            0x221E, 0x2220, 0x2229, 0x222A, 0x222B, 0x2248, 0x2260, 0x2264, 0x2265,
            0x2205, 0x2026, 0x2022, 0x2713, 0x2717, 0x2102, 0x211D, 0x2115, 0x2124,
            0x25A0, 0x25A1, 0x25B2, 0x25B6, 0x25BC, 0x25C0, 0x25CF, 0x25CB, 0x2588,
            0x2592, 0x2500, 0x2502, 0x250C, 0x2510, 0x2514, 0x2518, 0x23CE]
    return sorted(set(cps))

def parse_bdf(path):
    glyphs, cur, bitmap, in_bitmap = {}, None, [], False
    width = height = 0; ascent = 0
    for line in open(path, encoding='latin-1'):
        p = line.split()
        if not p: continue
        if p[0] == 'FONTBOUNDINGBOX': width, height = int(p[1]), int(p[2])
        elif p[0] == 'FONT_ASCENT': ascent = int(p[1])
        elif p[0] == 'ENCODING': cur = int(p[1])
        elif p[0] == 'BITMAP': in_bitmap, bitmap = True, []
        elif p[0] == 'ENDCHAR':
            if cur is not None and cur >= 0:
                glyphs[cur] = [int(r, 16) for r in bitmap]
            in_bitmap = False; cur = None
        elif in_bitmap: bitmap.append(p[0])
    # Spleen 6x12 stores each row as a single hex byte (MSB = leftmost pixel).
    return width, height, ascent, glyphs

def render_unifont(path, cps):
    from PIL import Image, ImageDraw, ImageFont
    f = ImageFont.truetype(path, 16)
    out = {}
    for cp in cps:
        ch = chr(cp)
        if f.getlength(ch) != 8: continue           # keep single-width glyphs only
        img = Image.new('1', (8, 16), 0)
        d = ImageDraw.Draw(img); d.fontmode = '1'
        d.text((0, 0), ch, font=f, fill=1)
        rows = []
        for y in range(16):
            v = 0
            for x in range(8):
                if img.getpixel((x, y)): v |= 0x80 >> x
            rows.append(v)
        if cp != 0x20 and not any(rows): continue    # glyph missing in font
        out[cp] = rows
    return out

def downscale(rows16):
    """Area-resample an 8x16 Unifont glyph to 6x12, mapping the Unifont baseline
    (row 14) onto the Spleen 6x12 baseline (row 9)."""
    out = []
    for ty in range(12):
        y0 = ty * 16 / 12 + 2.0
        y1 = (ty + 1) * 16 / 12 + 2.0
        v = 0
        for tx in range(6):
            x0, x1 = tx * 8 / 6, (tx + 1) * 8 / 6
            cov = area = 0.0
            sy = int(y0)
            while sy < y1:
                hy = min(y1, sy + 1) - max(y0, sy)
                sx = int(x0)
                while sx < x1:
                    hx = min(x1, sx + 1) - max(x0, sx)
                    on = 0 <= sy < 16 and (rows16[sy] >> (7 - sx)) & 1
                    cov += hx * hy * on; area += hx * hy
                    sx += 1
                sy += 1
            if area and cov / area >= 0.34:
                v |= 0x80 >> tx
        out.append(v)
    return out

def emit(name, width, height, baseline, glyphs, cps):
    cps = [c for c in cps if c in glyphs]
    lines = [f'static const std::uint8_t {name}Bits[] = {{']
    for c in cps:
        rows = glyphs[c]
        rows = (rows + [0] * height)[:height]
        lines.append('    ' + ','.join(f'0x{r:02X}' for r in rows) + f',  // U+{c:04X}')
    lines.append('};')
    lines.append(f'static const std::uint32_t {name}Codepoints[] = {{')
    for i in range(0, len(cps), 10):
        lines.append('    ' + ','.join(f'0x{c:04X}' for c in cps[i:i + 10]) + ',')
    lines.append('};')
    lines.append(f'const BitmapFont {name}{{{width},{height},{baseline},{name}Codepoints,{name}Bits,{len(cps)}}};')
    return '\n'.join(lines), cps

def main():
    bdf, otf = sys.argv[1], sys.argv[2]
    cps = wanted()
    w, h, ascent, small = parse_bdf(bdf)
    # Spleen 6x12 rows are 8-bit wide already (one hex byte per row).
    medium = render_unifont(otf, cps)
    # Fill symbols missing from Spleen 6x12 with resampled Unifont glyphs.
    for c in cps:
        if c not in small and c in medium and c > 0x7E:
            small[c] = downscale(medium[c])
    s_src, s_cps = emit('kSmall', w, h, ascent, small, cps)
    m_src, m_cps = emit('kMedium', 8, 16, 14, medium, cps)
    print('// Generated by tools/gen_fonts.py - do not edit by hand.')
    print('// Small: Spleen 6x12 (BSD-2-Clause, (c) Frederic Cambus); symbols missing from Spleen')
    print('//        are area-resampled from GNU Unifont.')
    print('// Medium: GNU Unifont 8x16 (GPL-2.0-or-later, (c) Roman Czyborra, Paul Hardy et al.).')
    print('#include "font_renderer.hpp"')
    print('#include <cstdint>')
    print('namespace font {')
    print(s_src); print(m_src)
    print('} // namespace font')
    missing_s = [hex(c) for c in cps if c not in s_cps]
    missing_m = [hex(c) for c in cps if c not in m_cps]
    print(f'small: {len(s_cps)} glyphs, missing {len(missing_s)}', file=sys.stderr)
    print(f'medium: {len(m_cps)} glyphs, missing {missing_m}', file=sys.stderr)

if __name__ == '__main__':
    main()

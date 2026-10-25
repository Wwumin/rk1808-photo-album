# -*- coding: utf-8 -*-
"""
从 Windows 字体生成 8x16 ASCII 点阵字库 (C 数组)。
字符集: ASCII 32..126, 共 95 个。
每个字符 16 字节，每字节代表一行，bit7 在最左边 —— 与主流 8x16 字库一致，
可直接用于嵌入式点阵绘制。同时导出 2 倍放大预览图用于肉眼校验。
"""

import os
from PIL import Image, ImageDraw, ImageFont

OUT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = r"C:\Windows\Fonts\simhei.ttf"   # 黑体，笔画粗，小字号可读性好
FIRST, LAST = 32, 126
GW, GH = 8, 16


def build_table():
    font = ImageFont.truetype(FONT, 15)
    table = []
    cell_preview = Image.new("RGB", (GW * (LAST - FIRST + 1), GH), (0, 0, 0))
    dp = ImageDraw.Draw(cell_preview)

    for ci, code in enumerate(range(FIRST, LAST + 1)):
        ch = chr(code)
        # 大画布上画，再裁切定位，避免基线偏差
        canvas = Image.new("L", (24, 24), 0)
        d = ImageDraw.Draw(canvas)
        d.text((2, 1), ch, font=font, fill=255)
        bbox = canvas.getbbox()
        glyph = Image.new("L", (GW, GH), 0)
        if bbox:
            gw, gh = bbox[2] - bbox[0], bbox[3] - bbox[1]
            crop = canvas.crop(bbox)
            # 水平不缩放(直接贴)，垂直略压到 12 像素内以适配 16 行
            if gh > 13:
                crop = crop.resize((max(1, round(gw * 13.0 / gh)), 13), Image.LANCZOS)
                gw, gh = crop.size
            ox = max(0, min(GW - gw, (GW - gw) // 2))
            oy = max(0, min(GH - gh, (GH - gh) // 2 + 1))
            glyph.paste(crop, (ox, oy))
        px = glyph.load()
        rows = []
        for y in range(GH):
            b = 0
            for x in range(GW):
                if px[x, y] > 110:
                    b |= 1 << (7 - x)
            rows.append(b)
        table.append(rows)
        for y in range(GH):
            for x in range(GW):
                if rows[y] & (1 << (7 - x)):
                    dp.point((ci * GW + x, y), (0, 255, 120))
    return table, cell_preview


def main():
    table, preview = build_table()
    preview.resize((preview.width * 3, preview.height * 3), Image.NEAREST).save(
        os.path.join(OUT_DIR, "_preview_font.png"))

    lines = []
    lines.append("/*")
    lines.append(" * font8x16.c  —  8x16 ASCII 点阵字库 (ASCII 32..126, 共 95 字)")
    lines.append(" *")
    lines.append(" * 由 tools/gen_font.py 自动生成，请勿手工修改。")
    lines.append(" * 每字符 16 字节；第 i 字节为该行位图，bit7 为最左像素。")
    lines.append(" * 用法见 draw.c 的 draw_char() / draw_text()。")
    lines.append(" */")
    lines.append("")
    lines.append('#include "font8x16.h"')
    lines.append("")
    lines.append("const unsigned char font8x16[95][16] = {")
    for ci, code in enumerate(range(FIRST, LAST + 1)):
        rows = table[ci]
        name = chr(code)
        if name == "\\":
            name = "backslash"
        elif name == "'":
            name = "quote"
        elif name == " ":
            name = "space"
        comment = "  /* '%s' */" % name
        vals = ",".join("0x%02X" % r for r in rows)
        lines.append("    {%s},%s" % (vals, comment))
    lines.append("};")
    lines.append("")

    dst = os.path.join(OUT_DIR, "album", "font8x16.c")
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    with open(dst, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print("写出 %s  (%d 字节)" % (dst, os.path.getsize(dst)))


if __name__ == "__main__":
    main()

# -*- coding: utf-8 -*-
"""
BMP/PNG -> RGB565 C 数组 (备用)

什么时候用得上:
    你现在的方案是 DRM 直显 + BMP 文件，不需要这个脚本。
    但如果后面要换成 LVGL / ST7789 / SPI 屏这类"图片烧进 flash"的方案，
    或者面试/答辩时被问"图片能做成 C 数组吗"，用这个一键转换。

用法:
    python bmp2c.py <图片> [更多图片...] --out images.c [--name album] [--swap]

示例:
    python bmp2c.py ../bmp/main_menu.bmp ../bmp/album_menu.bmp --out ui_images.c
    python bmp2c.py ../bmp/photo_*.bmp --out photos.c --swap

参数:
    --out    输出 .c 文件
    --name   数组名前缀，默认用文件名
    --swap   输出大端序 RGB565 (某些屏/驱动需要)
    --r5g6b5 用 RGB565；加 --rgb888 可改成 24bit 每像素
"""

import os
import sys
from PIL import Image

try:
    from PIL import Image
except ImportError:
    print("需要 Pillow:  pip install pillow")
    sys.exit(1)


def to_rgb565_bytes(im, swap):
    px = im.convert("RGB").load()
    w, h = im.size
    out = bytearray()
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            if swap:
                out.append((v >> 8) & 0xFF)
                out.append(v & 0xFF)
            else:
                out.append(v & 0xFF)
                out.append((v >> 8) & 0xFF)
    return bytes(out), w, h


def to_rgb888_bytes(im):
    return im.convert("RGB").tobytes(), im.size[0], im.size[1]


def sanitize(name):
    base = os.path.splitext(os.path.basename(name))[0]
    return "".join(ch if ch.isalnum() else "_" for ch in base)


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        sys.exit(1)

    out_path = "images.c"
    prefix = None
    swap = False
    rgb888 = False
    files = []

    i = 0
    while i < len(args):
        a = args[i]
        if a == "--out":
            i += 1
            out_path = args[i]
        elif a == "--name":
            i += 1
            prefix = args[i]
        elif a == "--swap":
            swap = True
        elif a == "--rgb888":
            rgb888 = True
        elif a.startswith("--"):
            print("未知参数: %s" % a)
            sys.exit(1)
        else:
            files.append(a)
        i += 1

    if not files:
        print("没有输入图片")
        sys.exit(1)

    lines = []
    lines.append("/*")
    lines.append(" * %s  —  由 tools/bmp2c.c 脚本自动生成，请勿手工修改" % os.path.basename(out_path))
    lines.append(" *")
    lines.append(" * 像素格式: %s%s" % ("RGB888 (3 字节/像素)" if rgb888 else "RGB565 (2 字节/像素)",
                                       " 大端序" if (swap and not rgb888) else " 小端序"))
    lines.append(" * 绘制方式: 按行连续存放，行之间无填充(pitch = width * %d)" % (3 if rgb888 else 2))
    lines.append(" */")
    lines.append("")
    lines.append('#include "images.h"')
    lines.append("")

    decls = []
    defs = []

    for path in files:
        if not os.path.isfile(path):
            print("跳过(不存在): %s" % path)
            continue
        with Image.open(path) as im:
            if rgb888:
                data, w, h = to_rgb888_bytes(im)
            else:
                data, w, h = to_rgb565_bytes(im, swap)
        name = sanitize(path) if not prefix else "%s_%s" % (prefix, sanitize(path))
        print("  %-28s %4dx%-4d %8.1f KB -> %s" % (os.path.basename(path), w, h, len(data) / 1024.0, name))
        decls.append("extern const ImageRes %s;" % name)
        defs.append("const ImageRes %s = { %d, %d, %d, %s_data };" % (name, w, h, len(data), name))
        defs.append("")
        defs.append("static const unsigned char %s_data[] = {" % name)
        per_line = 12
        for off in range(0, len(data), per_line):
            chunk = data[off:off + per_line]
            defs.append("    " + " ".join("0x%02X," % c for c in chunk))
        defs.append("};")
        defs.append("")

    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
        f.write("/* ===== 声明 ===== */\n")
        f.write("\n".join(decls))
        f.write("\n\n/* ===== 数据 ===== */\n")
        f.write("\n".join(defs))

    print("\n写出 %s (%.1f KB)" % (out_path, os.path.getsize(out_path) / 1024.0))

    hdr = os.path.splitext(out_path)[0] + ".h"
    if not os.path.exists(hdr):
        with open(hdr, "w", encoding="utf-8", newline="\n") as f:
            f.write("/* 由 tools/bmp2c.py 生成 */\n\n")
            f.write("#ifndef ALBUM_IMAGES_H\n#define ALBUM_IMAGES_H\n\n")
            f.write("typedef struct {\n")
            f.write("    unsigned short width;\n")
            f.write("    unsigned short height;\n")
            f.write("    unsigned int   bytes;\n")
            f.write("    const unsigned char *data;\n")
            f.write("} ImageRes;\n\n")
            f.write("#endif\n")
        print("顺手生成了头文件 %s" % hdr)


if __name__ == "__main__":
    main()

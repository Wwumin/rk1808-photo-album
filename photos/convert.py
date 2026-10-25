# -*- coding: utf-8 -*-
"""
把自己的照片批量转成板子能读的 24bit 未压缩 BMP。

用法:
    python convert.py <照片目录> [输出目录] [宽] [高]

示例:
    python convert.py D:\\我的照片 ..\\bmp
    python convert.py D:\\我的照片 ..\\bmp 1024 600

规则:
    - 等比缩放到"填满"目标尺寸，再从中心裁切(不变形)
    - 输出文件名 photo_01.bmp, photo_02.bmp ... (补零两位，排序才正确)
    - 保存为标准 BITMAPINFOHEADER / 24bit / 不压缩 / 自下而上 BMP
    - 输出目录里原有的 photo_*.bmp 会被覆盖同名文件，不会动别的文件
"""

import os
import struct
import sys

try:
    from PIL import Image, ImageOps
except ImportError:
    print("需要 Pillow:  pip install pillow")
    sys.exit(1)

EXTS = (".jpg", ".jpeg", ".png", ".bmp", ".webp", ".tif", ".tiff", ".gif")
TARGET_W, TARGET_H = 1024, 600


def fit_cover(im, tw, th):
    """等比缩放到刚好覆盖 tw x th，再中心裁切。"""
    im = ImageOps.exif_transpose(im).convert("RGB")
    sw, sh = im.size
    scale = max(tw / sw, th / sh)
    nw, nh = max(1, int(round(sw * scale))), max(1, int(round(sh * scale)))
    im = im.resize((nw, nh), Image.LANCZOS)
    left = (nw - tw) // 2
    top = (nh - th) // 2
    return im.crop((left, top, left + tw, top + th))


def verify_bmp(path):
    with open(path, "rb") as f:
        hd = f.read(54)
    ok = (hd[:2] == b"BM"
          and struct.unpack("<I", hd[14:18])[0] == 40
          and struct.unpack("<H", hd[26:28])[0] == 1
          and struct.unpack("<H", hd[28:30])[0] == 24
          and struct.unpack("<I", hd[30:34])[0] == 0)
    return ok


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else "."
    tw = int(sys.argv[3]) if len(sys.argv) > 3 else TARGET_W
    th = int(sys.argv[4]) if len(sys.argv) > 4 else TARGET_H

    if not os.path.isdir(src):
        print("目录不存在: %s" % src)
        sys.exit(1)
    os.makedirs(dst, exist_ok=True)

    files = sorted(f for f in os.listdir(src) if f.lower().endswith(EXTS))
    if not files:
        print("%s 里没找到图片" % src)
        sys.exit(1)

    print("源目录 : %s" % src)
    print("输出   : %s" % os.path.abspath(dst))
    print("目标尺寸: %d x %d" % (tw, th))
    print("共 %d 张\n" % len(files))

    for i, name in enumerate(files):
        out = os.path.join(dst, "photo_%02d.bmp" % (i + 1))
        with Image.open(os.path.join(src, name)) as im:
            fit_cover(im, tw, th).save(out, "BMP")
        size_mb = os.path.getsize(out) / 1048576.0
        flag = "OK " if verify_bmp(out) else "BAD"
        print("  [%s] %-28s -> %-16s %5.2f MB" % (flag, name[:28], os.path.basename(out), size_mb))

    print("\n完成。把这些 photo_*.bmp 和 main_menu.bmp / album_menu.bmp 放同一个目录，"
          "\n程序启动时就是 ./main <那个目录>")


if __name__ == "__main__":
    main()

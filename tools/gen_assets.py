# -*- coding: utf-8 -*-
"""
电子相册素材生成器  (嵌入式 Linux / DRM 直显专用)

目标平台参数 (依据 D:\\Users\\1\\Desktop\\share\\DRM库2 的示例代码确认):
    分辨率 : 1024 x 600
    像素格式: XRGB8888, 小端内存序 => 每像素 4 字节 [B][G][R][X]
    图片格式: 24bit 未压缩 BMP (BITMAPINFOHEADER, 自下而上)
              —— 与 share 里 showbmp() 的读取方式完全对应

输出:
    bmp/main_menu.bmp     主界面(含 相册/游戏/多媒体/其他/退出/选项)
    bmp/album_menu.bmp    相册二级界面(含 手动/自动/退出/选项)
    bmp/photo_XX.bmp      10 张示例图(供手动翻页 / 自动播放测试)
    bmp/_banner_export.bmp 逐像素真彩色测试图

运行:  python gen_assets.py
"""

import os
import struct
from PIL import Image, ImageDraw, ImageFont, ImageFilter

# ---------------------------------------------------------------- 基本参数
W, H = 1024, 600
OUT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BMP_DIR = os.path.join(OUT_ROOT, "bmp")

FONT_BOLD = r"C:\Windows\Fonts\msyhbd.ttc"   # 微软雅黑 Bold
FONT_UI = r"C:\Windows\Fonts\msyh.ttc"       # 微软雅黑
FONT_HEI = r"C:\Windows\Fonts\simhei.ttf"    # 黑体
FONT_NUM = r"C:\Windows\Fonts\Deng.ttf"      # 等线(数字好看)


def F(path, size):
    return ImageFont.truetype(path, size)


# ---------------------------------------------------------------- 绘制工具
def text_center(d, cx, cy, s, font, fill, shadow=None, off=3):
    """以 (cx,cy) 为中心绘字，可选阴影/描边以提升在照片上的可读性。"""
    box = d.textbbox((0, 0), s, font=font)
    w, h = box[2] - box[0], box[3] - box[1]
    x, y = cx - w / 2 - box[0], cy - h / 2 - box[1]
    if shadow:
        for dx, dy in ((-off, 0), (off, 0), (0, -off), (0, off), (off, off), (-off, -off), (off, -off), (-off, off)):
            d.text((x + dx, y + dy), s, font=font, fill=shadow)
    d.text((x, y), s, font=font, fill=fill)


def vgrad(size, top, bottom):
    """竖向线性渐变。"""
    w, h = size
    img = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        d.line([(0, y), (w, y)], fill=c)
    return img


def save_bmp(img, path):
    """保存为标准 24bit BGR 未压缩 BMP(自下而上)，并校验头部。"""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.convert("RGB").save(path, "BMP")
    with open(path, "rb") as f:
        hd = f.read(54)
    assert hd[:2] == b"BM", path
    off = struct.unpack("<I", hd[10:14])[0]
    hsz = struct.unpack("<I", hd[14:18])[0]
    w = struct.unpack("<i", hd[18:22])[0]
    h = struct.unpack("<i", hd[22:26])[0]
    planes, bpp = struct.unpack("<HH", hd[26:30])
    comp = struct.unpack("<I", hd[30:34])[0]
    assert hsz == 40 and planes == 1 and bpp == 24 and comp == 0, (hsz, planes, bpp, comp)
    assert (w, h) == (W, H), (w, h)
    assert off == 54, off
    print("  [OK] %-28s %8.2f MB  %dx%d %dbpp 偏移%d" %
          (os.path.basename(path), os.path.getsize(path) / 1048576.0, w, h, bpp, off))


# ---------------------------------------------------------------- 界面一：主界面
def draw_main_menu():
    img = vgrad((W, H), (22, 34, 72), (6, 10, 24))
    d = ImageDraw.Draw(img, "RGBA")

    # 顶部装饰光带
    for y in range(96):
        d.line([(0, y), (W, y)], fill=(60, 110, 220, int(46 * (1 - y / 96.0))))
    d.line([(0, 96), (W, 96)], fill=(90, 150, 255))

    text_center(d, W // 2, 50, "电 子 相 册", F(FONT_BOLD, 46), (240, 246, 255), shadow=(0, 0, 0))
    text_center(d, W // 2, 92, "ELECTRONIC  PHOTO  ALBUM", F(FONT_NUM, 15), (140, 180, 255))
    text_center(d, W - 60, 40, "RK1808  DRM", F(FONT_NUM, 14), (110, 140, 200))

    # 2 行 3 列表格
    cw, ch = 296, 190
    gx, gy = 40, 34
    x0 = (W - (3 * cw + 2 * gx)) // 2
    y0 = 132

    items = [
        ("相册", "PHOTO ALBUM", (74, 144, 226), "[1]"),
        ("游戏", "GAME", (232, 138, 46), "[2]"),
        ("多媒体", "MULTIMEDIA", (84, 190, 130), "[3]"),
        ("其他", "OTHERS", (150, 108, 214), "[4]"),
        ("退出", "EXIT", (226, 88, 92), "[5]"),
        ("选项", "OPTION", (108, 122, 148), "[6]"),
    ]
    rects = []
    for i, (cn, en, col, key) in enumerate(items):
        r, c = divmod(i, 3)
        bx = x0 + c * (cw + gx)
        by = y0 + r * (ch + gy)
        # 卡片底
        d.rounded_rectangle([bx, by, bx + cw, by + ch], radius=18, fill=(28, 38, 62), outline=col, width=3)
        # 左侧色条
        d.rounded_rectangle([bx + 14, by + 18, bx + 24, by + ch - 18], radius=5, fill=col)
        text_center(d, bx + cw // 2 + 6, by + 74, cn, F(FONT_BOLD, 58), (245, 250, 255), shadow=(0, 0, 0))
        text_center(d, bx + cw // 2 + 6, by + 128, en, F(FONT_NUM, 17), (150, 175, 210))
        text_center(d, bx + cw - 30, by + 26, key, F(FONT_NUM, 16), col)
        rects.append((bx, by, bx + cw, by + ch, cn))

    text_center(d, W // 2, H - 26, "触摸方框选择   ·   相册 / 游戏 / 多媒体 / 其他 / 退出 / 选项",
                F(FONT_UI, 16), (120, 145, 185))
    return img, rects


# ---------------------------------------------------------------- 界面二：相册二级
def draw_album_menu():
    img = vgrad((W, H), (18, 46, 60), (5, 12, 20))
    d = ImageDraw.Draw(img, "RGBA")

    for y in range(96):
        d.line([(0, y), (W, y)], fill=(46, 170, 190, int(46 * (1 - y / 96.0))))
    d.line([(0, 96), (W, 96)], fill=(80, 210, 225))

    text_center(d, W // 2, 46, "相  册", F(FONT_BOLD, 44), (238, 252, 255), shadow=(0, 0, 0))
    text_center(d, W // 2, 88, "PHOTO  ALBUM   ·   二级界面", F(FONT_NUM, 15), (140, 225, 235))
    text_center(d, 92, 46, "< 返回", F(FONT_UI, 22), (150, 200, 215))

    cw, ch = 452, 190
    gx, gy = 44, 30
    x0 = (W - (2 * cw + gx)) // 2
    y0 = 138

    items = [
        ("手动", "MANUAL", (80, 190, 255), "触摸上下翻页"),
        ("自动", "AUTO", (86, 214, 150), "每 3 秒自动切换"),
        ("退出", "EXIT", (232, 96, 100), "返回主界面"),
        ("选项", "OPTION", (110, 126, 156), "播放设置"),
    ]
    rects = []
    for i, (cn, en, col, tip) in enumerate(items):
        r, c = divmod(i, 2)
        bx = x0 + c * (cw + gx)
        by = y0 + r * (ch + gy)
        d.rounded_rectangle([bx, by, bx + cw, by + ch], radius=18, fill=(24, 44, 56), outline=col, width=3)
        d.rounded_rectangle([bx + 14, by + 18, bx + 22, by + ch - 18], radius=4, fill=col)
        text_center(d, bx + cw // 2 + 8, by + 76, cn, F(FONT_BOLD, 56), (245, 253, 255), shadow=(0, 0, 0))
        text_center(d, bx + cw // 2 + 8, by + 128, en, F(FONT_NUM, 17), (150, 190, 205))
        text_center(d, bx + cw // 2 + 8, by + 160, tip, F(FONT_UI, 16), (120, 155, 175))
        rects.append((bx, by, bx + cw, by + ch, cn))
    return img, rects


# ---------------------------------------------------------------- 示例图
PALETTES = [
    ((255, 94, 98), (255, 195, 113)),
    ((33, 147, 176), (109, 213, 237)),
    ((142, 45, 226), (74, 0, 173)),
    ((17, 153, 142), (56, 239, 125)),
    ((255, 128, 8), (255, 200, 55)),
    ((44, 62, 80), (76, 161, 175)),
    ((255, 61, 129), (255, 138, 198)),
    ((10, 61, 98), (0, 168, 204)),
    ((200, 45, 20), (250, 160, 60)),
    ((40, 90, 40), (150, 210, 80)),
]


def draw_photo(i):
    """生成第 i 张示例图: 渐变色块 + 序号 + 可辨识的几何图形。

    注意: 刻意不在图上写任何"状态文字"(比如 自动播放中)，
    因为程序会在底部再画一条 HUD 信息栏，烧在图里必然打架。
    """
    top, bot = PALETTES[i % len(PALETTES)]
    img = vgrad((W, H), top, bot)
    d = ImageDraw.Draw(img, "RGBA")

    # 右侧大圆环
    cx, cy, R = 830, 300, 210
    for k, r in enumerate((R, R - 34, R - 68)):
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(255, 255, 255, 70 - k * 12), width=4)
    # 左下三角
    d.polygon([(60, H - 60), (300, H - 60), (60, H - 300)], fill=(255, 255, 255, 46))
    # 斜纹
    for x in range(-H, W, 46):
        d.line([(x, H), (x + H, 0)], fill=(255, 255, 255, 18), width=2)

    # 左下角信息板(H-60 处结束，把最底下 60px 让给程序画的 HUD 信息栏)
    d.rounded_rectangle([46, H - 220, 560, H - 60], radius=16, fill=(0, 0, 0, 118))
    d.text((76, H - 206), "PHOTO SAMPLE", font=F(FONT_NUM, 20), fill=(205, 228, 245))
    d.text((76, H - 176), "%02d / %02d" % (i + 1, 10), font=F(FONT_NUM, 48), fill=(255, 255, 255))
    d.text((76, H - 114), "电子相册示例图", font=F(FONT_UI, 21), fill=(195, 218, 238))
    d.text((76, H - 84), "album/photo_%02d.bmp" % (i + 1), font=F(FONT_NUM, 16), fill=(170, 200, 220))

    # 右上角色块，逐张不同 → 方便肉眼确认没有重复/串图
    d.rounded_rectangle([W - 250, 30, W - 40, 100], radius=12, fill=(0, 0, 0, 100))
    for k in range(4):
        d.rectangle([W - 236 + k * 52, 46, W - 236 + k * 52 + 40, 84], fill=(top[0] + k * 12, bot[1] - k * 10, 200))

    return img


# ---------------------------------------------------------------- 逐像素测试图
def draw_banner():
    img = Image.new("RGB", (W, H))
    px = img.load()
    for y in range(H):
        for x in range(W):
            px[x, y] = ((x * 255) // (W - 1), (y * 255) // (H - 1), ((x + y) * 255) // (W + H - 2))
    d = ImageDraw.Draw(img, "RGBA")
    d.rectangle([W // 2 - 300, H // 2 - 60, W // 2 + 300, H // 2 + 60], fill=(0, 0, 0, 190))
    text_center(d, W // 2, H // 2, "色彩校准图  1024x600", F(FONT_BOLD, 40), (255, 255, 255))
    return img


# ---------------------------------------------------------------- main
def main():
    print("输出目录: %s" % BMP_DIR)
    print("生成界面 ...")
    main_img, main_rects = draw_main_menu()
    save_bmp(main_img, os.path.join(BMP_DIR, "main_menu.bmp"))
    alb_img, alb_rects = draw_album_menu()
    save_bmp(alb_img, os.path.join(BMP_DIR, "album_menu.bmp"))

    print("生成示例图 ...")
    for i in range(10):
        save_bmp(draw_photo(i), os.path.join(BMP_DIR, "photo_%02d.bmp" % (i + 1)))

    print("生成色彩校准图 ...")
    save_bmp(draw_banner(), os.path.join(BMP_DIR, "_banner_export.bmp"))

    print("\n主界面按钮矩形(供触摸判定用):")
    for (x1, y1, x2, y2, name) in main_rects:
        print("  %-4s x1=%4d y1=%4d x2=%4d y2=%4d" % (name, x1, y1, x2, y2))
    print("相册二级按钮矩形:")
    for (x1, y1, x2, y2, name) in alb_rects:
        print("  %-4s x1=%4d y1=%4d x2=%4d y2=%4d" % (name, x1, y1, x2, y2))


if __name__ == "__main__":
    main()

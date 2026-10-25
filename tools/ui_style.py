# -*- coding: utf-8 -*-
"""
ui_style.py  —  统一卡片样式库 (电子相册全部界面共用)

背景:
    生成器脚本有 4 个(gen_assets / gen_buttons / gen_extra / make_landscapes)，
    如果各写一份画卡片的代码，改个圆角/配色就要改四处，迟早不一致。
    所以把"卡片长什么样"集中到这里，其他脚本只提供内容(文字/配色/提示)。

卡片规格(和最早那版主界面完全一致，不能变):
    主界面   : 3 列 x 2 行，卡片 296x190，间距 40/34，左上角起点 (28, 132)
    二级界面 : 2 列 x 2 行，卡片 452x190，间距 44/30，左上角起点 (38, 138)
    卡片    : 圆角 18，填充 (28,38,62)/(24,44,56)，描边 3px 用主题色
              左侧 10px 圆角色条 + 中文大字 + 英文小字 (+ 可选提示行)
"""

import os
from PIL import Image, ImageDraw, ImageFont

from fontkit import pick as _pick_font

W, H = 1024, 600

# 跨平台 CJK 字体：Windows 用微软雅黑/黑体，CI 的 Linux 用 Noto Sans CJK，
# 都找不到时返回 None，下面的 F() 会退回 Pillow 内置点阵字体（只够冒烟测试）。
FONT_BOLD = _pick_font("bold")
FONT_UI = _pick_font("ui")
FONT_HEI = _pick_font("hei")
FONT_NUM = _pick_font("num")

# 两套主题：主界面偏蓝紫，相册二级偏青
THEME_MAIN = {
    "bg_top": (22, 34, 72), "bg_bottom": (6, 10, 24),
    "glow": (60, 110, 220), "line": (90, 150, 255),
    "title": (240, 246, 255), "subtitle": (140, 180, 255),
    "card_fill": (28, 38, 62), "cn": (245, 250, 255),
    "en": (150, 175, 210), "tip": (120, 145, 185),
}
THEME_ALBUM = {
    "bg_top": (18, 46, 60), "bg_bottom": (5, 12, 20),
    "glow": (46, 170, 190), "line": (80, 210, 225),
    "title": (238, 252, 255), "subtitle": (140, 225, 235),
    "card_fill": (24, 44, 56), "cn": (245, 253, 255),
    "en": (150, 190, 205), "tip": (120, 155, 175),
}
THEME_LOCK = {
    "bg_top": (34, 24, 62), "bg_bottom": (8, 6, 18),
    "glow": (150, 90, 220), "line": (190, 140, 255),
    "title": (246, 240, 255), "subtitle": (186, 156, 240),
    "card_fill": (32, 26, 52), "cn": (246, 240, 255),
    "en": (176, 156, 210), "tip": (150, 130, 190),
}
THEME_GAME = {
    "bg_top": (52, 30, 16), "bg_bottom": (14, 8, 5),
    "glow": (226, 130, 40), "line": (255, 176, 80),
    "title": (255, 246, 236), "subtitle": (250, 186, 120),
    "card_fill": (46, 32, 22), "cn": (255, 248, 240),
    "en": (216, 176, 140), "tip": (196, 154, 116),
}


def F(path, size):
    return ImageFont.truetype(path, size)


# ------------------------------------------------------------------ 绘制工具
def text_center(d, cx, cy, s, font, fill, shadow=None, off=3):
    box = d.textbbox((0, 0), s, font=font)
    w, h = box[2] - box[0], box[3] - box[1]
    x, y = cx - w / 2 - box[0], cy - h / 2 - box[1]
    if shadow:
        for dx, dy in ((-off, 0), (off, 0), (0, -off), (0, off),
                       (off, off), (-off, -off), (off, -off), (-off, off)):
            d.text((x + dx, y + dy), s, font=font, fill=shadow)
    d.text((x, y), s, font=font, fill=fill)


def vgrad(size, top, bottom):
    w, h = size
    img = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        d.line([(0, y), (w, y)], fill=c)
    return img


def page_bg(theme, title=None, subtitle=None, back_label=None):
    """一个界面页的背景(含顶部光带/标题/返回)。"""
    img = vgrad((W, H), theme["bg_top"], theme["bg_bottom"])
    d = ImageDraw.Draw(img, "RGBA")
    for y in range(96):
        d.line([(0, y), (W, y)], fill=theme["glow"] + (int(46 * (1 - y / 96.0)),))
    d.line([(0, 96), (W, 96)], fill=theme["line"])
    if title:
        text_center(d, W // 2, 46, title, F(FONT_BOLD, 44), theme["title"], shadow=(0, 0, 0))
    if subtitle:
        text_center(d, W // 2, 88, subtitle, F(FONT_NUM, 15), theme["subtitle"])
    if back_label:
        text_center(d, 92, 46, back_label, F(FONT_UI, 22), theme["subtitle"])
    return img


def card(size, theme, cn, en, color, key=None, tip=None, tip_font_size=16,
         cn_size=58, en_size=17, radius=18, bar_w=10, bar_x=14,
         cy_cn=None, cy_en=None):
    """
    画一张卡片，返回 RGBA 图(圆角外是透明的)。
    这是所有按钮图片的唯一来源 —— 背景图和独立按钮图都用它，保证像素级一致。

    cn_size/en_size/radius/bar_w 是给"小键"(密码键盘、扫雷雷区按钮)用的，
    大卡片全部走默认值，保证和最早那版主界面像素级一致。
    """
    cw, ch = size
    img = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")

    d.rounded_rectangle([0, 0, cw - 1, ch - 1], radius=radius,
                        fill=theme["card_fill"] + (255,),
                        outline=color + (255,), width=3)
    inset = max(8, int(round(ch * 0.095)))
    d.rounded_rectangle([bar_x, inset, bar_x + bar_w, ch - inset],
                        radius=5 if bar_w >= 8 else 3, fill=color + (255,))

    # 中文行 / 英文行的纵向位置。默认比例是给 190 高的大卡片定的；
    # 高度只有 64~66 的小键(游戏 chrome、相册退出键)按默认比例会把中英挤在一起，
    # 所以开放 cy_cn / cy_en 让调用方按实际高度微调。
    r_cn = 0.42 if not en else 0.39
    if cy_cn is not None:
        r_cn = cy_cn
    text_center(d, cw // 2 + 6, ch * r_cn, cn, F(FONT_BOLD, cn_size),
                theme["cn"], shadow=(0, 0, 0))
    if en:
        r_en = 0.67 if cy_en is None else cy_en
        text_center(d, cw // 2 + 6, ch * r_en, en, F(FONT_NUM, en_size), theme["en"])
    if tip:
        text_center(d, cw // 2 + 6, ch * 0.84, tip, F(FONT_UI, tip_font_size), theme["tip"])
    if key:
        text_center(d, cw - 30, 26, key, F(FONT_NUM, 16), color)
    return img


def pressed(card_img, color, scale=0.94):
    """
    按下态: 整体压暗 + 描边加粗，并稍微缩小(物理按键的"下陷"感)。
    返回和原图同样大小的 RGBA，方便直接覆盖绘制。
    """
    w, h = card_img.size
    dark = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    dark.paste(card_img, (0, 0))
    # 压暗: 把 RGB 乘 0.62
    r, g, b, a = dark.split()
    from PIL import ImageEnhance
    rgb = Image.merge("RGB", (r, g, b))
    rgb = ImageEnhance.Brightness(rgb).enhance(scale)
    dark = Image.merge("RGBA", (*rgb.split(), a))

    d = ImageDraw.Draw(dark, "RGBA")
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius=18, outline=(255, 255, 255, 235), width=4)
    return dark


def save_bmp(img, path, expect=(W, H)):
    """不透明图(背景/照片)存 24bit，体积小、和 share 里的 showbmp 兼容。"""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if expect and img.size != expect:
        raise AssertionError("%s 尺寸 %s != %s" % (path, img.size, expect))
    img.convert("RGB").save(path, "BMP")
    print("  [OK] %-30s %6.2f MB  %dx%d" %
          (os.path.basename(path), os.path.getsize(path) / 1048576.0,
           img.size[0], img.size[1]))


def save_bmp32(img, path, expect=None):
    """
    带 alpha 的 32bit BMP，圆角外的透明像素能保留下来。

    为什么必须自己写文件头:
        按钮/牌面/棋子都是"圆角卡片"，圆角外是透明的。PIL 存 BMP 只会写
        24bit 并把透明像素变成黑色 —— 贴到界面上就是四个黑角。
        这里手写 BITMAPINFOHEADER(40) + BI_RGB + 32bpp，每像素 BGRA，
        和 C 端 image_load_bmp 的"直读 4 字节"完全对齐。
    """
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if expect and img.size != expect:
        raise AssertionError("%s 尺寸 %s != %s" % (path, img.size, expect))

    img = img.convert("RGBA")
    w, h = img.size
    stride = w * 4
    pix = bytearray(stride * h)

    raw = img.tobytes()                    # 每像素 R,G,B,A，自上而下
    for y in range(h):
        src = y * stride
        dst = (h - 1 - y) * stride         # BMP 像素自上而下翻转
        row = bytearray(raw[src:src + stride])
        row[0::4] = raw[src + 2:src + stride:4]   # B <- R
        row[2::4] = raw[src:src + stride:4]       # R <- B
        pix[dst:dst + stride] = row

    size = 54 + len(pix)
    head = bytearray(54)
    head[0:2] = b"BM"
    head[2:6] = (size).to_bytes(4, "little")
    head[10:14] = (54).to_bytes(4, "little")
    head[14:18] = (40).to_bytes(4, "little")
    head[18:22] = w.to_bytes(4, "little")
    head[22:26] = h.to_bytes(4, "little")
    head[26:28] = (1).to_bytes(2, "little")
    head[28:30] = (32).to_bytes(2, "little")
    head[34:38] = len(pix).to_bytes(4, "little")

    with open(path, "wb") as f:
        f.write(head)
        f.write(pix)
    print("  [OK] %-30s %6.2f MB  %dx%d 32bit" %
          (os.path.basename(path), os.path.getsize(path) / 1048576.0, w, h))


def save_png(img, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, "PNG")

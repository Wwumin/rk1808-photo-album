# -*- coding: utf-8 -*-
"""
gen_extra.py  —  生成扩展素材

产出:
    btn_opt_<键>[_p].bmp    选项页按钮(播放间隔 1/3/5/10 秒 + 返回)
    tile_<值>.bmp           2048 牌面 91x91 (值: 0 空,2,4,8,...,2048) + 4096 用金色
    tile_bg.bmp             2048 棋盘底板 446x446
    no_photo.bmp            无照片占位提示 480x220 (带透明圆角)

运行:  python gen_extra.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PIL import Image, ImageDraw  # noqa: E402

import ui_style as S  # noqa: E402

BMP_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bmp")

# ---- 2048 配色 (经典 2048 的配色，深色底上对比度好) ----
TILE_COLORS = {
    0:    ((58, 62, 76), (44, 48, 60), (150, 158, 175)),
    2:    ((238, 232, 220), (216, 208, 194), (110, 104, 92)),
    4:    ((238, 224, 196), (216, 198, 162), (110, 100, 80)),
    8:    ((246, 166, 92),  (226, 138, 62),  (255, 255, 255)),
    16:   ((246, 138, 74),  (226, 110, 48),  (255, 255, 255)),
    32:   ((246, 106, 88),  (226, 80, 62),   (255, 255, 255)),
    64:   ((238, 74, 74),   (214, 52, 52),   (255, 255, 255)),
    128:  ((240, 206, 96),  (222, 182, 64),  (72, 56, 22)),
    256:  ((242, 200, 78),  (224, 174, 46),  (72, 56, 22)),
    512:  ((244, 192, 60),  (226, 166, 32),  (72, 56, 22)),
    1024: ((246, 184, 44),  (228, 158, 20),  (72, 56, 22)),
    2048: ((248, 176, 32),  (230, 148, 10),  (255, 255, 255)),
    4096: ((104, 92, 220),  (78, 66, 196),   (255, 255, 255)),
}

TILE_SIZE = 91
TILE_GAP = 16
BOARD_N = 4
BOARD = BOARD_N * TILE_SIZE + (BOARD_N + 1) * TILE_GAP      # 446


def make_tile(value):
    """单张牌面: 圆角矩形 + 垂直微渐变(用两段填充模拟) + 数字。"""
    top, bot, fg = TILE_COLORS.get(value, TILE_COLORS[4096])
    img = Image.new("RGBA", (TILE_SIZE, TILE_SIZE), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")

    d.rounded_rectangle([0, 0, TILE_SIZE - 1, TILE_SIZE - 1], radius=10,
                        fill=bot + (255,))
    # 上半部分再叠一层亮色，做出轻微立体感
    d.rounded_rectangle([0, 0, TILE_SIZE - 1, TILE_SIZE // 2], radius=10,
                        fill=top + (150,))

    if value:
        txt = str(value)
        size = 40 if value < 100 else (34 if value < 1000 else 27)
        S.text_center(d, TILE_SIZE // 2, TILE_SIZE // 2, txt,
                      S.F(S.FONT_BOLD, size), fg)
    return img


def gen_tiles():
    for value in (0, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096):
        img = make_tile(value)
        S.save_bmp32(img, os.path.join(BMP_DIR, "tile_%d.bmp" % value),
                   expect=(TILE_SIZE, TILE_SIZE))

    # 棋盘底板
    board = Image.new("RGBA", (BOARD, BOARD), (0, 0, 0, 0))
    d = ImageDraw.Draw(board, "RGBA")
    d.rounded_rectangle([0, 0, BOARD - 1, BOARD - 1], radius=16, fill=(38, 42, 54, 255))
    for r in range(BOARD_N):
        for c in range(BOARD_N):
            x = TILE_GAP + c * (TILE_SIZE + TILE_GAP)
            y = TILE_GAP + r * (TILE_SIZE + TILE_GAP)
            d.rounded_rectangle([x, y, x + TILE_SIZE - 1, y + TILE_SIZE - 1],
                                radius=10, fill=(50, 55, 70, 255))
    S.save_bmp32(board, os.path.join(BMP_DIR, "tile_bg.bmp"), expect=(BOARD, BOARD))


def gen_options():
    """选项页: 手动模式的播放间隔 + 返回。"""
    theme = S.THEME_MAIN
    bg = S.page_bg(theme, title="选  项", subtitle="OPTION   ·   播放设置", back_label="< 返回")
    d = ImageDraw.Draw(bg, "RGBA")
    S.text_center(d, S.W // 2, 126, "自动播放间隔", S.F(S.FONT_UI, 20), theme["subtitle"])
    S.text_center(d, S.W // 2, S.H - 40, "当前生效值显示在按钮下方", S.F(S.FONT_UI, 16), theme["tip"])
    S.save_bmp(bg, os.path.join(BMP_DIR, "option_menu.bg.bmp"))

    items = [
        ("1s", "1 秒", "1 SECOND",  (110, 190, 255)),
        ("3s", "3 秒", "3 SECONDS", (86, 214, 150)),
        ("5s", "5 秒", "5 SECONDS", (246, 196, 90)),
        ("10s", "10 秒", "10 SECONDS", (240, 140, 100)),
    ]
    size = (220, 150)
    for key, cn, en, col in items:
        img = S.card(size, theme, cn, en, col)
        S.save_bmp32(img, os.path.join(BMP_DIR, "btn_opt_%s.bmp" % key), expect=size)
        S.save_bmp32(S.pressed(img, col), os.path.join(BMP_DIR, "btn_opt_%s_p.bmp" % key),
                   expect=size)


def gen_placeholder():
    """无照片时的占位提示框(圆角外透明，直接叠在背景上)。"""
    w, h = 480, 220
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img, "RGBA")
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius=20, fill=(18, 20, 30, 226),
                        outline=(240, 170, 90, 255), width=3)
    S.text_center(d, w // 2, 74, "NO  PHOTO", S.F(S.FONT_BOLD, 40), (250, 200, 130))
    S.text_center(d, w // 2, 128, "bmp 目录里没有 photo_*.bmp", S.F(S.FONT_UI, 19), (200, 190, 180))
    S.text_center(d, w // 2, 164, "请放入 24bit 未压缩 BMP 后重启", S.F(S.FONT_UI, 17), (160, 155, 160))
    S.save_bmp32(img, os.path.join(BMP_DIR, "no_photo.bmp"), expect=(w, h))


def main():
    print("输出目录: %s" % os.path.abspath(BMP_DIR))
    print("\n[1/3] 2048 牌面与棋盘")
    gen_tiles()
    print("\n[2/3] 选项页")
    gen_options()
    print("\n[3/3] 无照片占位框")
    gen_placeholder()
    print("\n布局参考: 棋盘 %dx%d, 牌面 %dx%d, 间距 %d" % (BOARD, BOARD, TILE_SIZE, TILE_SIZE, TILE_GAP))


if __name__ == "__main__":
    main()

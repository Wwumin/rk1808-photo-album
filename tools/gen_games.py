# -*- coding: utf-8 -*-
"""
gen_games.py  —  六个小游戏的界面素材(统一主题，全部由代码生成)

产出(bmp/):
    ttt_bg.bmp / ms_bg.bmp / xq_bg.bmp / tetris_bg.bmp / snake_bg.bmp
        每个游戏一页背景: 顶部标题条 + 棋盘框/面板
    g_back[_p].bmp      返回     150x66 <统一放在左下>
    g_again[_p].bmp     重来     150x66 <统一放在右下>
    g_extra[_p].bmp     插旗模式 150x66 <扫雷专用，居中>
    g_left/rot/right[_p].bmp  86x66  <俄罗斯方块>
    g_drop[_p].bmp      120x66
    ttt_x.bmp / ttt_o.bmp       124x124  井字棋棋子
    ms_closed/open/flag/mine.bmp 41x41    扫雷格子
    xq_r_<字>.bmp / xq_b_<字>.bmp  40x40  象棋棋子(红 7 / 黑 7)
    tet_<I O T S Z J L>.bmp      22x22   俄罗斯方块方块
    snake_head/body/food.bmp     30x30   贪吃蛇

坐标常量必须和 album/games.h 里的一致。
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PIL import Image, ImageDraw  # noqa: E402

import ui_style as S  # noqa: E402

T = S.THEME_GAME
BMP = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bmp")

# ---------------------------------------------------------------- 几何(和 C 一致)
CHROME_BTN = (150, 66)
BACK_POS = (26, 514)
AGAIN_POS = (848, 514)
EXTRA_POS = (437, 514)

TTT_CELL, TTT_GAP, TTT_X0, TTT_Y0 = 124, 10, 316, 116
MS_CELL, MS_GAP, MS_X0, MS_Y0 = 41, 3, 315, 114
XQ_CELL, XQ_X0, XQ_Y0 = 40, 352, 124
TET_CELL, TET_COLS, TET_ROWS, TET_X0, TET_Y0 = 20, 10, 20, 258, 100
SNK_CELL, SNK_COLS, SNK_ROWS, SNK_X0, SNK_Y0 = 30, 20, 15, 212, 108

TET_DIR_POS = [(250, 514, 86), (346, 514, 86), (442, 514, 86), (538, 514, 120)]

# 2048: 棋盘 4x4，格子 91 + 间距 16 = 444；和 album/main.c 的 BOARD_ORIGIN_X/Y 一致
B2048_ORIGIN = (290, 130)
B2048_BOARD = 444
B2048_INFO_X = 758

XQ_PIECES = ["帅", "仕", "相", "马", "车", "炮", "兵"]
XQ_KIND = ["shuai", "shi", "xiang", "ma", "ju", "pao", "bing"]
XQ_BLACK = ["将", "士", "象", "马", "车", "炮", "卒"]
XQ_BKIND = ["jiang", "shi", "xiang", "ma", "ju", "pao", "zu"]

RED = (222, 64, 70)
BLACK_PIECE = (48, 48, 60)


def header(d, title, subtitle):
    for y in range(96):
        d.line([(0, y), (S.W, y)], fill=T["glow"] + (int(46 * (1 - y / 96.0)),))
    d.line([(0, 96), (S.W, 96)], fill=T["line"])
    S.text_center(d, S.W // 2, 46, title, S.F(S.FONT_BOLD, 40), T["title"], shadow=(0, 0, 0))
    S.text_center(d, S.W // 2, 88, subtitle, S.F(S.FONT_NUM, 15), T["subtitle"])
    S.text_center(d, 92, 46, "< 返回", S.F(S.FONT_UI, 22), T["subtitle"])


def panel(d, box, radius=14, fill=(18, 12, 8, 235), outline=True):
    d.rounded_rectangle(box, radius=radius, fill=fill,
                        outline=T["line"] + (200,) if outline else None, width=2)


# ---------------------------------------------------------------- 背景
def gen_2048_bg():
    """
    2048 的背景。

    2048 原来是自己画的一整套深灰界面，和另外五个游戏的橙色主题放一起很突兀。
    这里把静态部分全部烘进背景图（标题、棋盘底、SCORE/BEST/UNDO 面板、chrome 按钮），
    C 端只负责贴方块和写数字，六个游戏看起来才是一套东西。
    """
    img = S.vgrad((S.W, S.H), T["bg_top"], T["bg_bottom"]).convert("RGB")
    d = ImageDraw.Draw(img, "RGBA")
    header(d, "2048", "2048  ·  滑动合并，凑出 2048")

    bx, by = B2048_ORIGIN
    bz = B2048_BOARD
    panel(d, [bx - 8, by - 8, bx + bz + 8, by + bz + 8], radius=18,
          fill=(26, 18, 12, 245))
    # 空格底
    for r in range(4):
        for c in range(4):
            x = bx + 16 + c * (91 + 16)
            y = by + 16 + r * (91 + 16)
            d.rounded_rectangle([x, y, x + 90, y + 90], radius=10,
                                fill=(44, 32, 24, 255))

    # 右侧信息区
    ix = B2048_INFO_X
    panel(d, [ix, by, ix + 236, by + 108], radius=14)
    S.text_center(d, ix + 118, by + 22, "SCORE", S.F(S.FONT_NUM, 18), T["subtitle"])
    panel(d, [ix, by + 124, ix + 236, by + 232], radius=14)
    S.text_center(d, ix + 118, by + 146, "BEST", S.F(S.FONT_NUM, 18), T["subtitle"])
    panel(d, [ix, by + 248, ix + 236, by + 340], radius=14)
    S.text_center(d, ix + 118, by + 270, "UNDO (TAP)", S.F(S.FONT_NUM, 16), T["subtitle"])

    # chrome 按钮直接烘在背景上（和另外五个游戏同位置同造型）
    for key, pos, label, en, col in (
            ("g_back", BACK_POS, "返回", "BACK", (226, 88, 92)),
            ("g_again", AGAIN_POS, "重来", "RESTART", (86, 214, 150))):
        btn = S.card((150, 66), T, label, en, col, None, None,
                     cn_size=30, en_size=13, radius=12, bar_w=5, bar_x=8)
        img.paste(btn, pos, btn)

    S.save_bmp(img, os.path.join(BMP, "g2048_bg.bmp"), expect=(S.W, S.H))
    return img, (0, 0)


def gen_ttt_bg():
    img = S.vgrad((S.W, S.H), T["bg_top"], T["bg_bottom"]).convert("RGB")
    d = ImageDraw.Draw(img, "RGBA")
    header(d, "井 字 棋", "TIC-TAC-TOE  ·  你执 X 先手")
    bw = 3 * TTT_CELL + 2 * TTT_GAP
    panel(d, [TTT_X0 - 18, TTT_Y0 - 18, TTT_X0 + bw + 18, TTT_Y0 + bw + 18])
    for i in range(2):
        x = TTT_X0 + (i + 1) * TTT_CELL + i * TTT_GAP + TTT_GAP // 2
        y = TTT_Y0 + (i + 1) * TTT_CELL + i * TTT_GAP + TTT_GAP // 2
        d.line([(x, TTT_Y0 - 6), (x, TTT_Y0 + bw + 6)], fill=T["line"] + (150,), width=3)
        d.line([(TTT_X0 - 6, y), (TTT_X0 + bw + 6, y)], fill=T["line"] + (150,), width=3)
    S.save_bmp(img, os.path.join(BMP, "ttt_bg.bmp"))


def gen_ms_bg():
    img = S.vgrad((S.W, S.H), T["bg_top"], T["bg_bottom"]).convert("RGB")
    d = ImageDraw.Draw(img, "RGBA")
    header(d, "扫  雷", "MINESWEEPER  ·  9x9 / 10 颗雷")
    bw = 9 * MS_CELL + 8 * MS_GAP
    panel(d, [MS_X0 - 16, MS_Y0 - 16, MS_X0 + bw + 16, MS_Y0 + bw + 16])
    S.save_bmp(img, os.path.join(BMP, "ms_bg.bmp"))


def gen_xq_bg():
    img = S.vgrad((S.W, S.H), T["bg_top"], T["bg_bottom"]).convert("RGB")
    d = ImageDraw.Draw(img, "RGBA")
    header(d, "象  棋", "XIANGQI  ·  红方(你)先行")
    bw = 8 * XQ_CELL
    bh = 9 * XQ_CELL
    panel(d, [XQ_X0 - 26, XQ_Y0 - 26, XQ_X0 + bw + 26, XQ_Y0 + bh + 26],
          fill=(30, 20, 12, 240))
    # 内线
    for c in range(9):
        x = XQ_X0 + c * XQ_CELL
        d.line([(x, XQ_Y0), (x, XQ_Y0 + bh)], fill=(120, 92, 60, 255), width=2)
    for r in range(10):
        y = XQ_Y0 + r * XQ_CELL
        if r in (0, 9):
            d.line([(XQ_X0, y), (XQ_X0 + bw, y)], fill=(120, 92, 60, 255), width=2)
        else:
            # 中间七列(河界处断开)
            if r == 4 or r == 5:
                for x0, x1 in ((XQ_X0, XQ_X0 + 3 * XQ_CELL),
                               (XQ_X0 + 5 * XQ_CELL, XQ_X0 + bw)):
                    d.line([(x0, y), (x1, y)], fill=(120, 92, 60, 255), width=2)
            else:
                d.line([(XQ_X0, y), (XQ_X0 + bw, y)], fill=(120, 92, 60, 255), width=2)
    # 九宫斜线
    for (x0, y0) in ((3, 0), (3, 7)):
        d.line([(XQ_X0 + x0 * XQ_CELL, XQ_Y0 + y0 * XQ_CELL),
                (XQ_X0 + (x0 + 2) * XQ_CELL, XQ_Y0 + (y0 + 2) * XQ_CELL)],
               fill=(120, 92, 60, 200), width=2)
        d.line([(XQ_X0 + (x0 + 2) * XQ_CELL, XQ_Y0 + y0 * XQ_CELL),
                (XQ_X0 + x0 * XQ_CELL, XQ_Y0 + (y0 + 2) * XQ_CELL)],
               fill=(120, 92, 60, 200), width=2)
    # 河界文字
    S.text_center(d, S.W // 2, XQ_Y0 + 4.5 * XQ_CELL, "楚 河          汉 界",
                  S.F(S.FONT_UI, 18), (150, 118, 78))
    S.save_bmp(img, os.path.join(BMP, "xq_bg.bmp"))


def gen_tetris_bg():
    img = S.vgrad((S.W, S.H), T["bg_top"], T["bg_bottom"]).convert("RGB")
    d = ImageDraw.Draw(img, "RGBA")
    header(d, "俄罗斯方块", "TETRIS  ·  底部按钮操作")
    bw = TET_COLS * TET_CELL
    bh = TET_ROWS * TET_CELL
    panel(d, [TET_X0 - 10, TET_Y0 - 10, TET_X0 + bw + 10, TET_Y0 + bh + 10],
          fill=(10, 8, 6, 200))
    # 井字格底纹
    for c in range(TET_COLS + 1):
        x = TET_X0 + c * TET_CELL
        d.line([(x, TET_Y0), (x, TET_Y0 + bh)], fill=(60, 46, 32, 160), width=1)
    for r in range(TET_ROWS + 1):
        y = TET_Y0 + r * TET_CELL
        d.line([(TET_X0, y), (TET_X0 + bw, y)], fill=(60, 46, 32, 160), width=1)
    # 右侧信息面板
    panel(d, [700, 118, 990, 360], fill=(18, 12, 8, 220))
    S.text_center(d, 845, 150, "SCORE", S.F(S.FONT_NUM, 16), T["en"])
    S.text_center(d, 845, 250, "NEXT", S.F(S.FONT_NUM, 16), T["en"])
    S.save_bmp(img, os.path.join(BMP, "tetris_bg.bmp"))


def gen_snake_bg():
    img = S.vgrad((S.W, S.H), T["bg_top"], T["bg_bottom"]).convert("RGB")
    d = ImageDraw.Draw(img, "RGBA")
    header(d, "贪 吃 蛇", "SNAKE  ·  点哪边往哪边拐")
    bw = SNK_COLS * SNK_CELL
    bh = SNK_ROWS * SNK_CELL
    panel(d, [SNK_X0 - 10, SNK_Y0 - 10, SNK_X0 + bw + 10, SNK_Y0 + bh + 10],
          fill=(10, 16, 10, 220))
    for c in range(SNK_COLS + 1):
        x = SNK_X0 + c * SNK_CELL
        d.line([(x, SNK_Y0), (x, SNK_Y0 + bh)], fill=(50, 70, 50, 120), width=1)
    for r in range(SNK_ROWS + 1):
        y = SNK_Y0 + r * SNK_CELL
        d.line([(SNK_X0, y), (SNK_X0 + bw, y)], fill=(50, 70, 50, 120), width=1)
    S.save_bmp(img, os.path.join(BMP, "snake_bg.bmp"))


# ---------------------------------------------------------------- 按钮
def gen_chrome_buttons():
    mk = [("g_back", "返回", "BACK", (226, 88, 92), CHROME_BTN, 30),
          ("g_again", "重来", "RESTART", (86, 214, 150), CHROME_BTN, 30),
          ("g_extra", "插旗", "FLAG MODE", (150, 108, 214), CHROME_BTN, 28),
          ("g_left", "左", "", (120, 160, 220), (86, 66), 34),
          ("g_rot", "转", "", (232, 186, 80), (86, 66), 34),
          ("g_right", "右", "", (120, 160, 220), (86, 66), 34),
          ("g_drop", "落到底", "", (226, 96, 100), (120, 66), 26)]
    for (key, cn, en, col, size, cn_size) in mk:
        img = S.card(size, T, cn, en, col, cn_size=cn_size,
                     radius=12 if size[1] <= 66 else 18,
                     bar_w=6 if size[1] <= 66 else 10,
                     bar_x=10 if size[1] <= 66 else 14)
        S.save_bmp32(img, os.path.join(BMP, "btn_%s.bmp" % key), expect=size)
        S.save_bmp32(S.pressed(img, col, scale=0.88),
                     os.path.join(BMP, "btn_%s_p.bmp" % key), expect=size)

    # 相册播放页常驻的"退出"键。
    # 高度只有 64，字号必须比游戏页那批(66 高/30 号字)再压一档，
    # 否则"退出"和下面的 EXIT 会挤在一起(实测重叠约 3px)。
    pe_size = (180, 64)
    pe_col = (226, 88, 92)
    pe = S.card(pe_size, T, "退出", "EXIT", pe_col,
                cn_size=26, en_size=12, radius=12, bar_w=6, bar_x=10,
                cy_cn=0.32, cy_en=0.75)
    S.save_bmp32(pe, os.path.join(BMP, "btn_photo_exit.bmp"), expect=pe_size)
    S.save_bmp32(S.pressed(pe, pe_col, scale=0.88),
                 os.path.join(BMP, "btn_photo_exit_p.bmp"), expect=pe_size)


# ---------------------------------------------------------------- 棋子/方块
def gen_ttt_pieces():
    n = TTT_CELL
    for mark, col in (("x", (240, 240, 255)), ("o", (255, 186, 120))):
        img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
        d = ImageDraw.Draw(img, "RGBA")
        m = 26
        if mark == "x":
            for off in range(-4, 5):
                d.line([(m + off, m), (n - m + off, n - m)], fill=col + (255,), width=10)
                d.line([(n - m + off, m), (m + off, n - m)], fill=col + (255,), width=10)
        else:
            for r in range(n // 2 - m, n // 2 - m + 11):
                d.ellipse([n // 2 - r, n // 2 - r, n // 2 + r, n // 2 + r],
                          outline=col + (255,), width=2)
        S.save_bmp32(img, os.path.join(BMP, "ttt_%s.bmp" % mark), expect=(n, n))


def gen_ms_tiles():
    n = MS_CELL
    for kind, fill, outline in (
            ("closed", (78, 58, 40), (140, 104, 70)),
            ("open", (24, 18, 12), (58, 44, 32)),
            ("flag", (78, 58, 40), (140, 104, 70)),
            ("mine", (24, 18, 12), (58, 44, 32))):
        img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
        d = ImageDraw.Draw(img, "RGBA")
        d.rounded_rectangle([0, 0, n - 1, n - 1], radius=6, fill=fill + (255,),
                            outline=outline + (255,), width=2)
        if kind == "flag":
            d.line([(n // 2, 8), (n // 2, n - 9)], fill=(240, 240, 240, 255), width=2)
            d.polygon([(n // 2, 8), (n - 12, 15), (n // 2, 22)], fill=(226, 88, 92, 255))
            d.line([(6, n - 9), (n - 6, n - 9)], fill=(240, 240, 240, 255), width=2)
        elif kind == "mine":
            c = n // 2
            d.ellipse([c - 8, c - 8, c + 8, c + 8], fill=(230, 230, 230, 255))
            for dx, dy in ((0, -13), (0, 13), (-13, 0), (13, 0),
                           (-9, -9), (9, 9), (9, -9), (-9, 9)):
                d.line([(c, c), (c + dx, c + dy)], fill=(230, 230, 230, 255), width=2)
        S.save_bmp32(img, os.path.join(BMP, "ms_%s.bmp" % kind), expect=(n, n))


def gen_xq_pieces():
    n = XQ_CELL
    for side, names, kinds, col in (("r", XQ_PIECES, XQ_KIND, RED),
                                    ("b", XQ_BLACK, XQ_BKIND, BLACK_PIECE)):
        for ch, kind in zip(names, kinds):
            img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
            d = ImageDraw.Draw(img, "RGBA")
            d.ellipse([1, 1, n - 2, n - 2], fill=(246, 236, 210, 255),
                      outline=col + (255,), width=3)
            S.text_center(d, n // 2, n // 2, ch, S.F(S.FONT_BOLD, 22), col + (255,))
            S.save_bmp32(img, os.path.join(BMP, "xq_%s_%s.bmp" % (side, kind)), expect=(n, n))


TET_COLORS = {
    "I": (66, 200, 214), "O": (232, 200, 78), "T": (176, 106, 214),
    "S": (98, 200, 120), "Z": (226, 96, 100), "J": (86, 130, 226), "L": (232, 138, 60),
}


def gen_tet_blocks():
    n = TET_CELL
    for k, col in TET_COLORS.items():
        img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
        d = ImageDraw.Draw(img, "RGBA")
        dark = tuple(int(c * 0.62) for c in col)
        d.rounded_rectangle([0, 0, n - 1, n - 1], radius=5, fill=col + (255,),
                            outline=(255, 255, 255, 70), width=1)
        d.rounded_rectangle([3, 3, n - 4, n - 7], radius=3, fill=dark + (255,))
        S.save_bmp32(img, os.path.join(BMP, "tet_%s.bmp" % k), expect=(n, n))


def gen_snake_sprites():
    n = SNK_CELL
    mk = [("head", (110, 220, 130), True), ("body", (70, 170, 96), False),
          ("food", (226, 88, 92), False)]
    for kind, col, eye in mk:
        img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
        d = ImageDraw.Draw(img, "RGBA")
        d.rounded_rectangle([2, 2, n - 3, n - 3], radius=8, fill=col + (255,))
        if eye:
            d.ellipse([9, 8, 14, 13], fill=(20, 20, 20, 255))
            d.ellipse([n - 14, 8, n - 9, 13], fill=(20, 20, 20, 255))
        if kind == "food":
            d.ellipse([6, 6, n - 7, n - 7], fill=col + (255,))
            d.ellipse([10, 9, 16, 15], fill=(255, 200, 200, 120))
        S.save_bmp32(img, os.path.join(BMP, "snake_%s.bmp" % kind), expect=(n, n))


def main():
    print("输出目录: %s" % os.path.abspath(BMP))
    print("\n[1/6] 六个游戏背景")
    gen_2048_bg()
    gen_ttt_bg()
    gen_ms_bg()
    gen_xq_bg()
    gen_tetris_bg()
    gen_snake_bg()
    print("\n[2/6] chrome 按钮")
    gen_chrome_buttons()
    print("\n[3/6] 井字棋棋子")
    gen_ttt_pieces()
    print("\n[4/6] 扫雷格子")
    gen_ms_tiles()
    print("\n[5/6] 象棋棋子 / 俄罗斯方块")
    gen_xq_pieces()
    gen_tet_blocks()
    print("\n[6/6] 贪吃蛇")
    gen_snake_sprites()

    print("\n几何常量(必须和 album/games.h 一致):")
    print("  TTT  cell=%d gap=%d x0=%d y0=%d" % (TTT_CELL, TTT_GAP, TTT_X0, TTT_Y0))
    print("  MS   cell=%d gap=%d x0=%d y0=%d" % (MS_CELL, MS_GAP, MS_X0, MS_Y0))
    print("  XQ   cell=%d x0=%d y0=%d" % (XQ_CELL, XQ_X0, XQ_Y0))
    print("  TET  cell=%d cols=%d rows=%d x0=%d y0=%d" %
          (TET_CELL, TET_COLS, TET_ROWS, TET_X0, TET_Y0))
    print("  SNK  cell=%d cols=%d rows=%d x0=%d y0=%d" %
          (SNK_CELL, SNK_COLS, SNK_ROWS, SNK_X0, SNK_Y0))
    print("  按钮 back%s again%s extra%s" % (BACK_POS, AGAIN_POS, EXTRA_POS))
    print("  方向按钮 %s" % ([p[:2] for p in TET_DIR_POS],))


if __name__ == "__main__":
    main()

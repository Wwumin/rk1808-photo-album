# -*- coding: utf-8 -*-
"""
gen_buttons.py  —  生成"独立按钮图片"(含按下态)，全部走 ui_style 的统一卡片样式

产出(都在 bmp/ 下):
    btn_main_<入口>[_p].bmp        主界面 6 个入口，296x190
    btn_album_<项>[_p].bmp         相册二级 4 项，452x190
    btn_lock_<动作>[_p].bmp        密码锁界面按钮(备用，240x130)
    btn_game_<游戏>[_p].bmp        游戏菜单 6 个入口，300x190
    btn_key_<0-9|del|ok>[_p].bmp   密码键盘，118x70
    main_menu.bg.bmp               主界面背景(光带+标题，无按钮)
    album_menu.bg.bmp              相册二级背景
    lock.bg.bmp                    密码锁背景
    game_menu.bg.bmp               游戏菜单背景
    buttons.txt                    清单，C 代码按它加载

命名规则:
    带 _p 后缀的是"按下态"，用 ui_style.pressed() 生成，
    和普通态尺寸完全一致，程序在按下/抬起之间切换这两张即可。

重要: 按钮图存 32bit BMP(save_bmp32)，圆角外 alpha=0。
      24bit BMP 没有 alpha，圆角会被糊成四个黑角 —— 这是踩过的坑。

运行:  python gen_buttons.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PIL import Image, ImageDraw  # noqa: E402

import ui_style as S  # noqa: E402

W_ = S.W
BMP_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bmp")

# ------------------------------------------------------------------ 布局常量
# 与最早那版主界面/二级界面完全一致，改这里等于改全部
MAIN_CARD = (296, 190)
MAIN_COLS, MAIN_ROWS = 3, 2
MAIN_GAP = (40, 34)
MAIN_ORIGIN = (28, 132)

ALBUM_CARD = (452, 190)
ALBUM_COLS, ALBUM_ROWS = 2, 2
ALBUM_GAP = (44, 30)
ALBUM_ORIGIN = (38, 138)

# 游戏菜单: 6 个入口。300x190 三列两行，正好落在 1024 宽里
GAME_CARD = (300, 190)
GAME_COLS, GAME_ROWS = 3, 2
GAME_GAP = (32, 30)
GAME_ORIGIN = (32, 132)

# 密码键盘: 4 行 3 列，必须落在 600 高以内
# (老版本 132x96 从 y=320 起，第 3、4 行直接跑到屏幕外，点得到但看不见)
KEY_SIZE = (118, 70)
KEY_GX, KEY_GY = 16, 10
KEY_X0 = (S.W - (3 * KEY_SIZE[0] + 2 * KEY_GX)) // 2      # 319
KEY_Y0 = 268

# (键, 中文, 英文, 颜色, 快捷键)
MAIN_ITEMS = [
    ("album", "相册",   "PHOTO ALBUM", (74, 144, 226), "[1]"),
    ("game",  "游戏",   "GAME",        (232, 138, 46), "[2]"),
    ("media", "多媒体", "MULTIMEDIA",  (84, 190, 130), "[3]"),
    ("other", "其他",   "OTHERS",      (150, 108, 214), "[4]"),
    ("exit",  "退出",   "EXIT",        (226, 88, 92),  "[5]"),
    ("opt",   "选项",   "OPTION",      (108, 122, 148), "[6]"),
]

ALBUM_ITEMS = [
    ("manual", "手动", "MANUAL", (80, 190, 255), "触摸上下翻页"),
    ("auto",   "自动", "AUTO",   (86, 214, 150), "每 3 秒自动切换"),
    ("exit",   "退出", "EXIT",   (232, 96, 100), "返回主界面"),
    ("opt",    "选项", "OPTION", (110, 126, 156), "播放设置"),
]

LOCK_ITEMS = [
    ("ok",   "确定", "ENTER",        (86, 214, 150), None),
    ("del",  "删除", "BACKSPACE",    (226, 138, 46), None),
    ("exit", "退出", "LOCK / EXIT",  (226, 88, 92),  None),
]

# 游戏菜单: 键 -> 中文 / 英文 / 颜色 / 中文字号
GAME_ITEMS = [
    ("ttt",    "井字棋",     "TIC-TAC-TOE", (86, 190, 235),  54),
    ("2048",   "2048",       "2048",        (232, 138, 46),  58),
    ("ms",     "扫雷",       "MINESWEEPER", (226, 96, 100),  54),
    ("xq",     "象棋",       "XIANGQI",     (150, 108, 214), 54),
    ("tetris", "俄罗斯方块", "TETRIS",      (86, 214, 150),  44),
    ("snake",  "贪吃蛇",     "SNAKE",       (226, 190, 80),  50),
]

KEYPAD_DIGITS = ["1", "2", "3", "4", "5", "6", "7", "8", "9"]
KEYPAD_COLOR = (120, 150, 220)


def place(index, cols, card, gap, origin):
    r, c = divmod(index, cols)
    return (origin[0] + c * (card[0] + gap[0]),
            origin[1] + r * (card[1] + gap[1]))


# ------------------------------------------------------------------ 生成器
def gen_main():
    theme = S.THEME_MAIN
    bg = S.page_bg(theme, title="电 子 相 册", subtitle="ELECTRONIC  PHOTO  ALBUM")
    d = ImageDraw.Draw(bg, "RGBA")
    d.text((W_ - 200, 32), "RK1808  DRM", font=S.F(S.FONT_NUM, 14), fill=(110, 140, 200))
    S.text_center(d, S.W // 2, S.H - 26,
                  "触摸方框选择   ·   相册 / 游戏 / 多媒体 / 其他 / 退出 / 选项",
                  S.F(S.FONT_UI, 16), theme["tip"])
    S.save_bmp(bg, os.path.join(BMP_DIR, "main_menu.bg.bmp"))

    names = []
    for i, (key, cn, en, col, hint) in enumerate(MAIN_ITEMS):
        img = S.card(MAIN_CARD, theme, cn, en, col, key=hint)
        p = os.path.join(BMP_DIR, "btn_main_%s.bmp" % key)
        S.save_bmp32(img, p, expect=MAIN_CARD)
        S.save_bmp32(S.pressed(img, col), os.path.join(BMP_DIR, "btn_main_%s_p.bmp" % key),
                     expect=MAIN_CARD)
        names.append("main_%s" % key)
    return names


def gen_album():
    theme = S.THEME_ALBUM
    bg = S.page_bg(theme, title="相  册", subtitle="PHOTO  ALBUM   ·   二级界面",
                   back_label="< 返回")
    S.save_bmp(bg, os.path.join(BMP_DIR, "album_menu.bg.bmp"))

    names = []
    for i, (key, cn, en, col, tip) in enumerate(ALBUM_ITEMS):
        img = S.card(ALBUM_CARD, theme, cn, en, col, tip=tip)
        S.save_bmp32(img, os.path.join(BMP_DIR, "btn_album_%s.bmp" % key), expect=ALBUM_CARD)
        S.save_bmp32(S.pressed(img, col), os.path.join(BMP_DIR, "btn_album_%s_p.bmp" % key),
                     expect=ALBUM_CARD)
        names.append("album_%s" % key)
    return names


def gen_lock():
    theme = S.THEME_LOCK
    bg = S.page_bg(theme, title="账 户 登 录", subtitle="ACCOUNT  /  PASSWORD  LOCK")
    d = ImageDraw.Draw(bg, "RGBA")

    # 密码框(内容由 C 代码画点，这里只画框)。
    # 框和键盘都要给数字键盘腾地方: 键盘从 y=268 起，4 行到 y=578 结束。
    d.rounded_rectangle([312, 140, 712, 224], radius=14,
                        fill=(20, 16, 34, 255), outline=theme["line"] + (255,), width=3)
    S.text_center(d, S.W // 2, 118, "请输入 4 位密码", S.F(S.FONT_UI, 18), theme["subtitle"])
    S.text_center(d, S.W // 2, 244, "初始密码 1234   ·   忘记密码请联系管理员",
                  S.F(S.FONT_UI, 15), (150, 130, 190))
    S.save_bmp(bg, os.path.join(BMP_DIR, "lock.bg.bmp"))

    names = []
    size = (240, 130)
    for (key, cn, en, col, _) in LOCK_ITEMS:
        img = S.card(size, theme, cn, en, col)
        S.save_bmp32(img, os.path.join(BMP_DIR, "btn_lock_%s.bmp" % key), expect=size)
        S.save_bmp32(S.pressed(img, col), os.path.join(BMP_DIR, "btn_lock_%s_p.bmp" % key),
                     expect=size)
        names.append("lock_%s" % key)
    return names


def gen_game():
    theme = S.THEME_GAME
    bg = S.page_bg(theme, title="游  戏", subtitle="GAME   ·   附加题", back_label="< 返回")
    d = ImageDraw.Draw(bg, "RGBA")
    S.text_center(d, S.W // 2, S.H - 40,
                  "井字棋 / 2048 / 扫雷 / 象棋 / 俄罗斯方块 / 贪吃蛇",
                  S.F(S.FONT_UI, 17), theme["tip"])
    S.save_bmp(bg, os.path.join(BMP_DIR, "game_menu.bg.bmp"))

    names = []
    for i, (key, cn, en, col, cn_size) in enumerate(GAME_ITEMS):
        img = S.card(GAME_CARD, theme, cn, en, col, key="[%d]" % (i + 1),
                     cn_size=cn_size)
        S.save_bmp32(img, os.path.join(BMP_DIR, "btn_game_%s.bmp" % key), expect=GAME_CARD)
        S.save_bmp32(S.pressed(img, col), os.path.join(BMP_DIR, "btn_game_%s_p.bmp" % key),
                     expect=GAME_CARD)
        names.append("game_%s" % key)
    return names


def gen_keypad():
    """密码锁数字键盘: 0-9 + 确定 + 删除，共 12 个键。"""
    theme = S.THEME_LOCK
    names = []

    def key_img(label, cn_size):
        return S.card(KEY_SIZE, theme, label, "", KEYPAD_COLOR,
                      cn_size=cn_size, en_size=0, radius=12, bar_w=6, bar_x=10)

    def save(img, name):
        p = os.path.join(BMP_DIR, name)
        S.save_bmp32(img, p, expect=KEY_SIZE)
        S.save_bmp32(S.pressed(img, KEYPAD_COLOR), p.replace(".bmp", "_p.bmp"),
                     expect=KEY_SIZE)
        names.append(name[4:-4])

    for d in KEYPAD_DIGITS:
        save(key_img(d, 40), "btn_key_%s.bmp" % d)
    save(key_img("0", 40), "btn_key_0.bmp")
    save(key_img("删除", 30), "btn_key_del.bmp")
    save(key_img("确定", 30), "btn_key_ok.bmp")
    return names


def main():
    print("输出目录: %s" % os.path.abspath(BMP_DIR))
    all_names = []

    print("\n[1/5] 主界面")
    all_names += gen_main()
    print("\n[2/5] 相册二级界面")
    all_names += gen_album()
    print("\n[3/5] 账户登录(密码锁)")
    all_names += gen_lock()
    print("\n[4/5] 密码键盘")
    all_names += gen_keypad()
    print("\n[5/5] 游戏菜单")
    all_names += gen_game()

    manifest = os.path.join(BMP_DIR, "buttons.txt")
    with open(manifest, "w", encoding="utf-8", newline="\n") as f:
        f.write("# 由 tools/gen_buttons.py 生成，每行一个按钮 id\n")
        f.write("# 程序会按 id 去找 btn_<id>.bmp 与 btn_<id>_p.bmp\n")
        for n in all_names:
            f.write("%s\n" % n)
    print("\n写出清单 %s (%d 个按钮)" % (manifest, len(all_names)))

    print("\n布局坐标(和 C 代码里的热区必须一致):")
    for i, (key, cn, _e, _c, _k) in enumerate(MAIN_ITEMS):
        x, y = place(i, MAIN_COLS, MAIN_CARD, MAIN_GAP, MAIN_ORIGIN)
        print("  主界面 %-6s %-4s x=%4d y=%4d w=%d h=%d" % (key, cn, x, y, *MAIN_CARD))
    for i, (key, cn, _e, _c, _t) in enumerate(ALBUM_ITEMS):
        x, y = place(i, ALBUM_COLS, ALBUM_CARD, ALBUM_GAP, ALBUM_ORIGIN)
        print("  相册   %-6s %-4s x=%4d y=%4d w=%d h=%d" % (key, cn, x, y, *ALBUM_CARD))
    for i, (key, cn, _e, _c, _s) in enumerate(GAME_ITEMS):
        x, y = place(i, GAME_COLS, GAME_CARD, GAME_GAP, GAME_ORIGIN)
        print("  游戏   %-6s %-4s x=%4d y=%4d w=%d h=%d" % (key, cn, x, y, *GAME_CARD))

    print("  键盘   %dx%d 每键 %dx%d 起点 (%d,%d)" %
          (3, 4, KEY_SIZE[0], KEY_SIZE[1], KEY_X0, KEY_Y0))
    for i in range(12):
        r, c = divmod(i, 3)
        print("         第%d行第%d列 x=%4d y=%4d" %
              (r + 1, c + 1, KEY_X0 + c * (KEY_SIZE[0] + KEY_GX),
               KEY_Y0 + r * (KEY_SIZE[1] + KEY_GY)))


if __name__ == "__main__":
    main()

import io

p = "album/main.c"
s = io.open(p, encoding="utf-8").read()
orig = s

# ---------------------------------------------------------------- 1. 棋盘上移
s = s.replace('#define BOARD_ORIGIN_Y  150', '#define BOARD_ORIGIN_Y  130')

# ---------------------------------------------------------------- 2. 背景图全局
s = s.replace('static Image *g_tile_bg = NULL;',
              'static Image *g_tile_bg = NULL;\n'
              'static Image *g_2048_bg = NULL;      /* 2048 静态背景(标题/棋盘底/信息面板/chrome 按钮) */',
              1)

# ---------------------------------------------------------------- 3. 重写 2048 绘制
new_render = r'''static void game2048_render(void)
{
    int w = fbw(), h = fbh();
    int ix = BOARD_ORIGIN_X + 444 + 24;     /* 右侧信息栏左边 = 758 */
    int iy = BOARD_ORIGIN_Y;                /* 130 */
    char buf[128];

    /*
     * 背景是一张烘好的图: 标题、棋盘底、SCORE/BEST/UNDO 面板、左下"返回"/右下"重来"
     * 都在图里，和另外五个游戏用的是同一套主题和同一套 chrome 按钮位置。
     * 这里只画会变的部分: 方块、数字、结束遮罩。
     */
    if (g_2048_bg) {
        image_blit_alpha(fb(), fbp(), w, h, g_2048_bg, 0, 0);
    } else {
        draw_fill_rect(fb(), fbp(), w, h, 0, 0, w, h, BGRA(26, 18, 12), 255);
    }

    if (g_tile_bg) {
        image_blit_alpha(fb(), fbp(), w, h, g_tile_bg, BOARD_ORIGIN_X, BOARD_ORIGIN_Y);
    }

    for (int r = 0; r < G2048_N; r++) {
        for (int c = 0; c < G2048_N; c++) {
            int e = g2048_cell(r, c);
            if (e <= 0 || e > G2048_MAX_EXP) {
                continue;
            }
            if (!g_tile[e]) {
                continue;
            }
            int x = BOARD_ORIGIN_X + TILE_GAP + c * (TILE_SIZE + TILE_GAP);
            int y = BOARD_ORIGIN_Y + TILE_GAP + r * (TILE_SIZE + TILE_GAP);
            image_blit_alpha(fb(), fbp(), w, h, g_tile[e], x, y);
        }
    }

    /* 分数 / 最高分: 面板位置和 tools/gen_games.py 里的 gen_2048_bg() 对齐 */
    snprintf(buf, sizeof(buf), "%d", g2048_score());
    draw_text(fb(), fbp(), w, h, ix + 118 - draw_text_width(buf, 2) / 2, iy + 54,
              buf, C_WHITE, 2);

    snprintf(buf, sizeof(buf), "%d", g2048_best());
    draw_text(fb(), fbp(), w, h, ix + 118 - draw_text_width(buf, 2) / 2, iy + 178,
              buf, C_ACCENT, 2);

    /* 悔棋剩余次数 */
    snprintf(buf, sizeof(buf), "x %d", g2048_undo_left());
    draw_text(fb(), fbp(), w, h, ix + 118 - draw_text_width(buf, 2) / 2, iy + 294,
              buf, g2048_undo_left() > 0 ? C_OK : BGRA(120, 110, 100), 2);

    /* 胜利/结束遮罩 */
    if (g2048_won() || g2048_over()) {
        const char *title = g2048_won() ? "YOU WIN !" : "GAME OVER";
        uint32_t col = g2048_won() ? C_OK : BGRA(255, 120, 120);
        int bw = 460, bh = 130;
        int bx = (w - bw) / 2, by = (h - bh) / 2;
        draw_fill_rect(fb(), fbp(), w, h, bx - 2, by - 2, bw + 4, bh + 4, C_BLACK, 235);
        draw_rect(fb(), fbp(), w, h, bx, by, bw, bh, col);
        draw_text_center(fb(), fbp(), w, h, w / 2, by + 22, title, col, 3);
        snprintf(buf, sizeof(buf), "SCORE %d", g2048_score());
        draw_text_center(fb(), fbp(), w, h, w / 2, by + 74, buf, C_HUD, 1);
        draw_text_center(fb(), fbp(), w, h, w / 2, by + 100,
                         g2048_over() ? "TAP 重来 OR SWIPE TO CONTINUE"
                                      : "KEEP GOING WITH SWIPE", C_HUD, 1);
    }

    if (toast_active()) {
        draw_toast();
    }
    display_present();
}
'''

start = s.index('static void game2048_render(void)')
login = s.index('static void login_reset(void)')
comment = s.rindex('/*', start, login)
s = s[:start] + new_render + '\n' + s[comment:]

# ---------------------------------------------------------------- 4. 2048 的点击判定
old_press = '''static void on_2048_press(int x, int y)
{
    int ix = BOARD_ORIGIN_X + 444 + 24;
    int iy = BOARD_ORIGIN_Y;
'''
new_press = '''static void on_2048_press(int x, int y)
{
    int ix = BOARD_ORIGIN_X + 444 + 24;
    int iy = BOARD_ORIGIN_Y;

    /* chrome 按钮(位置和另外五个游戏一致, 见 games.h 的 G_BACK_*/G_AGAIN_*) */
    if (x >= G_BACK_X && x <= G_BACK_X + G_BACK_W &&
        y >= G_BACK_Y && y <= G_BACK_Y + G_BACK_H) {
        goto_game();
        return;
    }
    if (x >= G_AGAIN_X && x <= G_AGAIN_X + G_AGAIN_W &&
        y >= G_AGAIN_Y && y <= G_AGAIN_Y + G_AGAIN_H) {
        g2048_new();
        game2048_render();
        return;
    }
'''
if old_press not in s:
    raise SystemExit("on_2048_press 锚点没找到")
s = s.replace(old_press, new_press, 1)

# 悔棋面板的判定范围改成和新背景一致
s = s.replace('if (x >= ix && x <= ix + 240 && y >= iy + 200 && y <= iy + 240) {',
              'if (x >= ix && x <= ix + 236 && y >= iy + 248 && y <= iy + 340) {', 1)
s = s.replace('    /* 镇旀鍖哄煙 */', '    /* 悔棋面板 */', 1)

# ---------------------------------------------------------------- 5. 加载背景图
s = s.replace('    g_tile_bg = load_asset("tile_bg.bmp");',
              '    g_tile_bg = load_asset("tile_bg.bmp");\n'
              '    g_2048_bg = load_asset("g2048_bg.bmp");', 1)

io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("patched main.c: %d -> %d bytes" % (len(orig), len(s)))

/*
 * tet.c  —  俄罗斯方块(10x20，7 种方块)
 *
 * 触摸屏没有键盘，所以方向键做成 chrome 上四个按钮:
 *   左 / 转 / 右 / 落到底  (直接点棋盘 = 旋转，手感更顺)
 *
 * 方块用 4x4 的"位图"表示，旋转就是把 4x4 顺时针转 90 度 ——
 * 比给每个方块手写 4 组坐标少 28 组数字，也不容易写错。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games.h"
#include "display.h"
#include "draw.h"

#define TET_COLS_  10
#define TET_ROWS_  20
#define TET_NPIECE 7

static const UiPage *s_page = NULL;
static Image *s_blk[TET_NPIECE + 1];        /* 下标 = 类型 1..7 */

typedef struct {
    int cells[4][4];        /* 4x4 位图 */
} Shape;

static const Shape BASE[TET_NPIECE] = {
    /* I */ { { { 0,0,0,0 }, { 1,1,1,1 }, { 0,0,0,0 }, { 0,0,0,0 } } },
    /* O */ { { { 1,1,0,0 }, { 1,1,0,0 }, { 0,0,0,0 }, { 0,0,0,0 } } },
    /* T */ { { { 0,1,0,0 }, { 1,1,1,0 }, { 0,0,0,0 }, { 0,0,0,0 } } },
    /* S */ { { { 0,1,1,0 }, { 1,1,0,0 }, { 0,0,0,0 }, { 0,0,0,0 } } },
    /* Z */ { { { 1,1,0,0 }, { 0,1,1,0 }, { 0,0,0,0 }, { 0,0,0,0 } } },
    /* J */ { { { 1,0,0,0 }, { 1,1,1,0 }, { 0,0,0,0 }, { 0,0,0,0 } } },
    /* L */ { { { 0,0,1,0 }, { 1,1,1,0 }, { 0,0,0,0 }, { 0,0,0,0 } } },
};

static const char *TET_NAME[TET_NPIECE + 1] = {
    "", "tet_I.bmp", "tet_O.bmp", "tet_T.bmp", "tet_S.bmp", "tet_Z.bmp",
    "tet_J.bmp", "tet_L.bmp",
};

static int s_board[TET_ROWS_][TET_COLS_];
static Shape s_cur;
static int s_type, s_rot, s_row, s_col;
static int s_next;
static int s_score, s_lines, s_level;
static int s_over;
static long long s_last_drop;
static int s_drop_ms;

/* ------------------------------------------------------------------ 形状 */
static Shape rotate_cw(const Shape *s)
{
    Shape r;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            r.cells[i][j] = s->cells[3 - j][i];
        }
    }
    return r;
}

static int collides(const Shape *sh, int row, int col)
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (!sh->cells[i][j]) {
                continue;
            }
            int r = row + i, c = col + j;
            if (c < 0 || c >= TET_COLS_ || r >= TET_ROWS_) {
                return 1;
            }
            if (r >= 0 && s_board[r][c]) {
                return 1;
            }
        }
    }
    return 0;
}

static void spawn(void)
{
    s_type = s_next;
    s_next = rand() % TET_NPIECE;
    s_cur = BASE[s_type - 1];
    s_rot = 0;
    s_row = -1;
    s_col = (s_type == 1) ? 3 : 3;      /* I 型在 4x4 里也从第 3 列起 */
    if (collides(&s_cur, s_row, s_col)) {
        s_over = 1;
        gs_status("GAME OVER  -  SCORE %d  -  TAP RESTART", s_score);
        printf("[tet] 游戏结束 得分 %d\n", s_score);
    }
}

static void tet_reset(void)
{
    memset(s_board, 0, sizeof(s_board));
    s_score = 0;
    s_lines = 0;
    s_level = 1;
    s_over = 0;
    s_drop_ms = 480;
    s_next = rand() % TET_NPIECE;
    s_last_drop = 0;
    spawn();
    gs_status("LINES %d  -  LEFT / ROT / RIGHT / DROP", 0);
}

static void clear_lines(void)
{
    int cleared = 0;
    for (int r = TET_ROWS_ - 1; r >= 0; r--) {
        int full = 1;
        for (int c = 0; c < TET_COLS_; c++) {
            if (!s_board[r][c]) {
                full = 0;
                break;
            }
        }
        if (!full) {
            continue;
        }
        cleared++;
        for (int rr = r; rr > 0; rr--) {
            memcpy(s_board[rr], s_board[rr - 1], sizeof(s_board[0]));
        }
        memset(s_board[0], 0, sizeof(s_board[0]));
        r++;                /* 同一行还要再查一次(上面的行落下来了) */
    }
    if (cleared) {
        s_score += (cleared == 1) ? 100 : (cleared == 2) ? 300 : (cleared == 3) ? 500 : 800;
        s_lines += cleared;
        s_level = s_lines / 6 + 1;
        s_drop_ms = 480 - (s_level - 1) * 45;
        if (s_drop_ms < 120) {
            s_drop_ms = 120;
        }
    }
}

static void lock_piece(void)
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (!s_cur.cells[i][j]) {
                continue;
            }
            int r = s_row + i, c = s_col + j;
            if (r >= 0 && r < TET_ROWS_ && c >= 0 && c < TET_COLS_) {
                s_board[r][c] = s_type;
            }
        }
    }
    clear_lines();
    spawn();
    if (!s_over) {
        gs_status("SCORE %d  -  LEVEL %d  -  LINES %d", s_score, s_level, s_lines);
    }
}

static int try_move(int dr, int dc)
{
    if (s_over) {
        return 0;
    }
    if (!collides(&s_cur, s_row + dr, s_col + dc)) {
        s_row += dr;
        s_col += dc;
        return 1;
    }
    return 0;
}

/* 旋转后 4x4 里可能有整行空，往下贴一贴，不然方块会"悬空" */
static void collapse_zero_rows(Shape *sh)
{
    int empty_top = 1;
    for (int j = 0; j < 4; j++) {
        if (sh->cells[0][j]) {
            empty_top = 0;
            break;
        }
    }
    if (!empty_top) {
        return;
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            sh->cells[i][j] = sh->cells[i + 1][j];
        }
    }
    for (int j = 0; j < 4; j++) {
        sh->cells[3][j] = 0;
    }
}

static void try_rotate(void);

static void try_rotate(void)
{
    Shape r;
    if (s_over) {
        return;
    }
    r = rotate_cw(&s_cur);
    /* 简单的踢墙: 原地转不下就往左/右挪一格再试 */
    for (int k = 0; k <= 2; k++) {
        static const int OFF[3] = { 0, -1, 1 };
        if (!collides(&r, s_row, s_col + OFF[k])) {
            s_cur = r;
            s_col += OFF[k];
            collapse_zero_rows(&s_cur);
            return;
        }
    }
}

static void hard_drop(void)
{
    if (s_over) {
        return;
    }
    while (!collides(&s_cur, s_row + 1, s_col)) {
        s_row++;
    }
    lock_piece();
}

/* ------------------------------------------------------------------ 绘制 */
static void load_sprites(void)
{
    for (int t = 1; t <= TET_NPIECE; t++) {
        if (!s_blk[t]) {
            s_blk[t] = gs_load(NULL, TET_NAME[t]);
        }
    }
}

static void blk(uint8_t *fb, int pitch, int w, int h, int r, int c, int type)
{
    Image *im = s_blk[type];
    if (!im) {
        return;
    }
    image_blit_alpha(fb, pitch, w, h, im,
                     G_TET_X0 + c * G_TET_CELL, G_TET_Y0 + r * G_TET_CELL);
}

static void tet_render(int active)
{
    uint8_t *fb = display_backbuffer();
    int pitch = display_pitch();
    int w = display_width(), h = display_height();
    char buf[64];

    if (!s_page) {
        return;
    }
    load_sprites();
    ui_page_draw_bg(s_page);

    for (int r = 0; r < TET_ROWS_; r++) {
        for (int c = 0; c < TET_COLS_; c++) {
            if (s_board[r][c]) {
                blk(fb, pitch, w, h, r, c, s_board[r][c]);
            }
        }
    }

    if (!s_over) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (!s_cur.cells[i][j]) {
                    continue;
                }
                int r = s_row + i, c = s_col + j;
                if (r >= 0) {
                    blk(fb, pitch, w, h, r, c, s_type);
                }
            }
        }
    }

    /* 右侧: 分数 + 下一个 */
    snprintf(buf, sizeof(buf), "%d", s_score);
    draw_text_center(fb, pitch, w, h, 845, 186, buf, BGRA(255, 236, 200), 2);
    snprintf(buf, sizeof(buf), "LV %d", s_level);
    draw_text_center(fb, pitch, w, h, 845, 222, buf, BGRA(230, 200, 150), 1);

    {
        const Shape *nx = &BASE[s_next - 1];
        int minr = 4, maxr = -1, minc = 4, maxc = -1;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (nx->cells[i][j]) {
                    if (i < minr) minr = i;
                    if (i > maxr) maxr = i;
                    if (j < minc) minc = j;
                    if (j > maxc) maxc = j;
                }
            }
        }
        int bw = (maxc - minc + 1) * G_TET_CELL;
        int bh = (maxr - minr + 1) * G_TET_CELL;
        int bx = 845 - bw / 2 - minc * G_TET_CELL;
        int by = 300 - bh / 2 - minr * G_TET_CELL;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (!nx->cells[i][j]) {
                    continue;
                }
                Image *im = s_blk[s_next];
                if (im) {
                    image_blit_alpha(fb, pitch, w, h, im,
                                     bx + j * G_TET_CELL, by + i * G_TET_CELL);
                }
            }
        }
    }

    if (s_over) {
        int bw = 420, bh = 110;
        int bx = (w - bw) / 2, by = (h - bh) / 2;
        draw_fill_rect(fb, pitch, w, h, bx, by, bw, bh, BGRA(0, 0, 0), 225);
        draw_rect(fb, pitch, w, h, bx, by, bw, bh, BGRA(255, 120, 120));
        draw_text_center(fb, pitch, w, h, w / 2, by + 24, "GAME OVER",
                         BGRA(255, 120, 120), 3);
        snprintf(buf, sizeof(buf), "SCORE %d", s_score);
        draw_text_center(fb, pitch, w, h, w / 2, by + 74, buf, BGRA(230, 220, 210), 1);
    }

    ui_page_draw_buttons(s_page, active);
    gs_draw_status();
    display_present();
}

/* ------------------------------------------------------------------ 交互 */
static void tet_tap(int x, int y)
{
    if (x >= G_TET_X0 && x < G_TET_X0 + TET_COLS_ * G_TET_CELL &&
        y >= G_TET_Y0 && y < G_TET_Y0 + TET_ROWS_ * G_TET_CELL) {
        try_rotate();
        tet_render(-1);
        return;
    }
    /* 点右侧信息区也当旋转，扩大可点范围 */
    if (x >= 700 && y >= 118 && y <= 360) {
        try_rotate();
        tet_render(-1);
    }
}

static int tet_button(const char *id)
{
    if (strcmp(id, "g_again") == 0) {
        tet_reset();
        return 1;
    }
    if (s_over) {
        return 1;
    }
    if (strcmp(id, "g_left") == 0) {
        try_move(0, -1);
    } else if (strcmp(id, "g_right") == 0) {
        try_move(0, 1);
    } else if (strcmp(id, "g_rot") == 0) {
        try_rotate();
    } else if (strcmp(id, "g_drop") == 0) {
        hard_drop();
    } else {
        return 0;
    }
    return 1;
}

static void tet_enter(const UiPage *page)
{
    s_page = page;
    tet_reset();
    tet_render(-1);
}

static void tet_tick(long long now_ms)
{
    if (s_over) {
        return;
    }
    if (s_last_drop == 0) {
        s_last_drop = now_ms;
        return;
    }
    if (now_ms - s_last_drop < s_drop_ms) {
        return;
    }
    s_last_drop = now_ms;

    if (try_move(1, 0)) {
        tet_render(-1);
    } else {
        lock_piece();
        tet_render(-1);
    }
}

const GameOps game_tet = {
    "tet",
    tet_enter,
    tet_render,
    tet_tap,
    tet_button,
    tet_tick,
};

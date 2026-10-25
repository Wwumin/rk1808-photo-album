/*
 * snk.c  —  贪吃蛇(20x15)
 *
 * 操作: 直接点棋盘 —— 点在头的右边就往右拐，以此类推。
 * 为什么不做方向键: 蛇要连续微调，四个 66 像素高的按钮在触摸屏上
 * 反而比"点哪里就往哪里"慢；而且这样也和井字棋/象棋的"点棋盘"
 * 交互保持一致。
 *
 * 不允许 180 度掉头。这条规则要单独写，因为"反向"在数据上是一个
 * 合法方向，只是会让蛇立刻咬到自己脖子。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games.h"
#include "display.h"
#include "draw.h"

#define U 0
#define R 1
#define D 2
#define L 3

static const int DX[4] = { 0, 1, 0, -1 };
static const int DY[4] = { -1, 0, 1, 0 };

#define SNK_MAX (G_SNK_COLS * G_SNK_ROWS)

static const UiPage *s_page = NULL;
static Image *s_head = NULL, *s_body = NULL, *s_food = NULL;

static int s_br[SNK_MAX], s_bc[SNK_MAX];
static int s_len;
static int s_dir;
static int s_pending;       /* 本 tick 要拐的方向, -1 表示不拐 */
static int s_food_r, s_food_c;
static int s_score;
static int s_over;
static int s_step_ms;
static long long s_last_step;

static void place_food(void)
{
    int tries = 0;
    do {
        s_food_r = rand() % G_SNK_ROWS;
        s_food_c = rand() % G_SNK_COLS;
        tries++;
        int occupied = 0;
        for (int i = 0; i < s_len; i++) {
            if (s_br[i] == s_food_r && s_bc[i] == s_food_c) {
                occupied = 1;
                break;
            }
        }
        if (!occupied) {
            return;
        }
    } while (tries < 2000);
}

static void snk_reset(void)
{
    s_len = 4;
    for (int i = 0; i < s_len; i++) {
        s_br[i] = G_SNK_ROWS / 2;
        s_bc[i] = G_SNK_COLS / 2 - i;
    }
    s_dir = R;
    s_pending = -1;
    s_score = 0;
    s_over = 0;
    s_step_ms = 170;
    s_last_step = 0;
    place_food();
    gs_status("LEN %d  -  TAP A SIDE TO TURN", s_len);
}

static void snk_step(void)
{
    if (s_over) {
        return;
    }
    if (s_pending >= 0 && !((s_pending + 2) % 4 == s_dir)) {
        s_dir = s_pending;
    }
    s_pending = -1;

    int nr = s_br[0] + DY[s_dir];
    int nc = s_bc[0] + DX[s_dir];

    if (nr < 0 || nr >= G_SNK_ROWS || nc < 0 || nc >= G_SNK_COLS) {
        s_over = 1;
        gs_status("HIT WALL  -  LEN %d  -  SCORE %d", s_len, s_score);
        printf("[snk] 撞墙 得分 %d\n", s_score);
        return;
    }

    int eat = (nr == s_food_r && nc == s_food_c);
    int limit = eat ? s_len : s_len - 1;
    for (int i = 0; i < limit; i++) {
        if (s_br[i] == nr && s_bc[i] == nc) {
            s_over = 1;
            gs_status("ATE ITSELF  -  LEN %d  -  SCORE %d", s_len, s_score);
            printf("[snk] 自咬 得分 %d\n", s_score);
            return;
        }
    }

    int last_r = s_br[s_len - 1], last_c = s_bc[s_len - 1];
    for (int i = s_len - 1; i > 0; i--) {
        s_br[i] = s_br[i - 1];
        s_bc[i] = s_bc[i - 1];
    }
    s_br[0] = nr;
    s_bc[0] = nc;

    if (eat) {
        s_br[s_len] = last_r;
        s_bc[s_len] = last_c;
        s_len++;
        s_score += 10;
        if (s_step_ms > 95) {
            s_step_ms -= 3;
        }
        place_food();
        gs_status("LEN %d  -  SCORE %d  -  RESTART", s_len, s_score, 0);
    }
}

/* ------------------------------------------------------------------ 绘制 */
static void load_sprites(void)
{
    if (!s_head) s_head = gs_load(NULL, "snake_head.bmp");
    if (!s_body) s_body = gs_load(NULL, "snake_body.bmp");
    if (!s_food) s_food = gs_load(NULL, "snake_food.bmp");
}

static void cell_xy(int r, int c, int *x, int *y)
{
    *x = G_SNK_X0 + c * G_SNK_CELL;
    *y = G_SNK_Y0 + r * G_SNK_CELL;
}

static void snk_render(int active)
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

    if (s_food) {
        int x, y;
        cell_xy(s_food_r, s_food_c, &x, &y);
        image_blit_alpha(fb, pitch, w, h, s_food, x, y);
    }
    for (int i = s_len - 1; i >= 0; i--) {
        int x, y;
        cell_xy(s_br[i], s_bc[i], &x, &y);
        Image *im = (i == 0) ? s_head : s_body;
        if (im) {
            image_blit_alpha(fb, pitch, w, h, im, x, y);
        }
    }

    if (s_over) {
        int bw = 460, bh = 110;
        int bx = (w - bw) / 2, by = (h - bh) / 2;
        draw_fill_rect(fb, pitch, w, h, bx, by, bw, bh, BGRA(0, 0, 0), 225);
        draw_rect(fb, pitch, w, h, bx, by, bw, bh, BGRA(255, 150, 150));
        draw_text_center(fb, pitch, w, h, w / 2, by + 22, "GAME OVER",
                         BGRA(255, 150, 150), 3);
        snprintf(buf, sizeof(buf), "LEN %d   SCORE %d", s_len, s_score);
        draw_text_center(fb, pitch, w, h, w / 2, by + 72, buf,
                         BGRA(230, 220, 210), 1);
    }

    ui_page_draw_buttons(s_page, active);
    gs_draw_status();
    display_present();
}

/* ------------------------------------------------------------------ 交互 */
static void snk_tap(int x, int y)
{
    int hx, hy, dx, dy, dir;

    if (s_over) {
        return;
    }
    cell_xy(s_br[0], s_bc[0], &hx, &hy);
    dx = x - (hx + G_SNK_CELL / 2);
    dy = y - (hy + G_SNK_CELL / 2);

    if (dx == 0 && dy == 0) {
        return;
    }
    if (abs(dx) >= abs(dy)) {
        dir = (dx > 0) ? R : L;
    } else {
        dir = (dy > 0) ? D : U;
    }
    if ((dir + 2) % 4 == s_dir) {
        return;                 /* 不许掉头 */
    }
    s_pending = dir;
    snk_render(-1);
}

static int snk_button(const char *id)
{
    if (strcmp(id, "g_again") == 0) {
        snk_reset();
        return 1;
    }
    return 0;
}

static void snk_enter(const UiPage *page)
{
    s_page = page;
    snk_reset();
    snk_render(-1);
}

static void snk_tick(long long now_ms)
{
    if (s_over) {
        return;
    }
    if (s_last_step == 0) {
        s_last_step = now_ms;
        return;
    }
    if (now_ms - s_last_step < s_step_ms) {
        return;
    }
    s_last_step = now_ms;
    snk_step();
    snk_render(-1);
}

const GameOps game_snk = {
    "snk",
    snk_enter,
    snk_render,
    snk_tap,
    snk_button,
    snk_tick,
};

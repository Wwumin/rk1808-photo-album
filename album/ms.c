/*
 * ms.c  —  扫雷(9x9，10 颗雷)
 *
 * 交互约定:
 *   - 默认"翻开"模式: 点格子翻开
 *   - 点左下角"插旗"按钮切到"插旗"模式: 点格子插/拔旗
 *   触摸屏没有右键，只能用一个模式按钮代替，切模式时棋盘描一圈紫边做提示。
 *
 * 两个细节值得留在这里:
 *   1. 第一下点到的格子(以及它周围 8 格)不放雷 —— 否则开局就可能直接炸，
 *      体验很差。所以布雷是"等到第一次翻格"才做的，不是进界面就布。
 *   2. 翻开用显式栈做广度扩散，不用递归 —— 递归在 81 格时不会爆栈，
 *      但这类"看起来不会爆"的地方最容易在别的尺寸下变成真 bug。
 *      栈的容量按"每个格子最多入队一次"证明（见 reveal() 里的 queued[]）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games.h"
#include "display.h"
#include "draw.h"
#include "rng.h"

#define MS_N     9
#define MS_MINES 10

#define ST_IDLE 0            /* 还没布雷 */
#define ST_PLAY 1
#define ST_LOSE 2
#define ST_WIN  3

static const UiPage *s_page = NULL;
static Image *s_closed = NULL, *s_open = NULL, *s_flag = NULL, *s_mine_img = NULL;

static int s_mine[MS_N * MS_N];
static int s_open_[MS_N * MS_N];
static int s_flag_[MS_N * MS_N];
static int s_state;
static int s_flag_mode;
static int s_opened;
static int s_mines_left;

static const uint32_t NUM_COLOR[9] = {
    BGRA(120, 130, 150),     /* 0 不用 */
    BGRA(120, 170, 255),     /* 1 */
    BGRA(120, 220, 150),     /* 2 */
    BGRA(255, 130, 130),     /* 3 */
    BGRA(180, 140, 255),     /* 4 */
    BGRA(230, 190, 120),     /* 5 */
    BGRA(120, 220, 220),     /* 6 */
    BGRA(240, 240, 240),     /* 7 */
    BGRA(190, 190, 190),     /* 8 */
};

static int neighbors(int idx, int *out)
{
    int r = idx / MS_N, c = idx % MS_N, n = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) {
                continue;
            }
            int rr = r + dr, cc = c + dc;
            if (rr < 0 || rr >= MS_N || cc < 0 || cc >= MS_N) {
                continue;
            }
            out[n++] = rr * MS_N + cc;
        }
    }
    return n;
}

static int count_mines(int idx)
{
    int nb[8];
    int n = neighbors(idx, nb), cnt = 0;
    for (int i = 0; i < n; i++) {
        if (s_mine[nb[i]]) {
            cnt++;
        }
    }
    return cnt;
}

/* 第一次点完之后才布雷，且 safe 及其邻居不放雷 */
static void place_mines(int safe)
{
    int nb[8];
    int forbid[9];
    int fn = neighbors(safe, nb);
    forbid[0] = safe;
    for (int i = 0; i < fn; i++) {
        forbid[1 + i] = nb[i];
    }
    fn++;

    memset(s_mine, 0, sizeof(s_mine));
    int placed = 0;
    while (placed < MS_MINES) {
        int idx = album_rand_below(MS_N * MS_N);
        int bad = 0;
        for (int i = 0; i < fn; i++) {
            if (forbid[i] == idx) {
                bad = 1;
                break;
            }
        }
        if (bad || s_mine[idx]) {
            continue;
        }
        s_mine[idx] = 1;
        placed++;
    }
}

static void ms_reset(void)
{
    memset(s_mine, 0, sizeof(s_mine));
    memset(s_open_, 0, sizeof(s_open_));
    memset(s_flag_, 0, sizeof(s_flag_));
    s_state = ST_IDLE;
    s_flag_mode = 0;
    s_opened = 0;
    s_mines_left = MS_MINES;
    gs_status("OPEN MODE  -  MINES %d", s_mines_left);
}

static void load_sprites(void)
{
    if (!s_closed)   s_closed = gs_load(NULL, "ms_closed.bmp");
    if (!s_open)     s_open = gs_load(NULL, "ms_open.bmp");
    if (!s_flag)     s_flag = gs_load(NULL, "ms_flag.bmp");
    if (!s_mine_img) s_mine_img = gs_load(NULL, "ms_mine.bmp");
}

/* ------------------------------------------------------------------ 绘制 */
static void ms_render(int active)
{
    uint8_t *fb = display_backbuffer();
    int pitch = display_pitch();
    int w = display_width(), h = display_height();
    int step = G_MS_CELL + G_MS_GAP;
    char buf[64];

    if (!s_page) {
        return;
    }
    load_sprites();
    ui_page_draw_bg(s_page);

    for (int r = 0; r < MS_N; r++) {
        for (int c = 0; c < MS_N; c++) {
            int idx = r * MS_N + c;
            int x = G_MS_X0 + c * step;
            int y = G_MS_Y0 + r * step;
            Image *im;

            if (!s_open_[idx]) {
                im = s_flag_[idx] ? s_flag : s_closed;
                if (im) {
                    image_blit_alpha(fb, pitch, w, h, im, x, y);
                }
                continue;
            }
            if (s_mine[idx]) {
                im = s_mine_img;
                if (im) {
                    image_blit_alpha(fb, pitch, w, h, im, x, y);
                }
                continue;
            }
            im = s_open;
            if (im) {
                image_blit_alpha(fb, pitch, w, h, im, x, y);
            }
            int cnt = count_mines(idx);
            if (cnt > 0) {
                snprintf(buf, sizeof(buf), "%d", cnt);
                draw_text_center(fb, pitch, w, h, x + G_MS_CELL / 2,
                                 y + G_MS_CELL / 2 - 8, buf, NUM_COLOR[cnt], 1);
            }
        }
    }

    if (s_flag_mode) {
        /* 插旗模式下给棋盘描一圈紫边，避免玩家忘了自己在哪个模式 */
        draw_rect(fb, pitch, w, h, G_MS_X0 - 8, G_MS_Y0 - 8,
                  MS_N * step - G_MS_GAP + 16, MS_N * step - G_MS_GAP + 16,
                  BGRA(190, 140, 255));
    }

    if (s_state == ST_LOSE || s_state == ST_WIN) {
        int bw = 460, bh = 116;
        int bx = (w - bw) / 2, by = (h - bh) / 2;
        uint32_t col = (s_state == ST_WIN) ? BGRA(120, 230, 150) : BGRA(255, 120, 120);
        draw_fill_rect(fb, pitch, w, h, bx, by, bw, bh, BGRA(0, 0, 0), 228);
        draw_rect(fb, pitch, w, h, bx, by, bw, bh, col);
        draw_text_center(fb, pitch, w, h, w / 2, by + 20,
                         s_state == ST_WIN ? "YOU WIN !" : "BOOM !", col, 3);
        snprintf(buf, sizeof(buf), "%s  -  点重来再来一局",
                 s_state == ST_WIN ? "全部排完" : "踩到雷了");
        draw_text_center(fb, pitch, w, h, w / 2, by + 72, buf,
                         BGRA(230, 220, 210), 1);
    }

    ui_page_draw_buttons(s_page, active);
    gs_draw_status();
    display_present();
}

/* ------------------------------------------------------------------ 规则 */
static void reveal(int idx)
{
    /* stack[] 的容量正好是格子数，靠 queued[] 证明不会越界：
     * 每个格子最多被压栈一次。改动前没有 queued[]，一个格子会被周围多个
     * 0 邻格重复压栈，实测（20 万局模拟）最多到 131 项，越过 81 项后覆盖
     * 栈上的 canary —— Linux CI 上偶发 "stack smashing detected"，而
     * Windows/MinGW 没有 canary，只会静默写坏内存。 */
    int stack[MS_N * MS_N];
    int queued[MS_N * MS_N] = { 0 };
    int top = 0;

    if (s_open_[idx] || s_flag_[idx]) {
        return;
    }
    stack[top++] = idx;
    queued[idx] = 1;
    while (top > 0) {
        int cur = stack[--top];
        if (s_open_[cur] || s_flag_[cur] || s_mine[cur]) {
            continue;
        }
        s_open_[cur] = 1;
        s_opened++;
        if (count_mines(cur) == 0) {
            int nb[8];
            int n = neighbors(cur, nb);
            for (int i = 0; i < n; i++) {
                int nxt = nb[i];
                if (!queued[nxt] && !s_open_[nxt] && !s_flag_[nxt]) {
                    queued[nxt] = 1;
                    stack[top++] = nxt;
                }
            }
        }
    }
}

static void reveal_all_mines(void)
{
    for (int i = 0; i < MS_N * MS_N; i++) {
        if (s_mine[i]) {
            s_open_[i] = 1;
        }
    }
}

static void ms_lose(void)
{
    s_state = ST_LOSE;
    reveal_all_mines();
    printf("[ms] 踩雷\n");
    gs_status("BOOM!  TAP RESTART");
}

static void ms_check_win(void)
{
    if (s_opened >= MS_N * MS_N - MS_MINES) {
        s_state = ST_WIN;
        printf("[ms] 通关\n");
        gs_status("CLEARED!  TAP RESTART");
    }
}

static void ms_tap(int x, int y)
{
    int step = G_MS_CELL + G_MS_GAP;
    int dx = x - G_MS_X0;
    int dy = y - G_MS_Y0;

    if (s_state == ST_LOSE || s_state == ST_WIN) {
        return;
    }
    if (dx < 0 || dy < 0) {
        return;
    }
    int c = dx / step, r = dy / step;
    if (c >= MS_N || r >= MS_N) {
        return;
    }
    if (dx % step > G_MS_CELL || dy % step > G_MS_CELL) {
        return;                 /* 点在格子间隙 */
    }
    int idx = r * MS_N + c;

    if (s_flag_mode) {
        if (!s_open_[idx] && s_mines_left > 0) {
            s_flag_[idx] = !s_flag_[idx];
            s_mines_left += s_flag_[idx] ? -1 : +1;
        } else if (s_flag_[idx]) {
            s_flag_[idx] = 0;
            s_mines_left++;
        }
        gs_status("FLAG MODE  -  MINES %d", s_mines_left);
    } else {
        if (s_flag_[idx] || s_open_[idx]) {
            return;
        }
        if (s_state == ST_IDLE) {
            place_mines(idx);
            s_state = ST_PLAY;
        }
        if (s_mine[idx]) {
            ms_lose();
        } else {
            reveal(idx);
            ms_check_win();
        }
        if (s_state == ST_PLAY) {
            gs_status("OPEN MODE  -  MINES %d  -  OPENED %d",
                      s_mines_left, s_opened);
        }
    }
    ms_render(-1);
}

static int ms_button(const char *id)
{
    if (strcmp(id, "g_again") == 0) {
        ms_reset();
        return 1;
    }
    if (strcmp(id, "g_extra") == 0) {
        s_flag_mode = !s_flag_mode;
        gs_status("%s MODE  -  MINES %d",
                  s_flag_mode ? "FLAG" : "OPEN", s_mines_left);
        return 1;
    }
    return 0;
}

static void ms_enter(const UiPage *page)
{
    s_page = page;
    ms_reset();
    ms_render(-1);
}

static void ms_tick(long long now_ms)
{
    (void)now_ms;       /* 扫雷没有定时逻辑 */
}

const GameOps game_ms = {
    "ms",
    ms_enter,
    ms_render,
    ms_tap,
    ms_button,
    ms_tick,
};

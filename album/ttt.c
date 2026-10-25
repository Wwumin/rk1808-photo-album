/*
 * ttt.c  —  井字棋(你执 X 先手，AI 执 O)
 *
 * AI 用完整极小化极大搜索。3x3 只有 9 格、总状态不到 55 万，
 * 在 RK1808 上也是一瞬间的事，所以不用评估函数、不用剪枝 ——
 * 搜索到底得到的才是"永不输"。这一点值得写进注释里：
 * 小棋盘上"够用"的启发式反而会让 AI 犯低级错误。
 */

#include <stdio.h>
#include <string.h>

#include "games.h"
#include "display.h"
#include "draw.h"

#define N 3
#define EMPTY 0
#define HUMAN 1
#define AI    2

static const UiPage *s_page = NULL;
static Image *s_x = NULL;
static Image *s_o = NULL;

static int s_cell[N * N];
static int s_over = 0;        /* 0 进行中, 1 人赢, 2 AI赢, 3 平局 */
static int s_win[3] = { -1, -1, -1 };
static int s_busy = 0;        /* 防止 AI 还没落子就重复点击 */

static const int LINES[8][3] = {
    { 0, 1, 2 }, { 3, 4, 5 }, { 6, 7, 8 },
    { 0, 3, 6 }, { 1, 4, 7 }, { 2, 5, 8 },
    { 0, 4, 8 }, { 2, 4, 6 },
};

/* 返回 0 无结果, 1 X 赢, 2 O 赢, 3 平局 */
static int judge(const int *b, int *line)
{
    for (int i = 0; i < 8; i++) {
        int a = LINES[i][0], c = LINES[i][1], d = LINES[i][2];
        if (b[a] != EMPTY && b[a] == b[c] && b[a] == b[d]) {
            if (line) {
                line[0] = a;
                line[1] = c;
                line[2] = d;
            }
            return b[a];
        }
    }
    for (int i = 0; i < 9; i++) {
        if (b[i] == EMPTY) {
            return 0;
        }
    }
    return 3;
}

/*
 * 极小化极大: AI 想最大化分数, 人想最小化。
 * +10-depth 让"早点赢"比"晚点赢"得分高(已经赢了就尽快收工)。
 */
static int minimax(int *b, int turn, int depth)
{
    int line[3];
    int r = judge(b, line);

    if (r == AI) {
        return 10 - depth;
    }
    if (r == HUMAN) {
        return depth - 10;
    }
    if (r == 3) {
        return 0;
    }

    int best = (turn == AI) ? -1000 : 1000;
    for (int i = 0; i < 9; i++) {
        if (b[i] != EMPTY) {
            continue;
        }
        b[i] = turn;
        int v = minimax(b, turn == AI ? HUMAN : AI, depth + 1);
        b[i] = EMPTY;
        if (turn == AI) {
            if (v > best) {
                best = v;
            }
        } else {
            if (v < best) {
                best = v;
            }
        }
    }
    return best;
}

static void ai_move(void)
{
    int best = -1000;
    int best_i = -1;

    for (int i = 0; i < 9; i++) {
        if (s_cell[i] != EMPTY) {
            continue;
        }
        s_cell[i] = AI;
        int v = minimax(s_cell, HUMAN, 0);
        s_cell[i] = EMPTY;
        if (v > best) {
            best = v;
            best_i = i;
        }
    }
    if (best_i >= 0) {
        s_cell[best_i] = AI;
    }
}

static void ttt_reset(void)
{
    memset(s_cell, 0, sizeof(s_cell));
    s_over = 0;
    s_busy = 0;
    s_win[0] = s_win[1] = s_win[2] = -1;
    gs_status("YOU ARE X  -  TAP AN EMPTY CELL");
}

static void load_sprites(void)
{
    if (!s_x) {
        s_x = gs_load(NULL, "ttt_x.bmp");
    }
    if (!s_o) {
        s_o = gs_load(NULL, "ttt_o.bmp");
    }
}

/* ------------------------------------------------------------------ 绘制 */
static void ttt_render(int active)
{
    int x0 = G_TTT_X0, y0 = G_TTT_Y0;
    int step = G_TTT_CELL + G_TTT_GAP;
    uint8_t *fb = display_backbuffer();
    int pitch = display_pitch();
    int w = display_width(), h = display_height();

    if (!s_page) {
        return;
    }
    load_sprites();

    ui_page_draw_bg(s_page);

    /* 已落子的格子 */
    for (int r = 0; r < N; r++) {
        for (int c = 0; c < N; c++) {
            int v = s_cell[r * N + c];
            if (v == EMPTY) {
                continue;
            }
            Image *im = (v == HUMAN) ? s_x : s_o;
            if (im) {
                image_blit_alpha(fb, pitch, w, h, im, x0 + c * step, y0 + r * step);
            }
        }
    }

    /* 胜利线高亮 */
    if (s_over == 1 || s_over == 2) {
        for (int k = 0; k < 3; k++) {
            int idx = s_win[k];
            if (idx < 0) {
                continue;
            }
            int r = idx / N, c = idx % N;
            draw_rect(fb, pitch, w, h, x0 + c * step + 2, y0 + r * step + 2,
                      G_TTT_CELL - 4, G_TTT_CELL - 4, BGRA(120, 255, 160));
        }
    }

    ui_page_draw_buttons(s_page, active);
    gs_draw_status();
    display_present();
}

/* ------------------------------------------------------------------ 交互 */
static int cell_at(int x, int y)
{
    int step = G_TTT_CELL + G_TTT_GAP;
    int dx = x - G_TTT_X0;
    int dy = y - G_TTT_Y0;
    if (dx < 0 || dy < 0) {
        return -1;
    }
    int c = dx / step, r = dy / step;
    if (c >= N || r >= N) {
        return -1;
    }
    /* 落在格间距里不算 */
    if (dx % step > G_TTT_CELL || dy % step > G_TTT_CELL) {
        return -1;
    }
    return r * N + c;
}

static void ttt_tap(int x, int y)
{
    int idx = cell_at(x, y);
    int line[3];

    if (idx < 0 || s_over != 0 || s_busy || s_cell[idx] != EMPTY) {
        return;
    }

    s_cell[idx] = HUMAN;
    s_over = judge(s_cell, line);
    if (s_over) {
        memcpy(s_win, line, sizeof(s_win));
    } else {
        s_busy = 1;
        ai_move();
        s_busy = 0;
        s_over = judge(s_cell, line);
        if (s_over) {
            memcpy(s_win, line, sizeof(s_win));
        }
    }

    switch (s_over) {
    case 1:  gs_status("YOU WIN!  TAP RESTART"); printf("[ttt] 玩家赢\n"); break;
    case 2:  gs_status("AI WINS  -  TAP RESTART");  printf("[ttt] AI 赢\n");  break;
    case 3:  gs_status("DRAW  -  TAP RESTART");     printf("[ttt] 平局\n");   break;
    default: gs_status("AI MOVED  -  YOUR TURN");      break;
    }
    ttt_render(-1);
}

static int ttt_button(const char *id)
{
    if (strcmp(id, "g_again") == 0) {
        ttt_reset();
        return 1;
    }
    return 0;
}

static void ttt_enter(const UiPage *page)
{
    s_page = page;
    ttt_reset();
    ttt_render(-1);
}

static void ttt_tick(long long now_ms)
{
    (void)now_ms;       /* 井字棋无定时逻辑 */
}

const GameOps game_ttt = {
    "ttt",
    ttt_enter,
    ttt_render,
    ttt_tap,
    ttt_button,
    ttt_tick,
};

/*
 * xq.c  —  中国象棋(9 列 x 10 行，你执红先手，AI 执黑)
 *
 * 规则实现范围(够下完一整局):
 *   帅/将: 宫内直走一格 + "双将照面"判定
 *   仕/士: 宫内斜走一格
 *   相/象: 田字，塞象眼，不过河
 *   马:    日字，蹩马腿
 *   车:    直线任意格
 *   炮:    直走，吃子必须隔一个"炮架"
 *   兵/卒: 过河前只能向前，过河后可左右
 *
 * AI: 2 层(黑走一步 + 红方最佳应手)的极小化极大，叶子用子力评估。
 *     深度 2 就够它不白送子了；再深在 RK1808 上会让点击明显卡顿，
 *     这个取舍比"棋力高一点"更重要。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "games.h"
#include "display.h"
#include "draw.h"

#define XQ_ROWS 10
#define XQ_COLS 9

#define SIDE_RED   1
#define SIDE_BLACK 2

#define K 1
#define A 2
#define B 3
#define N 4
#define R 5
#define C 6
#define P 7

#define CODE(side, type) ((side) * 10 + (type))
#define SIDE_OF(code)    ((code) / 10)
#define TYPE_OF(code)    ((code) % 10)

typedef struct {
    int fr, fc, tr, tc;
} Move;

static const UiPage *s_page = NULL;
static Image *s_piece[3][8];        /* [side][type] */

static int s_bd[XQ_ROWS][XQ_COLS];
static int s_sel_r = -1, s_sel_c = -1;
static int s_turn;                  /* SIDE_RED / SIDE_BLACK */
static int s_over;                  /* 0 进行, 1 红胜, 2 黑胜 */
static int s_thinking;

static const char *RED_NAME[8]  = { "", "shuai", "shi", "xiang", "ma", "ju", "pao", "bing" };
static const char *BLACK_NAME[8] = { "", "jiang", "shi", "xiang", "ma", "ju", "pao", "zu" };

static const int PIECE_VALUE[8] = { 0, 10000, 200, 200, 400, 900, 450, 100 };

/* ------------------------------------------------------------------ 棋盘 */
static void xq_reset(void)
{
    static const int back_row[XQ_COLS] = { R, N, B, A, K, A, B, N, R };

    memset(s_bd, 0, sizeof(s_bd));
    for (int c = 0; c < XQ_COLS; c++) {
        s_bd[0][c] = CODE(SIDE_BLACK, back_row[c]);
        s_bd[9][c] = CODE(SIDE_RED, back_row[c]);
    }
    s_bd[2][1] = CODE(SIDE_BLACK, C);
    s_bd[2][7] = CODE(SIDE_BLACK, C);
    s_bd[3][0] = s_bd[3][2] = s_bd[3][4] = s_bd[3][6] = s_bd[3][8] = CODE(SIDE_BLACK, P);

    s_bd[7][1] = CODE(SIDE_RED, C);
    s_bd[7][7] = CODE(SIDE_RED, C);
    s_bd[6][0] = s_bd[6][2] = s_bd[6][4] = s_bd[6][6] = s_bd[6][8] = CODE(SIDE_RED, P);

    s_sel_r = s_sel_c = -1;
    s_turn = SIDE_RED;
    s_over = 0;
    s_thinking = 0;
    gs_status("RED FIRST  -  TAP YOUR PIECE");
}

static int in_board(int r, int c)
{
    return r >= 0 && r < XQ_ROWS && c >= 0 && c < XQ_COLS;
}

static int in_palace(int side, int r, int c)
{
    if (c < 3 || c > 5) {
        return 0;
    }
    return side == SIDE_RED ? (r >= 7 && r <= 9) : (r >= 0 && r <= 2);
}

static int own_half(int side, int r)
{
    return side == SIDE_RED ? (r >= 5) : (r <= 4);
}

static int crossed_river(int side, int r)
{
    return side == SIDE_RED ? (r <= 4) : (r >= 5);
}

static int forward_dir(int side)
{
    return side == SIDE_RED ? -1 : +1;
}

/* 某一块棋子能走到哪儿(不考虑"送将"，那个由 xq_legal 再筛) */
static int gen_piece(int r, int c, Move *out, int max)
{
    int code = s_bd[r][c];
    if (code == 0) {
        return 0;
    }
    int side = SIDE_OF(code), type = TYPE_OF(code);
    int n = 0;

/* 宏参数用 tr_/tc_，千万别用 tr/tc：
 * 宏体会展开成 out[n].tr / out[n].tc，参数名撞上结构体字段名
 * 就会被预处理器替换掉 —— 传 (tr,tc) 时恰好替换成一样的东西看不出来，
 * 传 (fr,c) 就变成 out[n].fr / out[n].c 编译报"没有成员 c"。 */
#define PUSH(tr_, tc_)                                                \
    do {                                                              \
        int _tr = (tr_), _tc = (tc_);                                 \
        if (in_board(_tr, _tc)) {                                     \
            int _t = s_bd[_tr][_tc];                                  \
            if (_t == 0 || SIDE_OF(_t) != side) {                     \
                if (out && n < max) {                                 \
                    out[n].fr = r; out[n].fc = c;                     \
                    out[n].tr = _tr; out[n].tc = _tc;                 \
                }                                                     \
                n++;                                                  \
            }                                                         \
        }                                                             \
    } while (0)

    switch (type) {
    case K:
        for (int d = 0; d < 4; d++) {
            static const int DR[4] = { -1, 1, 0, 0 }, DC[4] = { 0, 0, -1, 1 };
            int tr = r + DR[d], tc = c + DC[d];
            if (in_palace(side, tr, tc)) {
                PUSH(tr, tc);
            }
        }
        break;

    case A:
        for (int d = 0; d < 4; d++) {
            static const int DR[4] = { -1, -1, 1, 1 }, DC[4] = { -1, 1, -1, 1 };
            int tr = r + DR[d], tc = c + DC[d];
            if (in_palace(side, tr, tc)) {
                PUSH(tr, tc);
            }
        }
        break;

    case B:
        for (int d = 0; d < 4; d++) {
            static const int DR[4] = { -2, -2, 2, 2 }, DC[4] = { -2, 2, -2, 2 };
            int tr = r + DR[d], tc = c + DC[d];
            int er = r + DR[d] / 2, ec = c + DC[d] / 2;
            if (in_board(tr, tc) && own_half(side, tr) && s_bd[er][ec] == 0) {
                PUSH(tr, tc);
            }
        }
        break;

    case N:
        for (int d = 0; d < 8; d++) {
            static const int DR[8] = { -2, -2, 2, 2, -1, 1, -1, 1 };
            static const int DC[8] = { -1, 1, -1, 1, -2, -2, 2, 2 };
            int tr = r + DR[d], tc = c + DC[d];
            int lr = r + (DR[d] == -2 ? -1 : (DR[d] == 2 ? 1 : 0));
            int lc = c + (DC[d] == -2 ? -1 : (DC[d] == 2 ? 1 : 0));
            if (in_board(tr, tc) && s_bd[lr][lc] == 0) {
                PUSH(tr, tc);
            }
        }
        break;

    case R:
        for (int d = 0; d < 4; d++) {
            static const int DR[4] = { -1, 1, 0, 0 }, DC[4] = { 0, 0, -1, 1 };
            int tr = r + DR[d], tc = c + DC[d];
            while (in_board(tr, tc)) {
                int t = s_bd[tr][tc];
                if (t == 0) {
                    PUSH(tr, tc);
                } else {
                    if (SIDE_OF(t) != side) {
                        PUSH(tr, tc);
                    }
                    break;
                }
                tr += DR[d];
                tc += DC[d];
            }
        }
        break;

    case C:
        for (int d = 0; d < 4; d++) {
            static const int DR[4] = { -1, 1, 0, 0 }, DC[4] = { 0, 0, -1, 1 };
            int tr = r + DR[d], tc = c + DC[d];
            int jumped = 0;
            while (in_board(tr, tc)) {
                int t = s_bd[tr][tc];
                if (!jumped) {
                    if (t == 0) {
                        PUSH(tr, tc);
                    } else {
                        jumped = 1;         /* 第一个挡子 = 炮架 */
                    }
                } else {
                    if (t != 0) {
                        if (SIDE_OF(t) != side) {
                            PUSH(tr, tc);
                        }
                        break;
                    }
                }
                tr += DR[d];
                tc += DC[d];
            }
        }
        break;

    case P: {
        int fr = r + forward_dir(side);
        PUSH(fr, c);
        if (crossed_river(side, r)) {
            PUSH(r, c - 1);
            PUSH(r, c + 1);
        }
        break;    }

    default:
        break;
    }
#undef PUSH
    return n;
}

static int gen_all(int side, Move *out, int max)
{
    int n = 0;
    for (int r = 0; r < XQ_ROWS; r++) {
        for (int c = 0; c < XQ_COLS; c++) {
            if (s_bd[r][c] == 0 || SIDE_OF(s_bd[r][c]) != side) {
                continue;
            }
            Move tmp[32];
            int m = gen_piece(r, c, tmp, 32);
            for (int i = 0; i < m; i++) {
                if (out && n < max) {
                    out[n] = tmp[i];
                }
                n++;
            }
        }
    }
    return n;
}

/* 两将照面(中间无子) */
static int generals_face(void)
{
    int rk = -1, rkr = -1, rkc = -1;
    int bk = -1, bkr = -1, bkc = -1;

    for (int r = 0; r < XQ_ROWS; r++) {
        for (int c = 0; c < XQ_COLS; c++) {
            if (s_bd[r][c] == CODE(SIDE_RED, K)) { rk = 1; rkr = r; rkc = c; }
            if (s_bd[r][c] == CODE(SIDE_BLACK, K)) { bk = 1; bkr = r; bkc = c; }
        }
    }
    if (!rk || !bk || rkc != bkc) {
        return 0;
    }
    int lo = rkr < bkr ? rkr : bkr;
    int hi = rkr < bkr ? bkr : rkr;
    for (int r = lo + 1; r < hi; r++) {
        if (s_bd[r][rkc] != 0) {
            return 0;
        }
    }
    return 1;
}

static int king_pos(int side, int *rr, int *cc)
{
    for (int r = 0; r < XQ_ROWS; r++) {
        for (int c = 0; c < XQ_COLS; c++) {
            if (s_bd[r][c] == CODE(side, K)) {
                *rr = r;
                *cc = c;
                return 1;
            }
        }
    }
    return 0;
}

/* 走子之后自己的将/帅是否安全(被吃 或 照面) */
static int king_unsafe(int side)
{
    int kr, kc;
    if (!king_pos(side, &kr, &kc)) {
        return 1;
    }
    if (generals_face()) {
        return 1;
    }
    int enemy = (side == SIDE_RED) ? SIDE_BLACK : SIDE_RED;
    Move mv[256];
    int n = gen_all(enemy, mv, 256);
    for (int i = 0; i < n; i++) {
        if (mv[i].tr == kr && mv[i].tc == kc) {
            return 1;
        }
    }
    return 0;
}

static int xq_legal(int fr, int fc, int tr, int tc)
{
    int code = s_bd[fr][fc];
    int saved = s_bd[tr][tc];

    s_bd[tr][tc] = code;
    s_bd[fr][fc] = 0;
    int bad = king_unsafe(SIDE_OF(code));
    s_bd[fr][fc] = code;
    s_bd[tr][tc] = saved;

    return !bad;
}

/* ------------------------------------------------------------------ AI */
static int eval_board(void)
{
    int score = 0;
    for (int r = 0; r < XQ_ROWS; r++) {
        for (int c = 0; c < XQ_COLS; c++) {
            int code = s_bd[r][c];
            if (!code) {
                continue;
            }
            int v = PIECE_VALUE[TYPE_OF(code)];
            int pos = 0;
            if (TYPE_OF(code) == P) {
                /* 兵越靠前越值钱 */
                pos = (SIDE_OF(code) == SIDE_RED) ? (9 - r) : r;
            }
            if (SIDE_OF(code) == SIDE_BLACK) {
                score += v + pos * 4;
            } else {
                score -= v + pos * 4;
            }
        }
    }
    return score;
}

static void do_move(const Move *m, int *cap)
{
    *cap = s_bd[m->tr][m->tc];
    s_bd[m->tr][m->tc] = s_bd[m->fr][m->fc];
    s_bd[m->fr][m->fc] = 0;
}

static void undo_move(const Move *m, int cap)
{
    s_bd[m->fr][m->fc] = s_bd[m->tr][m->tc];
    s_bd[m->tr][m->tc] = cap;
}

/* 黑方走完这一手后，红方最佳应手带来的局面分 */
static int reply_score(void)
{
    Move mv[256];
    int n = gen_all(SIDE_RED, mv, 256);
    int best = 100000;      /* 红方让分最小 */

    for (int i = 0; i < n; i++) {
        if (mv[i].tr > 9 || mv[i].tc > 8) {
            continue;
        }
        int cap;
        do_move(&mv[i], &cap);
        int s = eval_board();
        undo_move(&mv[i], cap);
        if (s < best) {
            best = s;
        }
    }
    return (n == 0) ? eval_board() : best;
}

static void ai_move(void)
{
    Move mv[256];
    int n = gen_all(SIDE_BLACK, mv, 256);
    int best_v = -1000000;
    int best_i = -1;

    for (int i = 0; i < n; i++) {
        int cap;
        do_move(&mv[i], &cap);
        int v;
        if (TYPE_OF(cap) == K) {
            v = 1000000;                    /* 直接吃将 */
        } else {
            v = reply_score();
        }
        undo_move(&mv[i], cap);
        if (v > best_v) {
            best_v = v;
            best_i = i;
        }
    }

    if (best_i < 0) {
        return;
    }
    int cap;
    do_move(&mv[best_i], &cap);
    printf("[xq] AI 走 (%d,%d)->(%d,%d)\n",
           mv[best_i].fr, mv[best_i].fc, mv[best_i].tr, mv[best_i].tc);

    if (TYPE_OF(cap) == K) {
        s_over = 2;
        gs_status("BLACK WINS  -  TAP RESTART");
    }
}

/* ------------------------------------------------------------------ 绘制 */
static void load_sprites(void)
{
    for (int t = 1; t <= 7; t++) {
        if (!s_piece[SIDE_RED][t]) {
            char name[32];
            snprintf(name, sizeof(name), "xq_r_%s.bmp", RED_NAME[t]);
            s_piece[SIDE_RED][t] = gs_load(NULL, name);
        }
        if (!s_piece[SIDE_BLACK][t]) {
            char name[32];
            snprintf(name, sizeof(name), "xq_b_%s.bmp", BLACK_NAME[t]);
            s_piece[SIDE_BLACK][t] = gs_load(NULL, name);
        }
    }
}

static void cell_center(int r, int c, int *x, int *y)
{
    *x = G_XQ_X0 + c * G_XQ_CELL;
    *y = G_XQ_Y0 + r * G_XQ_CELL;
}

static void xq_render(int active)
{
    uint8_t *fb = display_backbuffer();
    int pitch = display_pitch();
    int w = display_width(), h = display_height();
    int half = G_XQ_CELL / 2;

    if (!s_page) {
        return;
    }
    load_sprites();
    ui_page_draw_bg(s_page);

    /* 选中框 + 可走点 */
    if (s_sel_r >= 0) {
        int x, y;
        cell_center(s_sel_r, s_sel_c, &x, &y);
        draw_rect(fb, pitch, w, h, x - half - 3, y - half - 3,
                  G_XQ_CELL + 6, G_XQ_CELL + 6, BGRA(120, 255, 150));

        Move mv[64];
        int n = gen_piece(s_sel_r, s_sel_c, mv, 64);
        for (int i = 0; i < n; i++) {
            if (!xq_legal(mv[i].fr, mv[i].fc, mv[i].tr, mv[i].tc)) {
                continue;
            }
            int tx, ty;
            cell_center(mv[i].tr, mv[i].tc, &tx, &ty);
            if (s_bd[mv[i].tr][mv[i].tc] == 0) {
                draw_fill_rect(fb, pitch, w, h, tx - 6, ty - 6, 12, 12,
                               BGRA(140, 255, 170), 190);
            } else {
                draw_rect(fb, pitch, w, h, tx - half - 2, ty - half - 2,
                          G_XQ_CELL + 4, G_XQ_CELL + 4, BGRA(255, 170, 130));
            }
        }
    }

    /* 棋子 */
    for (int r = 0; r < XQ_ROWS; r++) {
        for (int c = 0; c < XQ_COLS; c++) {
            int code = s_bd[r][c];
            if (!code) {
                continue;
            }
            Image *im = s_piece[SIDE_OF(code)][TYPE_OF(code)];
            if (!im) {
                continue;
            }
            int x, y;
            cell_center(r, c, &x, &y);
            image_blit_alpha(fb, pitch, w, h, im, x - im->w / 2, y - im->h / 2);
        }
    }

    ui_page_draw_buttons(s_page, active);
    gs_draw_status();
    display_present();
}

/* ------------------------------------------------------------------ 交互 */
static int pick_cell(int x, int y, int *rr, int *cc)
{
    int r = (y - G_XQ_Y0 + G_XQ_CELL / 2) / G_XQ_CELL;
    int c = (x - G_XQ_X0 + G_XQ_CELL / 2) / G_XQ_CELL;

    if (r < 0 || r >= XQ_ROWS || c < 0 || c >= XQ_COLS) {
        return 0;
    }
    /* 离交叉点太远不算 */
    int cx = G_XQ_X0 + c * G_XQ_CELL, cy = G_XQ_Y0 + r * G_XQ_CELL;
    if (abs(x - cx) > G_XQ_CELL / 2 || abs(y - cy) > G_XQ_CELL / 2) {
        return 0;
    }
    *rr = r;
    *cc = c;
    return 1;
}

static void after_move_msg(void)
{
    if (s_over) {
        return;
    }
    int kr, kc;
    if (king_pos(SIDE_BLACK, &kr, &kc) && king_unsafe(SIDE_BLACK)) {
        gs_status("CHECK!  BLACK IN CHECK");
    } else if (king_pos(SIDE_RED, &kr, &kc) && king_unsafe(SIDE_RED)) {
        gs_status("YOUR KING IS IN CHECK");
    } else {
        gs_status("RED TO MOVE  -  TAP YOUR PIECE");
    }
}

static void xq_tap(int x, int y)
{
    int r, c;

    if (s_over || s_thinking) {
        return;
    }
    if (!pick_cell(x, y, &r, &c)) {
        return;
    }

    /* 已选中: 点可走点则走子 */
    if (s_sel_r >= 0) {
        if (s_sel_r == r && s_sel_c == c) {
            s_sel_r = s_sel_c = -1;
            gs_status("SELECTION CLEARED");
            xq_render(-1);
            return;
        }
        if (xq_legal(s_sel_r, s_sel_c, r, c)) {
            int cap = s_bd[r][c];
            s_bd[r][c] = s_bd[s_sel_r][s_sel_c];
            s_bd[s_sel_r][s_sel_c] = 0;
            printf("[xq] 红方 (%d,%d)->(%d,%d)\n", s_sel_r, s_sel_c, r, c);
            s_sel_r = s_sel_c = -1;

            if (TYPE_OF(cap) == K) {
                s_over = 1;
                gs_status("RED WINS!  TAP RESTART");
                xq_render(-1);
                return;
            }

            s_thinking = 1;
            gs_status("AI THINKING...");
            xq_render(-1);
            ai_move();
            s_thinking = 0;
            if (s_over) {
                xq_render(-1);
                return;
            }
            after_move_msg();
            xq_render(-1);
            return;
        }
    }

    /* 选自己的棋子 */
    if (s_bd[r][c] != 0 && SIDE_OF(s_bd[r][c]) == SIDE_RED) {
        s_sel_r = r;
        s_sel_c = c;
        gs_status("PICKED (%d,%d)  -  TAP A GREEN DOT", r, c);
    } else {
        s_sel_r = s_sel_c = -1;
        gs_status("RED TO MOVE  -  TAP YOUR PIECE");
    }
    xq_render(-1);
}

static int xq_button(const char *id)
{
    if (strcmp(id, "g_again") == 0) {
        xq_reset();
        return 1;
    }
    return 0;
}

static void xq_enter(const UiPage *page)
{
    s_page = page;
    xq_reset();
    xq_render(-1);
}

static void xq_tick(long long now_ms)
{
    (void)now_ms;
}

const GameOps game_xq = {
    "xq",
    xq_enter,
    xq_render,
    xq_tap,
    xq_button,
    xq_tick,
};

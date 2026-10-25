/*
 * game2048.c  —  2048 逻辑实现 (不依赖屏幕，可单独测试)
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "game2048.h"

/*
 * 每步都打印的话，随机对局跑几百步会刷屏，根本看不清。
 * 默认只打印"新开一局 / 悔棋 / 胜利 / 结束"这类关键事件，
 * 需要逐步排查时把环境变量 G2048_VERBOSE 设成 1。
 */
static int verbose_on(void)
{
    static int cached = -1;
    if (cached < 0) {
        const char *e = getenv("G2048_VERBOSE");
        cached = (e && *e && *e != '0') ? 1 : 0;
    }
    return cached;
}

static int  g_board[G2048_N][G2048_N];
static int  g_prev_board[G2048_N][G2048_N];
static int  g_prev_score;
static int  g_undo_left;

static int  g_score;
static int  g_best;
static int  g_won;
static int  g_over;
static int  g_has_snapshot;      /* 是否有一个"可以悔回去"的历史局面 */

/* ------------------------------------------------------------------ 随机 */
static int  g_seeded = 0;
static unsigned g_seed_override = 0;

/* 给测试用: 固定随机种子，让随机补牌可复现 */
void g2048_set_seed(unsigned seed)
{
    g_seed_override = seed;
    g_seeded = 0;
}

static int rnd_below(int n)
{
    /* 用标准 rand()/srand(): random()/srandom() 在 Windows 上编不了，
     * 这个文件要能在 PC 上跑单测，所以选可移植的那对。
     * 2048 对随机质量要求很低，rand() 足够。 */
    if (!g_seeded) {
        unsigned seed = g_seed_override ? g_seed_override : (unsigned)time(NULL);
        srand(seed);
        g_seeded = 1;
    }
    return (int)(rand() % n);
}

/* ------------------------------------------------------------------ 工具 */
static int board_empty_count(void)
{
    int c = 0;
    for (int r = 0; r < G2048_N; r++) {
        for (int col = 0; col < G2048_N; col++) {
            if (g_board[r][col] == 0) {
                c++;
            }
        }
    }
    return c;
}

static void spawn_tile(void)
{
    int empty = board_empty_count();
    if (empty <= 0) {
        return;
    }
    int pick = rnd_below(empty);
    for (int r = 0; r < G2048_N; r++) {
        for (int col = 0; col < G2048_N; col++) {
            if (g_board[r][col] != 0) {
                continue;
            }
            if (pick-- == 0) {
                /* 90% 出 2，10% 出 4 —— 和原版 2048 一致 */
                g_board[r][col] = (rnd_below(10) == 0) ? 2 : 1;
                return;
            }
        }
    }
}

static int can_move(void)
{
    if (board_empty_count() > 0) {
        return 1;
    }
    for (int r = 0; r < G2048_N; r++) {
        for (int col = 0; col < G2048_N; col++) {
            int v = g_board[r][col];
            if (col + 1 < G2048_N && g_board[r][col + 1] == v) {
                return 1;
            }
            if (r + 1 < G2048_N && g_board[r + 1][col] == v) {
                return 1;
            }
        }
    }
    return 0;
}

static void refresh_state(void)
{
    for (int r = 0; r < G2048_N; r++) {
        for (int col = 0; col < G2048_N; col++) {
            if (g_board[r][col] >= G2048_WIN_EXP) {
                g_won = 1;
            }
        }
    }
    g_over = can_move() ? 0 : 1;
    if (g_score > g_best) {
        g_best = g_score;
    }
}

/* ------------------------------------------------------------------ 单行合并 */
/*
 * 把一行(长度为 N)朝"索引 0 的方向"压缩合并。
 * 返回这一行是否有变化。score_gain 累加本次合并得分。
 *
 * 例: [2,2,4,0] -> [4,4,0,0] 得分 +4
 *     [2,2,2,2] -> [4,4,0,0] 得分 +8   (一次移动中每张牌只合并一次)
 */
static int collapse_line(int *line, int *score_gain)
{
    int out[G2048_N];
    int n = 0, changed = 0;

    /* 1. 去掉空格 */
    for (int i = 0; i < G2048_N; i++) {
        if (line[i] != 0) {
            out[n++] = line[i];
        }
    }
    /* 2. 相邻相同则合并(合并后指数 +1) */
    for (int i = 0; i + 1 < n; i++) {
        if (out[i] == out[i + 1]) {
            out[i] += 1;
            *score_gain += (1 << out[i]);
            for (int j = i + 1; j + 1 < n; j++) {
                out[j] = out[j + 1];
            }
            n--;
            /*
             * 这里绝对不能再写 i++。
             * 合并出来的新牌本回合不能再参与合并，靠的是"指针不前进";
             * 如果写成 i++，就会把刚合出来的牌再跳过一格，
             * 于是 [2,2,2,2] 会变成 [4,2,2] 而不是 [4,4]。
             */
        }
    }
    /* 3. 后面补零 */
    while (n < G2048_N) {
        out[n++] = 0;
    }
    /* 4. 与原行比较 */
    for (int i = 0; i < G2048_N; i++) {
        if (line[i] != out[i]) {
            changed = 1;
        }
        line[i] = out[i];
    }
    return changed;
}

/* ------------------------------------------------------------------ 移动 */
int g2048_move(G2048Dir dir)
{
    int snapshot[G2048_N][G2048_N];
    int gain = 0, changed = 0;

    if (g_over) {
        return 0;
    }

    /* 存档用于悔棋。没得悔就别存，避免把 undo 额度浪费在无效移动上 */
    memcpy(snapshot, g_board, sizeof(g_board));

    if (dir == DIR_LEFT || dir == DIR_RIGHT) {
        for (int r = 0; r < G2048_N; r++) {
            int line[G2048_N];
            for (int c = 0; c < G2048_N; c++) {
                /* 向右等于把行反过来向左处理 */
                line[c] = g_board[r][(dir == DIR_LEFT) ? c : (G2048_N - 1 - c)];
            }
            changed |= collapse_line(line, &gain);
            for (int c = 0; c < G2048_N; c++) {
                g_board[r][(dir == DIR_LEFT) ? c : (G2048_N - 1 - c)] = line[c];
            }
        }
    } else {
        for (int c = 0; c < G2048_N; c++) {
            int line[G2048_N];
            for (int r = 0; r < G2048_N; r++) {
                line[r] = g_board[(dir == DIR_UP) ? r : (G2048_N - 1 - r)][c];
            }
            changed |= collapse_line(line, &gain);
            for (int r = 0; r < G2048_N; r++) {
                g_board[(dir == DIR_UP) ? r : (G2048_N - 1 - r)][c] = line[r];
            }
        }
    }

    if (!changed) {
        return 0;               /* 走不动，不补牌也不消耗悔棋 */
    }

    memcpy(g_prev_board, snapshot, sizeof(snapshot));
    g_prev_score = g_score;
    g_has_snapshot = 1;
    g_score += gain;
    spawn_tile();
    refresh_state();

    if (verbose_on()) {
        printf("[2048] %s  得分 +%d -> %d\n",
               dir == DIR_UP ? "上" : dir == DIR_DOWN ? "下" :
               dir == DIR_LEFT ? "左" : "右", gain, g_score);
    }
    return 1;
}

/* ------------------------------------------------------------------ 查询 */
int g2048_score(void)     { return g_score; }
int g2048_best(void)      { return g_best; }
int g2048_won(void)       { return g_won; }
int g2048_over(void)      { return g_over; }
int g2048_undo_left(void) { return g_undo_left; }

int g2048_cell(int row, int col)
{
    if (row < 0 || row >= G2048_N || col < 0 || col >= G2048_N) {
        return 0;
    }
    return g_board[row][col];
}

int g2048_undo(void)
{
    /*
     * 必须用 g_has_snapshot 判断，而不是 g_score == 0。
     * 因为"得分还是 0"并不代表"没走过棋": 第一步完全可能没有合并、
     * 不加分，但仍是一个可以悔回去的局面。之前用分数判断，
     * 会出现"走了一步却悔不了"的情况(单测里就踩到了)。
     */
    if (g_undo_left <= 0 || !g_has_snapshot) {
        return 0;
    }
    memcpy(g_board, g_prev_board, sizeof(g_board));
    g_score = g_prev_score;
    g_undo_left--;
    g_won = 0;
    g_over = 0;
    g_has_snapshot = 0;         /* 一步只能悔一次 */
    refresh_state();
    if (verbose_on()) {
        printf("[2048] 悔棋，剩 %d 次，得分 %d\n", g_undo_left, g_score);
    }
    return 1;
}

/* ------------------------------------------------------------------ 开局 */
void g2048_new(void)
{
    memset(g_board, 0, sizeof(g_board));
    memset(g_prev_board, 0, sizeof(g_prev_board));
    g_prev_score = 0;
    g_score = 0;
    g_won = 0;
    g_over = 0;
    g_undo_left = G2048_UNDO_MAX;
    g_has_snapshot = 0;

    spawn_tile();
    spawn_tile();
    refresh_state();
    if (verbose_on()) {
        printf("[2048] 新开一局\n");
    }
}

void g2048_set_board(const int cells[G2048_N][G2048_N])
{
    memcpy(g_board, cells, sizeof(g_board));
    memcpy(g_prev_board, cells, sizeof(g_prev_board));
    g_prev_score = g_score;
    g_undo_left = G2048_UNDO_MAX;
    g_has_snapshot = 0;         /* 摆出来的局面没有历史，不能悔 */
    refresh_state();
}

/* ------------------------------------------------------------------ 手势 */
int g2048_swipe_to_dir(int x0, int y0, int x1, int y1, G2048Dir *out)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;

    /* 位移太小当成"点击"，不当作滑动，避免误触就翻一屏 */
    if (adx < 40 && ady < 40) {
        return 0;
    }
    if (adx >= ady) {
        *out = (dx > 0) ? DIR_RIGHT : DIR_LEFT;
    } else {
        *out = (dy > 0) ? DIR_DOWN : DIR_UP;
    }
    return 1;
}

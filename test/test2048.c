/*
 * test/test2048.c  —  2048 逻辑单元测试 (PC 上跑，不碰屏幕)
 *
 * 编译: mingw32-make -f Makefile unit
 * 它把 game2048.c 内部的合并规则逐条验证:
 *   空格压缩 / 相邻合并且每张牌一回合只合一次 / 无效移动不补牌 / 悔棋 / 胜负判定
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "game2048.h"

static int g_fail = 0;

#define CHECK(cond, fmt, ...) do {                                        \
    if (!(cond)) {                                                        \
        printf("  [FAIL] " fmt "\n", ##__VA_ARGS__);                      \
        g_fail++;                                                         \
    } else {                                                              \
        printf("  [ok]   " fmt "\n", ##__VA_ARGS__);                      \
    }                                                                     \
} while (0)

/* 把棋盘打印成 2 的幂(排错用) */
static void dump(void)
{
    for (int r = 0; r < G2048_N; r++) {
        printf("      ");
        for (int c = 0; c < G2048_N; c++) {
            int e = g2048_cell(r, c);
            if (e == 0) {
                printf("     .");
            } else {
                printf("%6d", 1 << e);
            }
        }
        printf("\n");
    }
}

/*
 * 确定性验证合并规则。
 *
 * 做法: 用 g2048_set_board() 直接摆出指定局面，走一步后断言第一行。
 * 关键点: move() 成功时会随机补一张新牌，所以**空格位置不可断言**。
 *         期望值里用 -1 表示"这一格不管"(可能是补出来的牌)。
 *         受影响的格子(移动后落在哪几张牌)才是真正要断言的。
 */
static void expect_merge(const char *name, int line[G2048_N],
                         int e0, int e1, int e2, int e3)
{
    int board[G2048_N][G2048_N];
    int want[G2048_N] = { e0, e1, e2, e3 };
    int got[G2048_N];
    int ok = 1;

    memset(board, 0, sizeof(board));
    for (int i = 0; i < G2048_N; i++) {
        board[0][i] = line[i];              /* 只摆第一行，其他行留空不干扰 */
    }
    g2048_set_board(board);
    g2048_move(DIR_LEFT);
    for (int i = 0; i < G2048_N; i++) {
        got[i] = g2048_cell(0, i);
        if (want[i] >= 0 && got[i] != want[i]) {
            ok = 0;
        }
    }

    printf("      %-16s [%d,%d,%d,%d] 左移 -> ", name,
           line[0] ? 1 << line[0] : 0, line[1] ? 1 << line[1] : 0,
           line[2] ? 1 << line[2] : 0, line[3] ? 1 << line[3] : 0);
    for (int i = 0; i < G2048_N; i++) {
        printf("%d%s", got[i] ? (1 << got[i]) : 0, i + 1 < G2048_N ? "," : "");
    }
    printf("   期望 ");
    for (int i = 0; i < G2048_N; i++) {
        if (want[i] < 0) {
            printf("*%s", i + 1 < G2048_N ? "," : "");
        } else {
            printf("%d%s", want[i] ? (1 << want[i]) : 0, i + 1 < G2048_N ? "," : "");
        }
    }
    printf("  (* = 可能是随机补的牌)   %s\n", ok ? "OK" : "<-- 不符");
    if (!ok) {
        g_fail++;
    }
}

static void test_rules(void)
{
    printf("\n== 规则验证 ==\n");

    /* ---- 合并规则(确定性) ---- */
    printf("\n  合并规则 (期望里的 * 表示可能是随机补出来的牌):\n");
    {
        int l1[G2048_N] = { 1, 1, 0, 0 };      /* 2 2 _ _  -> 4 * * * */
        expect_merge("两张 2 合并", l1, 2, -1, -1, -1);
        int l2[G2048_N] = { 1, 1, 1, 1 };      /* 2 2 2 2  -> 4 4 * * */
        expect_merge("四张 2 分两组", l2, 2, 2, -1, -1);
        int l3[G2048_N] = { 1, 1, 2, 0 };      /* 2 2 4 _  -> 4 4 * * */
        expect_merge("合出来的不再连锁", l3, 2, 2, -1, -1);
        int l4[G2048_N] = { 1, 0, 1, 1 };      /* 2 _ 2 2  -> 4 2 * * */
        expect_merge("隔空的先靠拢再合", l4, 2, 1, -1, -1);
        int l5[G2048_N] = { 2, 1, 1, 0 };      /* 4 2 2 _  -> 4 4 * * */
        expect_merge("不同值不合并", l5, 2, 2, -1, -1);
        int l6[G2048_N] = { 1, 1, 1, 0 };      /* 2 2 2 _  -> 4 2 * * */
        expect_merge("三张只合一对", l6, 2, 1, -1, -1);
        int l7[G2048_N] = { 3, 3, 3, 3 };      /* 8 8 8 8  -> 16 16 * * */
        expect_merge("四张 8 分两组", l7, 4, 4, -1, -1);
        int l8[G2048_N] = { 0, 0, 0, 1 };      /* _ _ _ 2  -> 2 * * * */
        expect_merge("单个往左滑", l8, 1, -1, -1, -1);
        int l9[G2048_N] = { 1, 2, 1, 2 };      /* 2 4 2 4  -> 不动(全不同) */
        expect_merge("全不同则原样", l9, 1, 2, 1, 2);
    }

    /* ---- 无效移动 ---- */
    printf("\n  无效移动:\n");
    {
        int board[G2048_N][G2048_N];
        memset(board, 0, sizeof(board));
        board[3][0] = 1;                       /* 左下角一张 2，往左推不动 */
        g2048_set_board(board);
        int before = g2048_score();
        int moved = g2048_move(DIR_LEFT);
        CHECK(moved == 0, "已经靠左的局面再左移应当无效");
        CHECK(g2048_score() == before, "无效移动不加分");
    }

    /* ---- 胜负判定 ---- */
    printf("\n  胜负判定:\n");
    {
        int board[G2048_N][G2048_N];
        /* 摆一个"塞满但仍有相邻相同"的盘面 -> 未结束 */
        int vals[G2048_N][G2048_N] = {
            { 1, 2, 1, 2 },
            { 2, 1, 2, 1 },
            { 1, 2, 1, 2 },
            { 2, 1, 2, 2 },        /* 最后一格和左边相同 */
        };
        memcpy(board, vals, sizeof(vals));
        g2048_set_board(board);
        CHECK(g2048_over() == 0, "有相邻相同牌 -> 未结束");

        /* 改成国际象棋盘式(全不同) -> 结束 */
        int vals2[G2048_N][G2048_N] = {
            { 1, 2, 1, 2 },
            { 2, 1, 2, 1 },
            { 1, 2, 1, 2 },
            { 2, 1, 2, 1 },
        };
        memcpy(board, vals2, sizeof(vals2));
        g2048_set_board(board);
        CHECK(g2048_over() == 1, "格子全满且无相邻相同 -> 结束");

        /* 出现过 2048 -> won */
        memset(board, 0, sizeof(board));
        board[0][0] = G2048_WIN_EXP;
        g2048_set_board(board);
        CHECK(g2048_won() == 1, "出现 2048 -> 判定胜利");
    }

    /* ---- 开局与随机对局 ---- */
    printf("\n  开局与随机对局:\n");
    g2048_new();
    int before_empty = 0;
    for (int r = 0; r < G2048_N; r++) {
        for (int c = 0; c < G2048_N; c++) {
            if (g2048_cell(r, c) == 0) {
                before_empty++;
            }
        }
    }
    CHECK(before_empty == G2048_N * G2048_N - 2, "开局正好 2 张牌，空格 %d", before_empty);

    g2048_new();
    int last_score = g2048_score();
    int moves = 0, monotonic = 1, best = 0;
    for (int i = 0; i < 400; i++) {
        if (g2048_over()) {
            if (g2048_score() > best) {
                best = g2048_score();
            }
            g2048_new();
            last_score = g2048_score();
            continue;
        }
        if (g2048_move((G2048Dir)(rand() % 4))) {
            moves++;
            if (g2048_score() < last_score) {
                monotonic = 0;
            }
            last_score = g2048_score();
        }
    }
    if (g2048_score() > best) {
        best = g2048_score();
    }
    CHECK(moves > 50, "随机走 400 次里有 %d 次有效移动", moves);
    CHECK(monotonic, "分数单调不减，本局 %d，最高 %d", g2048_score(), best);

    /* ---- 悔棋 ---- */
    printf("\n  悔棋:\n");
    g2048_new();
    int s0 = g2048_score();
    int stepped = 0;
    for (int d = 0; d < 4 && !stepped; d++) {
        stepped = g2048_move((G2048Dir)d);
    }
    CHECK(stepped, "能走出第一步");
    int left_before = g2048_undo_left();
    CHECK(g2048_undo() == 1, "悔棋成功");
    CHECK(g2048_undo_left() == left_before - 1, "悔棋次数 %d -> %d",
          left_before, g2048_undo_left());
    CHECK(g2048_score() == s0, "悔棋后分数回到 %d", g2048_score());
    CHECK(g2048_undo() == 0, "同一步不能连续悔两次");

    /* 回归用例: 走了一步但没合并(得分仍为 0)时，必须也能悔棋。
     * 之前用 g_score==0 当"没走过"的判据，这条会失败。 */
    {
        int board[G2048_N][G2048_N];
        memset(board, 0, sizeof(board));
        board[0][3] = 1;            /* 2 在右上角 */
        board[3][0] = 2;            /* 4 在左下角 */
        g2048_set_board(board);
        int before[G2048_N][G2048_N];
        for (int r = 0; r < G2048_N; r++) {
            for (int c = 0; c < G2048_N; c++) {
                before[r][c] = g2048_cell(r, c);
            }
        }
        CHECK(g2048_move(DIR_LEFT) == 1, "零得分的一步也算有效移动");
        CHECK(g2048_score() == 0, "这一步确实没有合并(得分 0)");
        CHECK(g2048_undo() == 1, "零得分的一步也应当能悔棋");
        int same = 1;
        for (int r = 0; r < G2048_N; r++) {
            for (int c = 0; c < G2048_N; c++) {
                if (g2048_cell(r, c) != before[r][c]) {
                    same = 0;
                }
            }
        }
        CHECK(same, "悔棋后棋盘恢复原样");
    }

    g2048_new();
    for (int i = 0; i < G2048_UNDO_MAX; i++) {
        for (int d = 0; d < 4; d++) {
            if (g2048_move((G2048Dir)d)) {
                break;
            }
        }
        g2048_undo();
    }
    CHECK(g2048_undo_left() == 0, "悔棋额度用尽");
    CHECK(g2048_undo() == 0, "额度用尽后再悔棋应当失败");

    /* ---- 手势 ---- */
    printf("\n  手势换算:\n");
    {
        G2048Dir dir;
        CHECK(g2048_swipe_to_dir(500, 300, 500, 100, &dir) && dir == DIR_UP, "上滑 -> UP");
        CHECK(g2048_swipe_to_dir(500, 300, 500, 500, &dir) && dir == DIR_DOWN, "下滑 -> DOWN");
        CHECK(g2048_swipe_to_dir(500, 300, 200, 300, &dir) && dir == DIR_LEFT, "左滑 -> LEFT");
        CHECK(g2048_swipe_to_dir(500, 300, 800, 300, &dir) && dir == DIR_RIGHT, "右滑 -> RIGHT");
        CHECK(!g2048_swipe_to_dir(500, 300, 510, 305, &dir), "位移过小不算滑动");
        CHECK(g2048_swipe_to_dir(500, 300, 440, 200, &dir) && dir == DIR_UP,
              "斜滑(竖向位移更大)按竖向处理");
    }

    /* ---- 终局打印，人工看一眼 ---- */
    printf("\n  随机对局结束时的棋盘:\n");
    g2048_new();
    while (!g2048_over()) {
        int d = rand() % 4;
        g2048_move((G2048Dir)d);
    }
    dump();
    printf("      得分 %d  最高 %d  2048=%d  结束=%d\n",
           g2048_score(), g2048_best(), g2048_won(), g2048_over());
}

int main(void)
{
    printf("=== 2048 逻辑测试 ===\n");
    test_rules();

    printf("\n=== 结果: %s (%d 项失败) ===\n", g_fail ? "失败" : "全部通过", g_fail);
    return g_fail ? 1 : 0;
}

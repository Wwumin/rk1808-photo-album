/*
 * game2048.h  —  2048 小游戏 (附加题)
 *
 * 棋盘用"指数"存储而不是直接存数值:
 *   0 = 空格, 1 = 2, 2 = 4, ... n = 2^n
 * 好处: 判断胜负只要比大小(app[11] == 2048)，合并只要 +1，
 *       不会出现数值溢出，画牌面时直接 "tile_%d" 拼文件名。
 *
 * 操作: 上下左右四个方向滑屏或方向键。按题目"触摸"的要求，
 *       滑动手势由上层把 press/release 两个坐标转成方向后传进来。
 */

#ifndef ALBUM_GAME2048_H
#define ALBUM_GAME2048_H

#include <stdint.h>

#define G2048_N        4
#define G2048_WIN_EXP  11          /* 2^11 = 2048 */
#define G2048_MAX_EXP  12          /* 出现过 4096 就画金色牌 */
#define G2048_UNDO_MAX 3           /* 每局最多悔棋 3 次 */

typedef enum {
    DIR_UP = 0,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} G2048Dir;

/* 重开一局(会放两个初始牌) */
void g2048_new(void);

/* 走一步。返回 1 = 棋盘发生变化(有效移动)，0 = 该方向走不动 */
int  g2048_move(G2048Dir dir);

int  g2048_score(void);
int  g2048_best(void);            /* 会记住本次进程内的最高分 */
int  g2048_won(void);             /* 出现过 2048 */
int  g2048_over(void);            /* 无路可走 */
int  g2048_undo_left(void);       /* 还剩几次悔棋 */
int  g2048_undo(void);            /* 悔棋，成功返回 1 */

/* 取棋盘某格，返回指数(0..12) */
int  g2048_cell(int row, int col);

/*
 * 直接摆一个局面并刷新胜负状态。
 * 只给测试和"调试关卡"用: 正式流程里棋盘只能由 g2048_new()/g2048_move() 改变。
 * 摆完之后不会自动补牌，方便断言合并结果。
 */
void g2048_set_board(const int cells[G2048_N][G2048_N]);

/* 给测试用: 固定随机种子，让"补什么牌"可复现 */
void g2048_set_seed(unsigned seed);

/* 把四方向滑动手势换算成方向。返回 1 表示有效滑动，0 表示位移太小(当点击处理) */
int  g2048_swipe_to_dir(int x0, int y0, int x1, int y1, G2048Dir *out);

#endif /* ALBUM_GAME2048_H */

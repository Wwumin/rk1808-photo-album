/*
 * rng.h — 小游戏共用的可复现随机源
 *
 * 为什么要有这个文件:
 *   原来 2048/扫雷/贪吃蛇/俄罗斯方块各自调用 rand()，而 2048 在第一次补牌时
 *   用 time(NULL) 播种。结果是"同一份代码 + 同一个脚本"，每跑一次扫雷的雷
 *   布局都不一样 —— 冒烟测试因此不可复现（CI 上出现过的 stack smashing 就是
 *   只在某种雷布局下才触发的）。
 *
 *   现在所有小游戏都走 album_rand_below()：第一次调用时播种一次，
 *   播种优先级 = album_rng_seed() 显式设置 > 环境变量 ALBUM_SEED > time(NULL)。
 *   测试/CI 里设 ALBUM_SEED=<固定值> 就能 100% 复现。
 */
#ifndef ALBUM_RNG_H
#define ALBUM_RNG_H

/* 返回 [0, n) 的伪随机数；n <= 1 时返回 0。 */
int album_rand_below(int n);

/* 显式指定种子（测试用），下次取数时生效。 */
void album_rng_seed(unsigned seed);

#endif /* ALBUM_RNG_H */

/*
 * games.h  —  五个小游戏(井字棋/扫雷/象棋/俄罗斯方块/贪吃蛇)的公共约定
 *
 * 统一约定(六个游戏都遵守，含原来的 2048 也在往这个架子靠):
 *   - 每个游戏一页背景(tools/gen_games.py 生成) + 独立的 chrome 按钮
 *   - chrome 按钮固定三个位置: 左下"返回" 右下"重来" (扫雷多一个"插旗")
 *   - 每个游戏模块自己画动态层: ui_page_draw_bg() -> 画棋盘 -> ui_page_draw_buttons()
 *   - 触摸只分成 press/release，棋盘类游戏在 release 时判点击；需要连发的游戏
 *     (俄罗斯方块/贪吃蛇)用 tick() 里的计时器推进
 *
 * 坐标常量必须和 tools/gen_games.py 打印的一致，改一边就要改另一边。
 */

#ifndef ALBUM_GAMES_H
#define ALBUM_GAMES_H

#include "ui.h"

/* chrome 按钮(和 gen_games.py 的 BACK_POS/AGAIN_POS/EXTRA_POS 一致) */
#define G_BACK_X    26
#define G_BACK_Y    514
#define G_BACK_W    150
#define G_BACK_H    66

#define G_AGAIN_X   848
#define G_AGAIN_Y   514
#define G_AGAIN_W   150
#define G_AGAIN_H   66

#define G_EXTRA_X   437
#define G_EXTRA_Y   514
#define G_EXTRA_W   150
#define G_EXTRA_H   66

/* 方向键(俄罗斯方块用) */
#define G_DIR_Y     514
#define G_DIR_H     66
#define G_DL_X      250
#define G_ROT_X     346
#define G_DR_X      442
#define G_DROP_X    538
#define G_DIR_W     86
#define G_DROP_W    120

/* 棋盘几何 */
#define G_TTT_CELL  124
#define G_TTT_GAP   10
#define G_TTT_X0    316
#define G_TTT_Y0    116

#define G_MS_CELL   41
#define G_MS_GAP    3
#define G_MS_X0     315
#define G_MS_Y0     114

#define G_XQ_CELL   40
#define G_XQ_X0     352
#define G_XQ_Y0     124

#define G_TET_CELL  20
#define G_TET_COLS  10
#define G_TET_ROWS  20
#define G_TET_X0    258
#define G_TET_Y0    100

#define G_SNK_CELL  30
#define G_SNK_COLS  20
#define G_SNK_ROWS  15
#define G_SNK_X0    212
#define G_SNK_Y0    108

/* 屏幕底部状态行 */
/* 状态栏文字顶端 y。字体 16px 高，画到 y+15；
 * 原来 584 会让最后一行贴到屏幕最底(600)，看着像被切掉，上移 3px 留白。 */
#define G_STATUS_Y  581

typedef struct {
    const char *name;                          /* 用于日志 */
    void (*enter)(const UiPage *page);         /* 进入界面: 记录页面 + 重置 */
    void (*render)(int active);                /* 画一帧(含背景/内容/按钮/状态) */
    void (*tap)(int x, int y);                 /* 点击棋盘(非 chrome 按钮) */
    int  (*button)(const char *id);            /* chrome 按钮, 返回 1 = 已处理 */
    void (*tick)(long long now_ms);            /* 定时推进(重力/蛇移动) */
} GameOps;

extern const GameOps game_ttt;
extern const GameOps game_ms;
extern const GameOps game_xq;
extern const GameOps game_tet;
extern const GameOps game_snk;

/* ---- 公共小工具(games.c) ---- */
void gs_status(const char *fmt, ...);      /* 存一行状态文字，render 时画在底部 */
void gs_draw_status(void);
void gs_set_dir(const char *dir);          /* 资源目录(默认 "bmp") */
Image *gs_load(const char *dir, const char *name);

#endif /* ALBUM_GAMES_H */

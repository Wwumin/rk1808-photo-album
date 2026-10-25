/*
 * main.c  —  电子相册主程序
 *
 * 平台: RK1808 / aarch64-linux-gnu / DRM 直显 1024x600 / 触摸 /dev/input/event2
 *
 * ============================ 对应的题目要求 ============================
 *  1. 显示主界面                      -> SCREEN_MAIN
 *  2. 主界面: 相册/游戏/多媒体/其他/退出/选项   -> btn_main_*.bmp，六宫格
 *  3. 进入相册有二级界面: 手动/自动/退出/选项   -> SCREEN_ALBUM
 *  4. 自动播放，每 3 秒切换一张             -> PlayMode.auto，间隔可调(选项页)
 *  5. 手动触摸上下翻页 / 退出              -> SCREEN_PHOTO 里上半屏下一张、下半屏上一张
 *  附加 1. 小游戏                          -> SCREEN_GAME(六入口) -> 各游戏界面
 *  附加 2. 账户登录(密码锁)                 -> SCREEN_LOGIN + SCREEN_PIN (数字键盘)
 * ======================================================================
 *
 * 输入模型(和最早那版最大的改动):
 *   原来是"按下并抬起 = 一次点击"，做不出按钮的按下高亮。
 *   现在把触摸拆成 press / release 两个事件:
 *     - press  记下按住的按钮下标，立刻重画一帧(按钮换成 _p 按下态)
 *     - release 时如果和按下的是同一个按钮才算"点击"，然后执行动作
 *   这样手指滑出按钮再松开不会误触发，也顺手有了得体的交互反馈。
 */

/*
 * 必须在所有 #include 之前:
 *  - clock_gettime / CLOCK_MONOTONIC 属于 POSIX(2008) 接口, 在 glibc 上
 *    只有 _POSIX_C_SOURCE >= 199309L 时才在 <time.h> 里声明; 用 -std=c11
 *    编译时 glibc 默认是 __STRICT_ANSI__, 这两个就"未声明"了。
 *  - strcasecmp / strncasecmp 同理, 声明在 <strings.h>, 也需要 POSIX 宏。
 * 交叉编译时如果不想改源码, 在 Makefile 里加 -D_POSIX_C_SOURCE=200809L 等价。
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>    /* strcasecmp / strncasecmp (POSIX) */
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>

#include "display.h"
#include "image.h"
#include "draw.h"
#include "touch.h"
#include "ui.h"
#include "game2048.h"
#include "account.h"
#include "games.h"

/* ------------------------------------------------------------------ 配置 */
#define ASSET_DIR_DEFAULT   "bmp"
/*
 * 触摸设备留空表示"自动找"。
 * 板子上 /dev/input/ 通常有键盘、鼠标、触摸好几个 event 设备，编号跟
 * 接线和驱动加载顺序有关，写死一个很可能就是鼠标。touch.c 会扫描
 * /dev/input/event0..N，按"有没有绝对坐标轴"挑出真正的触摸屏。
 * 也可以用环境变量 TOUCH_DEV 或第二个命令行参数强制指定。
 */
#define TOUCH_DEV_DEFAULT   NULL
#define ACCOUNT_CFG_DEFAULT "account.cfg"

#define TICK_MS             30        /* 主循环节拍: 够跟手，又不空转 */
#define DEFAULT_INTERVAL_MS 3000      /* 题目要求: 每 3 秒切换 */
#define MAX_PHOTOS          128
#define TOAST_MS            1800      /* 提示条停留时间 */
#define LOGIN_FAIL_LIMIT    3         /* 密码错几次就把界面锁一下 */

#define PHOTO_DIR_DEFAULT   ASSET_DIR_DEFAULT

/* 颜色 */
#define C_WHITE   BGRA(255, 255, 255)
#define C_BLACK   BGRA(0, 0, 0)
#define C_HUD     BGRA(190, 215, 235)
#define C_ACCENT  BGRA(255, 190, 90)
#define C_WARN    BGRA(120, 160, 255)
#define C_OK      BGRA(120, 220, 150)

/* ------------------------------------------------------------------ 界面 */
typedef enum {
    SCREEN_LOGIN = 0,     /* 输入密码 */
    SCREEN_PIN,           /* 改密码: 输入新密码 */
    SCREEN_MAIN,
    SCREEN_ALBUM,
    SCREEN_PHOTO,
    SCREEN_OPTION,
    SCREEN_GAME,          /* 游戏菜单(六个入口) */
    SCREEN_2048,
    SCREEN_TTT,           /* 井字棋 */
    SCREEN_MS,            /* 扫雷 */
    SCREEN_XQ,            /* 象棋 */
    SCREEN_TET,           /* 俄罗斯方块 */
    SCREEN_SNK            /* 贪吃蛇 */
} Screen;

typedef enum {
    MODE_MANUAL = 0,
    MODE_AUTO
} PlayMode;

/* ------------------------------------------------------------------ 按钮布局 */
/*
 * 坐标必须和 tools/gen_buttons.py 打印的完全一致。
 * 那个脚本改布局后会重新打印这张表，抄过来即可。
 */
#define MAIN_CARD_W 296
#define MAIN_CARD_H 190
#define MAIN_GAP_X  40
#define MAIN_GAP_Y  34
#define MAIN_X0     28
#define MAIN_Y0     132

#define ALBUM_CARD_W 452
#define ALBUM_CARD_H 190
#define ALBUM_GAP_X  44
#define ALBUM_GAP_Y  30
#define ALBUM_X0     38
#define ALBUM_Y0     138

/* 游戏菜单: 300x190 三列两行，和 tools/gen_buttons.py 的 GAME_* 一致 */
#define GAME_CARD_W 300
#define GAME_CARD_H 190
#define GAME_GAP_X  32
#define GAME_GAP_Y  30
#define GAME_X0     32
#define GAME_Y0     132

/*
 * 密码键盘: 4 行 3 列。
 * 老版本用 132x96 从 y=320 起，第 3、4 行直接排到屏幕外 —— 界面上看不见，
 * 手指却还能点到，属于最难受的那种 bug。改成 118x70、从 268 起，最后一行
 * 落在 508..578，整块键盘都在 600 高以内。
 */
#define KEY_W   118
#define KEY_H   70
#define KEY_GX  16
#define KEY_GY  10
#define KEY_ROWS 4
#define KEY_COLS 3
#define KEY_Y0  268

static int main_btn_x[6], main_btn_y[6];
static int album_btn_x[4], album_btn_y[4];
static int game_btn_x[6], game_btn_y[6];
static int key_btn_x[12], key_btn_y[12];

static void layout_init(void)
{
    for (int i = 0; i < 6; i++) {
        main_btn_x[i] = MAIN_X0 + (i % 3) * (MAIN_CARD_W + MAIN_GAP_X);
        main_btn_y[i] = MAIN_Y0 + (i / 3) * (MAIN_CARD_H + MAIN_GAP_Y);
        game_btn_x[i] = GAME_X0 + (i % 3) * (GAME_CARD_W + GAME_GAP_X);
        game_btn_y[i] = GAME_Y0 + (i / 3) * (GAME_CARD_H + GAME_GAP_Y);
    }
    for (int i = 0; i < 4; i++) {
        album_btn_x[i] = ALBUM_X0 + (i % 2) * (ALBUM_CARD_W + ALBUM_GAP_X);
        album_btn_y[i] = ALBUM_Y0 + (i / 2) * (ALBUM_CARD_H + ALBUM_GAP_Y);
    }
    int kx0 = (1024 - (KEY_COLS * KEY_W + (KEY_COLS - 1) * KEY_GX)) / 2;
    for (int i = 0; i < 12; i++) {
        key_btn_x[i] = kx0 + (i % KEY_COLS) * (KEY_W + KEY_GX);
        key_btn_y[i] = KEY_Y0 + (i / KEY_COLS) * (KEY_H + KEY_GY);
    }
}

/* ------------------------------------------------------------------ 页面与状态 */
static UiPage g_page_login, g_page_pin, g_page_main, g_page_album;
static UiPage g_page_option, g_page_game;
/* 五个新游戏的页面(chrome 按钮各不相同, 所以一页一份) */
static UiPage g_page_ttt, g_page_ms, g_page_xq, g_page_tet, g_page_snk;
static UiPage *g_page = NULL;      /* 当前显示的页面(没有页面的界面为 NULL) */

/* 当前游戏(六选一; 2048 还留在 main.c 里, 其余走 GameOps 接口) */
static const GameOps *g_game_ops = NULL;

static Screen   g_screen = SCREEN_LOGIN;
static PlayMode g_mode = MODE_MANUAL;
static int      g_interval_ms = DEFAULT_INTERVAL_MS;

/* 触摸状态 */
static int g_pressed = 0;
static int g_press_x, g_press_y;
static int g_press_btn = -1;
static int g_press_swipe = 0;      /* 按下时落在 2048 棋盘上，抬手要判手势 */

/* 相册 */
static Image *g_photos[MAX_PHOTOS];
static char   g_photo_name[MAX_PHOTOS][64];
static int    g_photo_count = 0;
static int    g_index = 0;
static long long g_last_switch_ms = 0;
static const char *g_asset_dir = ASSET_DIR_DEFAULT;

/* 登录 */
static char g_pin_buf[16];
static int  g_pin_len = 0;
static int  g_login_errors = 0;
static int  g_pin_change_mode = 0;   /* 1 = 正在设置新密码 */
static char g_pin_first[16];         /* 改密码时第一次输入的内容(要求输两遍) */

/* 提示条 */
static char g_toast[64];
static long long g_toast_until_ms = 0;

/* 2048 */
static Image *g_tile[13];            /* 下标 = 指数 0..12 */
static Image *g_tile_bg = NULL;
static Image *g_2048_bg = NULL;      /* 2048 静态背景(标题/棋盘底/信息面板/chrome 按钮) */
static Image *g_no_photo = NULL;
static Image *g_undo_icon = NULL;
/* 相册播放页右下角的"退出"键(手动/自动都显示)。按钮图是 180x64，
 * 位置贴近右下角、但让开底部 30px 信息条，避免压住文件名。 */
static Image *g_photo_exit = NULL;
static Image *g_photo_exit_p = NULL;
#define PHOTO_EXIT_W   180
#define PHOTO_EXIT_H   64
#define PHOTO_EXIT_X   (1024 - 12 - PHOTO_EXIT_W)
#define PHOTO_EXIT_Y   (600 - 30 - 8 - PHOTO_EXIT_H)   /* = 498 */
#define BOARD_ORIGIN_X  ((1024 - 444) / 2)
#define BOARD_ORIGIN_Y  130
#define TILE_SIZE       91
#define TILE_GAP        16

static int g_running = 1;

/* ------------------------------------------------------------------ 时间 */
static long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* ------------------------------------------------------------------ 提示条 */
static void toast(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_toast, sizeof(g_toast), fmt, ap);
    va_end(ap);
    g_toast_until_ms = now_ms() + TOAST_MS;
    printf("[ui] 提示: %s\n", g_toast);
}

static int toast_active(void)
{
    return g_toast_until_ms > now_ms();
}

/* ------------------------------------------------------------------ 资源 */
static char *path_join(const char *dir, const char *name)
{
    static char buf[512];
    snprintf(buf, sizeof(buf), "%s/%s", dir, name);
    return buf;
}

static Image *load_asset(const char *name)
{
    return image_load_bmp(path_join(g_asset_dir, name));
}

/* ------------------------------------------------------------------ 相册清单 */
static int load_photo_list(void)
{
    DIR *d = opendir(g_asset_dir);
    struct dirent *e;

    if (!d) {
        perror(g_asset_dir);
        return -1;
    }
    while ((e = readdir(d)) != NULL && g_photo_count < MAX_PHOTOS) {
        size_t len = strlen(e->d_name);
        if (len < 5 || strncasecmp(e->d_name, "photo_", 6) != 0) {
            continue;
        }
        if (strcasecmp(e->d_name + len - 4, ".bmp") != 0) {
            continue;
        }
        snprintf(g_photo_name[g_photo_count], sizeof(g_photo_name[0]), "%s", e->d_name);
        g_photo_count++;
    }
    closedir(d);

    /* 插入排序，保证翻页顺序稳定(readdir 的顺序不保证) */
    for (int i = 1; i < g_photo_count; i++) {
        char key[64];
        snprintf(key, sizeof(key), "%s", g_photo_name[i]);
        int j = i - 1;
        while (j >= 0 && strcmp(g_photo_name[j], key) > 0) {
            snprintf(g_photo_name[j + 1], sizeof(g_photo_name[0]), "%s", g_photo_name[j]);
            j--;
        }
        snprintf(g_photo_name[j + 1], sizeof(g_photo_name[0]), "%s", key);
    }
    return g_photo_count > 0 ? 0 : -1;
}

static int load_one_photo(int idx)
{
    if (idx < 0 || idx >= g_photo_count) {
        return -1;
    }
    if (g_photos[idx]) {
        return 0;
    }
    g_photos[idx] = image_load_bmp(path_join(g_asset_dir, g_photo_name[idx]));
    return g_photos[idx] ? 0 : -1;
}

/* ------------------------------------------------------------------ 绘制 */
static uint8_t *fb(void)          { return display_backbuffer(); }
static int      fbp(void)         { return display_pitch(); }
static int      fbw(void)         { return display_width(); }
static int      fbh(void)         { return display_height(); }

static void draw_toast(void)
{
    const char *text = g_toast;
    int scale = 2;
    int tw = draw_text_width(text, scale);
    int bw = tw + 44;
    int bx = (fbw() - bw) / 2;
    int by = 108;
    uint32_t col = C_ACCENT;

    if (bw > 1000) {
        scale = 1;
        tw = draw_text_width(text, scale);
        bw = tw + 44;
        bx = (fbw() - bw) / 2;
    }
    draw_fill_rect(fb(), fbp(), fbw(), fbh(), bx, by, bw, 50, C_BLACK, 215);
    draw_rect(fb(), fbp(), fbw(), fbh(), bx, by, bw, 50, col);
    draw_text_center(fb(), fbp(), fbw(), fbh(), fbw() / 2, by + 17, text, col, scale);
}

/* 画当前页面 + 提示条 */
static void render_page(void)
{
    ui_page_draw(g_page, g_press_btn);
    if (toast_active()) {
        draw_toast();
        display_present();
    }
}

/* 只重画一个按钮区域，不整屏重绘 —— 按下/抬起时用，避免闪 */
static void render_button_state(int idx)
{
    const UiButton *b;
    if (!g_page || idx < 0) {
        return;
    }
    b = ui_page_button(g_page, idx);
    if (!b || !b->has_image) {
        return;
    }
    /* 重新铺这一块背景，再贴按钮的普通/按下态 */
    if (g_page->bg) {
        image_blit_alpha_sub(fb(), fbp(), fbw(), fbh(), g_page->bg,
                             b->x, b->y, b->w, b->h, b->x, b->y);
    }
    image_blit_alpha(fb(), fbp(), fbw(), fbh(), b->normal, b->x, b->y);
    if (idx == g_press_btn && b->press) {
        image_blit_alpha(fb(), fbp(), fbw(), fbh(), b->press, b->x, b->y);
    }
    display_present();
}

/* ------------------------------------------------------------------ 登录界面 */
static void login_render(void)
{
    ui_page_draw(&g_page_login, g_press_btn);

    /* 密码框里的圆点 */
    {
        int cx0 = 512 - (ACCOUNT_PIN_LEN * 56) / 2;
        for (int i = 0; i < ACCOUNT_PIN_LEN; i++) {
            int cx = cx0 + i * 56 + 28;
            int filled = (i < g_pin_len);
            draw_fill_rect(fb(), fbp(), fbw(), fbh(), cx - 15, 182, 30, 30,
                           filled ? C_ACCENT : C_BLACK, filled ? 255 : 120);
            draw_rect(fb(), fbp(), fbw(), fbh(), cx - 15, 182, 30, 30,
                      filled ? C_ACCENT : BGRA(120, 130, 170));
        }
    }

    /* 状态行 */
    {
        char line[128];
        if (g_login_errors > 0) {
            snprintf(line, sizeof(line), "PASSWORD ERROR  (%d/%d)",
                     g_login_errors, LOGIN_FAIL_LIMIT);
            draw_text_center(fb(), fbp(), fbw(), fbh(), 512, 250, line,
                             BGRA(255, 120, 120), 1);
        } else {
            snprintf(line, sizeof(line), "DEFAULT PIN = 1234   -   %d DIGITS",
                     ACCOUNT_PIN_LEN);
            draw_text_center(fb(), fbp(), fbw(), fbh(), 512, 250, line, C_HUD, 1);
        }
    }

    if (toast_active()) {
        draw_toast();
    }
    display_present();
}

static void pin_render(void)
{
    ui_page_draw(&g_page_pin, g_press_btn);

    {
        int cx0 = 512 - (ACCOUNT_PIN_LEN * 56) / 2;
        for (int i = 0; i < ACCOUNT_PIN_LEN; i++) {
            int cx = cx0 + i * 56 + 28;
            int filled = (i < g_pin_len);
            draw_fill_rect(fb(), fbp(), fbw(), fbh(), cx - 15, 182, 30, 30,
                           filled ? C_OK : C_BLACK, filled ? 255 : 120);
            draw_rect(fb(), fbp(), fbw(), fbh(), cx - 15, 182, 30, 30,
                      filled ? C_OK : BGRA(120, 130, 170));
        }
    }
    {
        char line[128];
        snprintf(line, sizeof(line), "NEW PIN  -  ENTER TWICE  -  %s",
                 g_pin_change_mode == 1 ? "FIRST" : "AGAIN");
        draw_text_center(fb(), fbp(), fbw(), fbh(), 512, 250, line, C_HUD, 1);
    }
    if (toast_active()) {
        draw_toast();
    }
    display_present();
}

/* ------------------------------------------------------------------ 选项界面 */
static void option_render(void)
{
    ui_page_draw(&g_page_option, g_press_btn);

    /* 把当前生效值标出来 */
    {
        char line[128];
        snprintf(line, sizeof(line), "CURRENT INTERVAL = %d MS", g_interval_ms);
        draw_text_center(fb(), fbp(), fbw(), fbh(), 512, 300, line, C_ACCENT, 1);
    }
    if (toast_active()) {
        draw_toast();
    }
    display_present();
}

/* ------------------------------------------------------------------ 照片界面 */
static void photo_render(void)
{
    int w = fbw(), h = fbh();
    char buf[160];

    if (g_photo_count <= 0) {
        /* 没有照片: 背景纯色 + 占位框，而不是白屏 */
        draw_fill_rect(fb(), fbp(), w, h, 0, 0, w, h, BGRA(14, 16, 26), 255);
        if (g_no_photo) {
            image_blit_alpha(fb(), fbp(), w, h, g_no_photo,
                             (w - g_no_photo->w) / 2, (h - g_no_photo->h) / 2);
        } else {
            draw_text_center(fb(), fbp(), w, h, w / 2, h / 2, "NO PHOTO", C_WARN, 3);
        }
        draw_text_center(fb(), fbp(), w, h, w / 2, h - 60,
                         "ALBUM EMPTY", BGRA(200, 190, 180), 1);
        display_present();
        return;
    }

    if (load_one_photo(g_index) != 0) {
        draw_fill_rect(fb(), fbp(), w, h, 0, 0, w, h, C_BLACK, 255);
        draw_text_center(fb(), fbp(), w, h, w / 2, h / 2 - 8,
                         "PHOTO LOAD FAILED", C_WARN, 2);
        display_present();
        return;
    }

    image_blit(fb(), fbp(), w, h, g_photos[g_index], 0, 0);

    /* 顶部状态条 */
    draw_fill_rect(fb(), fbp(), w, h, 0, 0, w, 34, C_BLACK, 170);
    if (g_mode == MODE_AUTO) {
        snprintf(buf, sizeof(buf), "AUTO  %d.%ds / PHOTO", g_interval_ms / 1000,
                 (g_interval_ms % 1000) / 100);
    } else {
        snprintf(buf, sizeof(buf), "MANUAL  TAP UP = NEXT   DOWN = PREV");
    }
    draw_text(fb(), fbp(), w, h, 12, 9, buf, C_HUD, 1);

    snprintf(buf, sizeof(buf), "%d / %d", g_index + 1, g_photo_count);
    draw_text(fb(), fbp(), w, h, w - 12 - draw_text_width(buf, 1), 9, buf, C_ACCENT, 1);

    /* 底部信息条 */
    draw_fill_rect(fb(), fbp(), w, h, 0, h - 30, w, 30, C_BLACK, 175);
    snprintf(buf, sizeof(buf), "%s", g_photo_name[g_index]);
    draw_text(fb(), fbp(), w, h, 12, h - 23, buf, C_HUD, 1);
    snprintf(buf, sizeof(buf), "TAP TOP-LEFT = BACK");
    draw_text(fb(), fbp(), w, h, w - 12 - draw_text_width(buf, 1), h - 23, buf, C_HUD, 1);

    /* 自动模式的进度条 */
    if (g_mode == MODE_AUTO && g_interval_ms > 0) {
        long long elapsed = now_ms() - g_last_switch_ms;
        int pw = (int)((elapsed * w) / g_interval_ms);
        if (pw > w) {
            pw = w;
        }
        if (pw > 0) {
            draw_fill_rect(fb(), fbp(), w, h, 0, h - 4, pw, 4, C_ACCENT, 255);
        }
    }

    /* 右下角常驻"退出"键: 手动/自动两种模式都显示 */
    if (g_photo_exit) {
        image_blit_alpha(fb(), fbp(), w, h, g_photo_exit,
                         PHOTO_EXIT_X, PHOTO_EXIT_Y);
    }

    if (toast_active()) {
        draw_toast();
    }
    display_present();
}

static void photo_goto(int idx)
{
    if (g_photo_count <= 0) {
        return;
    }
    g_index = ((idx % g_photo_count) + g_photo_count) % g_photo_count;
    g_last_switch_ms = now_ms();
}

static void photo_step(int delta)
{
    if (g_photo_count <= 0) {
        return;
    }
    photo_goto(g_index + delta);
    printf("[album] %s -> %d/%d (%s)\n", delta > 0 ? "下一张" : "上一张",
           g_index + 1, g_photo_count, g_photo_name[g_index]);
    photo_render();
}

/* ------------------------------------------------------------------ 2048 绘制 */
static void game2048_render(void)
{
    int w = fbw(), h = fbh();
    int ix = BOARD_ORIGIN_X + 444 + 24;     /* 右侧信息栏左边 = 758 */
    int iy = BOARD_ORIGIN_Y;                /* 130 */
    char buf[128];

    /*
     * 背景是一张烘好的图: 标题、棋盘底、SCORE/BEST/UNDO 面板、左下"返回"/右下"重来"
     * 都在图里，和另外五个游戏用的是同一套主题和同一套 chrome 按钮位置。
     * 这里只画会变的部分: 方块、数字、结束遮罩。
     */
    if (g_2048_bg) {
        image_blit_alpha(fb(), fbp(), w, h, g_2048_bg, 0, 0);
    } else {
        draw_fill_rect(fb(), fbp(), w, h, 0, 0, w, h, BGRA(26, 18, 12), 255);
    }

    if (g_tile_bg) {
        image_blit_alpha(fb(), fbp(), w, h, g_tile_bg, BOARD_ORIGIN_X, BOARD_ORIGIN_Y);
    }

    for (int r = 0; r < G2048_N; r++) {
        for (int c = 0; c < G2048_N; c++) {
            int e = g2048_cell(r, c);
            if (e <= 0 || e > G2048_MAX_EXP) {
                continue;
            }
            if (!g_tile[e]) {
                continue;
            }
            int x = BOARD_ORIGIN_X + TILE_GAP + c * (TILE_SIZE + TILE_GAP);
            int y = BOARD_ORIGIN_Y + TILE_GAP + r * (TILE_SIZE + TILE_GAP);
            image_blit_alpha(fb(), fbp(), w, h, g_tile[e], x, y);
        }
    }

    /* 分数 / 最高分: 面板位置和 tools/gen_games.py 里的 gen_2048_bg() 对齐 */
    snprintf(buf, sizeof(buf), "%d", g2048_score());
    draw_text(fb(), fbp(), w, h, ix + 118 - draw_text_width(buf, 2) / 2, iy + 54,
              buf, C_WHITE, 2);

    snprintf(buf, sizeof(buf), "%d", g2048_best());
    draw_text(fb(), fbp(), w, h, ix + 118 - draw_text_width(buf, 2) / 2, iy + 178,
              buf, C_ACCENT, 2);

    /* 悔棋剩余次数 */
    snprintf(buf, sizeof(buf), "x %d", g2048_undo_left());
    draw_text(fb(), fbp(), w, h, ix + 118 - draw_text_width(buf, 2) / 2, iy + 294,
              buf, g2048_undo_left() > 0 ? C_OK : BGRA(120, 110, 100), 2);

    /* 胜利/结束遮罩 */
    if (g2048_won() || g2048_over()) {
        const char *title = g2048_won() ? "YOU WIN !" : "GAME OVER";
        uint32_t col = g2048_won() ? C_OK : BGRA(255, 120, 120);
        int bw = 460, bh = 130;
        int bx = (w - bw) / 2, by = (h - bh) / 2;
        draw_fill_rect(fb(), fbp(), w, h, bx - 2, by - 2, bw + 4, bh + 4, C_BLACK, 235);
        draw_rect(fb(), fbp(), w, h, bx, by, bw, bh, col);
        draw_text_center(fb(), fbp(), w, h, w / 2, by + 22, title, col, 3);
        snprintf(buf, sizeof(buf), "SCORE %d", g2048_score());
        draw_text_center(fb(), fbp(), w, h, w / 2, by + 74, buf, C_HUD, 1);
        draw_text_center(fb(), fbp(), w, h, w / 2, by + 100,
                         g2048_over() ? "TAP 重来 OR SWIPE TO CONTINUE"
                                      : "KEEP GOING WITH SWIPE", C_HUD, 1);
    }

    if (toast_active()) {
        draw_toast();
    }
    display_present();
}

/* ------------------------------------------------------------------ 登录逻辑 */
static void login_reset(void)
{
    g_pin_len = 0;
    g_pin_buf[0] = '\0';
}

static void login_enter_main(void)
{
    g_screen = SCREEN_MAIN;
    login_reset();
    g_page = &g_page_main;
    render_page();
}

static void login_submit(void)
{
    g_pin_buf[g_pin_len] = '\0';

    if (g_screen == SCREEN_PIN) {
        /* 改密码: 输两遍 */
        if (g_pin_change_mode == 1) {
            snprintf(g_pin_first, sizeof(g_pin_first), "%s", g_pin_buf);
            g_pin_change_mode = 2;
            login_reset();
            toast("ENTER THE SAME PIN AGAIN");
            pin_render();
            return;
        }
        if (strcmp(g_pin_first, g_pin_buf) != 0) {
            g_pin_change_mode = 1;
            login_reset();
            toast("NOT MATCH, TRY AGAIN");
            pin_render();
            return;
        }
        if (account_change_pin(g_pin_buf) == 0) {
            toast("PASSWORD CHANGED");
        } else {
            toast("SAVE FAILED");
        }
        g_screen = SCREEN_MAIN;
        login_reset();
        g_page = &g_page_main;
        render_page();
        return;
    }

    /* 登录 */
    if (account_verify(g_pin_buf)) {
        printf("[account] 密码正确\n");
        g_login_errors = 0;
        toast("WELCOME");
        login_enter_main();
        return;
    }

    g_login_errors++;
    printf("[account] 密码错误 (%d/%d)\n", g_login_errors, LOGIN_FAIL_LIMIT);
    login_reset();
    if (g_login_errors >= LOGIN_FAIL_LIMIT) {
        g_login_errors = 0;
        toast("TOO MANY ERRORS, WAIT 3s");
        login_render();
        sleep(3);                   /* 简单的失败惩罚 */
        login_render();
    } else {
        toast("WRONG PASSWORD");
        login_render();
    }
}

/* 键盘上的 0..9 / del / ok，action 为 '0'..'9' | 'd' | 'o' */
static void login_key(char action)
{
    if (action == 'd') {
        if (g_pin_len > 0) {
            g_pin_buf[--g_pin_len] = '\0';
        }
        (g_screen == SCREEN_PIN ? pin_render : login_render)();
        return;
    }
    if (action == 'o') {
        if (g_pin_len == ACCOUNT_PIN_LEN) {
            login_submit();
        } else {
            toast("NEED %d DIGITS", ACCOUNT_PIN_LEN);
        }
        return;
    }
    if (action >= '0' && action <= '9') {
        if (g_pin_len < ACCOUNT_PIN_LEN) {
            g_pin_buf[g_pin_len++] = action;
            g_pin_buf[g_pin_len] = '\0';
        }
        if (g_pin_len == ACCOUNT_PIN_LEN) {
            /* 输满自动提交，少按一次确定 */
            (g_screen == SCREEN_PIN ? pin_render : login_render)();
            login_submit();
        } else {
            (g_screen == SCREEN_PIN ? pin_render : login_render)();
        }
    }
}

/* ------------------------------------------------------------------ 进入各界面 */
static void goto_main(void)
{
    g_screen = SCREEN_MAIN;
    g_page = &g_page_main;
    g_game_ops = NULL;
    render_page();
}

static void goto_album(void)
{
    g_screen = SCREEN_ALBUM;
    g_page = &g_page_album;
    g_game_ops = NULL;
    render_page();
}

static void goto_login(void)
{
    g_screen = SCREEN_LOGIN;
    g_page = &g_page_login;
    g_game_ops = NULL;
    login_reset();
    g_login_errors = 0;
    login_render();
}

static void goto_change_pin(void)
{
    g_screen = SCREEN_PIN;
    g_page = &g_page_pin;
    g_game_ops = NULL;
    g_pin_change_mode = 1;
    login_reset();
    pin_render();
}

static void goto_option(void)
{
    g_screen = SCREEN_OPTION;
    g_page = &g_page_option;
    g_game_ops = NULL;
    render_page();
}

static void goto_game(void)
{
    g_screen = SCREEN_GAME;
    g_page = &g_page_game;
    g_game_ops = NULL;
    render_page();
}

static void goto_2048(void)
{
    g_screen = SCREEN_2048;
    g_page = NULL;
    g_game_ops = NULL;
    g2048_new();
    game2048_render();
}

/* 五个新游戏的统一入口: 切屏 + 交给各自的 enter() 重置状态并画第一帧 */
static void goto_sub_game(Screen sc, UiPage *page, const GameOps *ops)
{
    g_screen = sc;
    g_page = page;
    g_game_ops = ops;
    printf("[game] 进入 %s\n", ops->name);
    ops->enter(page);
}

static void goto_photo(PlayMode mode)
{
    g_mode = mode;
    g_screen = SCREEN_PHOTO;
    g_page = NULL;
    photo_goto(0);
    photo_render();
}

/* 返回上一层: 每个界面按"左上角"时的去处 */
static void go_back(void)
{
    switch (g_screen) {
    case SCREEN_ALBUM:
    case SCREEN_OPTION:
    case SCREEN_GAME:
        goto_main();
        break;
    case SCREEN_PIN:            /* 改密码界面: 放弃修改, 回主界面 */
        g_pin_change_mode = 0;
        login_reset();
        goto_main();
        break;
    case SCREEN_PHOTO:
        goto_album();
        break;
    case SCREEN_2048:
    case SCREEN_TTT:
    case SCREEN_MS:
    case SCREEN_XQ:
    case SCREEN_TET:
    case SCREEN_SNK:
        goto_game();
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------------ 点击动作 */
/*
 * 注意: 按钮 id 来自 tools/gen_buttons.py 的清单，**带 ui 前缀**
 *       (main_exit / album_manual / opt_3s / game_2048 / key_ok ...)。
 *       这里必须拿完整 id 比较，只写 "exit" 是永远匹配不上的 ——
 *       这个坑真实踩过一次: 点退出没反应，界面停在主界面，
 *       主循环空转 120 万次 g_running 还是 1。
 */
#define ID_IS(id, s) (strcmp((id), (s)) == 0)

static void on_main_button(const char *id)
{
    printf("[main] %s\n", id);
    if (ID_IS(id, "main_album")) {
        goto_album();
    } else if (ID_IS(id, "main_game")) {
        goto_game();
    } else if (ID_IS(id, "main_exit")) {
        g_running = 0;
    } else if (ID_IS(id, "main_opt")) {
        goto_option();
    } else if (ID_IS(id, "main_other")) {
        goto_change_pin();          /* "其他" -> 账户登录/改密码 */
    } else if (ID_IS(id, "main_media")) {
        toast("MULTIMEDIA NOT IMPLEMENTED");
    }
}

static void on_album_button(const char *id)
{
    printf("[album] %s\n", id);
    if (ID_IS(id, "album_manual")) {
        goto_photo(MODE_MANUAL);
    } else if (ID_IS(id, "album_auto")) {
        goto_photo(MODE_AUTO);
    } else if (ID_IS(id, "album_exit")) {
        goto_main();
    } else if (ID_IS(id, "album_opt")) {
        goto_option();
    }
}

static void on_option_button(const char *id)
{
    printf("[option] %s\n", id);
    if (ID_IS(id, "opt_1s")) {
        g_interval_ms = 1000;
    } else if (ID_IS(id, "opt_3s")) {
        g_interval_ms = 3000;
    } else if (ID_IS(id, "opt_5s")) {
        g_interval_ms = 5000;
    } else if (ID_IS(id, "opt_10s")) {
        g_interval_ms = 10000;
    }
    toast("INTERVAL = %d MS", g_interval_ms);
    option_render();
}

static void on_game_button(const char *id)
{
    printf("[game] %s\n", id);
    if (ID_IS(id, "game_2048")) {
        goto_2048();
    } else if (ID_IS(id, "game_ttt")) {
        goto_sub_game(SCREEN_TTT, &g_page_ttt, &game_ttt);
    } else if (ID_IS(id, "game_ms")) {
        goto_sub_game(SCREEN_MS, &g_page_ms, &game_ms);
    } else if (ID_IS(id, "game_xq")) {
        goto_sub_game(SCREEN_XQ, &g_page_xq, &game_xq);
    } else if (ID_IS(id, "game_tetris")) {
        goto_sub_game(SCREEN_TET, &g_page_tet, &game_tet);
    } else if (ID_IS(id, "game_snake")) {
        goto_sub_game(SCREEN_SNK, &g_page_snk, &game_snk);
    }
}

/* 键盘按钮 id: key_0 .. key_9 / key_del / key_ok */
static void on_key_button(const char *id)
{
    if (strncmp(id, "key_", 4) != 0) {
        return;
    }
    const char *k = id + 4;
    if (strcmp(k, "del") == 0) {
        login_key('d');
    } else if (strcmp(k, "ok") == 0) {
        login_key('o');
    } else if (k[0] >= '0' && k[0] <= '9' && k[1] == '\0') {
        login_key(k[0]);
    }
}

/* 2048 界面: 判断点在哪 */
static void on_2048_press(int x, int y)
{
    int ix = BOARD_ORIGIN_X + 444 + 24;
    int iy = BOARD_ORIGIN_Y;

    /* chrome 按钮(位置和另外五个游戏一致, 见 games.h 的 G_BACK_x / G_AGAIN_x) */
    if (x >= G_BACK_X && x <= G_BACK_X + G_BACK_W &&
        y >= G_BACK_Y && y <= G_BACK_Y + G_BACK_H) {
        goto_game();
        return;
    }
    if (x >= G_AGAIN_X && x <= G_AGAIN_X + G_AGAIN_W &&
        y >= G_AGAIN_Y && y <= G_AGAIN_Y + G_AGAIN_H) {
        g2048_new();
        game2048_render();
        return;
    }

    /* 悔棋区域 */
    if (x >= ix && x <= ix + 236 && y >= iy + 248 && y <= iy + 340) {
        if (g2048_undo()) {
            toast("UNDO OK");
        } else {
            toast("NO UNDO LEFT");
        }
        game2048_render();
        return;
    }
    g_press_swipe = 1;      /* 记下来，抬手时按滑动方向处理 */
}

static void on_2048_release(int x0, int y0, int x1, int y1)
{
    G2048Dir dir;
    if (!g2048_swipe_to_dir(x0, y0, x1, y1, &dir)) {
        return;             /* 位移太小，当作误触 */
    }
    if (g2048_move(dir)) {
        game2048_render();
    }
}

/* ------------------------------------------------------------------ 触摸事件分发 */
static void on_press(int x, int y)
{
    g_pressed = 1;
    g_press_x = x;
    g_press_y = y;
    g_press_btn = -1;
    g_press_swipe = 0;

    switch (g_screen) {
    case SCREEN_LOGIN:
    case SCREEN_PIN:
    case SCREEN_MAIN:
    case SCREEN_ALBUM:
    case SCREEN_OPTION:
    case SCREEN_GAME:
        g_press_btn = ui_page_hit(g_page, x, y);
        if (g_press_btn >= 0) {
            render_button_state(g_press_btn);
        }
        break;
    case SCREEN_2048:
        on_2048_press(x, y);
        break;
    case SCREEN_TTT:
    case SCREEN_MS:
    case SCREEN_XQ:
    case SCREEN_TET:
    case SCREEN_SNK:
        /* 只有 chrome 按钮要按下高亮，棋盘由各游戏自己在 release 时判 */
        g_press_btn = ui_page_hit(g_page, x, y);
        if (g_press_btn >= 0) {
            render_button_state(g_press_btn);
        }
        break;
    default:
        break;
    }
}

static void on_release(int x, int y)
{
    int btn = g_press_btn;
    int px = g_press_x, py = g_press_y;
    int was_swipe = g_press_swipe;

    g_pressed = 0;
    g_press_btn = -1;
    g_press_swipe = 0;

    switch (g_screen) {
    case SCREEN_LOGIN:
    case SCREEN_PIN:
    case SCREEN_MAIN:
    case SCREEN_ALBUM:
    case SCREEN_OPTION:
    case SCREEN_GAME: {
        const UiButton *b;
        int hit = ui_page_hit(g_page, x, y);
        if (btn >= 0 && btn == hit) {
            b = ui_page_button(g_page, btn);
            if (b) {
                const char *id = b->id;
                /* 先恢复普通态，再执行动作(动作用的是别的页面) */
                g_press_btn = btn;
                render_button_state(btn);
                g_press_btn = -1;

                if (g_screen == SCREEN_MAIN) {
                    on_main_button(id);
                } else if (g_screen == SCREEN_ALBUM) {
                    on_album_button(id);
                } else if (g_screen == SCREEN_OPTION) {
                    on_option_button(id);
                } else if (g_screen == SCREEN_GAME) {
                    on_game_button(id);
                } else {
                    on_key_button(id);
                }
                return;
            }
        }
        /* 没点中按钮: 左上角当返回 */
        if (btn >= 0) {
            render_button_state(btn);   /* 手指滑出去了，恢复原样 */
        }
        if (x < 200 && y < 100) {
            go_back();
            return;
        }
        /* 登录界面点空白不做事 */
        break;
    }

    case SCREEN_PHOTO:
        if (x >= PHOTO_EXIT_X && x < PHOTO_EXIT_X + PHOTO_EXIT_W &&
            y >= PHOTO_EXIT_Y && y < PHOTO_EXIT_Y + PHOTO_EXIT_H) {
            goto_album();                   /* 右下角退出键: 回相册二级 */
        } else if (x < 200 && y < 100) {    /* 左上角返回相册二级(保留) */
            goto_album();
        } else if (g_mode == MODE_AUTO) {
            /* 自动播放中碰一下屏幕 -> 交给用户手动控制。
             * 这样"自动/手动"是一个连贯的体验: 先自动放，想细看就点一下，
             * 而不是点了之后照片跳一张、用户却还在自动模式里。 */
            g_mode = MODE_MANUAL;
            toast("SWITCHED TO MANUAL");
            photo_render();
        } else if (y < fbh() / 2) {         /* 上半屏 = 下一张 */
            photo_step(+1);
        } else {                            /* 下半屏 = 上一张 */
            photo_step(-1);
        }
        break;

    case SCREEN_2048:
        if (x < 300 && y < 44) {            /* 顶栏左侧返回 */
            goto_game();
        } else if (was_swipe) {
            on_2048_release(px, py, x, y);
        } else {
            /* 点在棋盘上但没滑动: 结束后重开一局 */
            if (g2048_over()) {
                g2048_new();
                game2048_render();
            }
        }
        break;

    case SCREEN_TTT:
    case SCREEN_MS:
    case SCREEN_XQ:
    case SCREEN_TET:
    case SCREEN_SNK: {
        const UiButton *b;
        int hit = ui_page_hit(g_page, x, y);
        if (btn >= 0 && btn == hit) {
            b = ui_page_button(g_page, btn);
            if (b) {
                const char *id = b->id;
                g_press_btn = btn;
                render_button_state(btn);       /* 先恢复普通态 */
                g_press_btn = -1;

                if (ID_IS(id, "g_back")) {
                    goto_game();
                } else if (g_game_ops) {
                    g_game_ops->button(id);
                    g_game_ops->render(-1);
                }
                return;
            }
        }
        if (btn >= 0) {
            render_button_state(btn);           /* 手指滑出去了 */
        }
        if (x < 200 && y < 100) {               /* 左上角当返回 */
            goto_game();
            return;
        }
        if (g_game_ops) {
            g_game_ops->tap(x, y);              /* 棋盘上的点击 */
        }
        break;
    }

    default:
        break;
    }
}

/* ------------------------------------------------------------------ 资源加载 */
static const char *MAIN_IDS[6] = { "main_album", "main_game", "main_media",
                                   "main_other", "main_exit", "main_opt" };
static const char *ALBUM_IDS[4] = { "album_manual", "album_auto",
                                    "album_exit", "album_opt" };
static const char *OPTION_IDS[4] = { "opt_1s", "opt_3s", "opt_5s", "opt_10s" };
static const char *GAME_IDS[6] = { "game_ttt", "game_2048", "game_ms",
                                    "game_xq", "game_tetris", "game_snake" };

/* chrome 按钮: 六个游戏共用同一套坐标(和 gen_games.py 一致) */
#define GC_BACK_N   2
static const char *GC_BACK_IDS[GC_BACK_N] = { "g_back", "g_again" };
static int gc_back_x[GC_BACK_N], gc_back_y[GC_BACK_N];
static int gc_back_w[GC_BACK_N] = { G_BACK_W, G_AGAIN_W };
static int gc_back_h[GC_BACK_N] = { G_BACK_H, G_AGAIN_H };

/* 扫雷多一个"插旗" */
#define GC_MS_N   3
static const char *GC_MS_IDS[GC_MS_N] = { "g_back", "g_extra", "g_again" };
static int gc_ms_x[GC_MS_N], gc_ms_y[GC_MS_N];
static int gc_ms_w[GC_MS_N] = { G_BACK_W, G_EXTRA_W, G_AGAIN_W };
static int gc_ms_h[GC_MS_N] = { G_BACK_H, G_EXTRA_H, G_AGAIN_H };

/* 俄罗斯方块多四个操作键 */
#define GC_TET_N   6
static const char *GC_TET_IDS[GC_TET_N] = { "g_back", "g_left", "g_rot",
                                            "g_right", "g_drop", "g_again" };
static int gc_tet_x[GC_TET_N], gc_tet_y[GC_TET_N];
static int gc_tet_w[GC_TET_N] = { G_BACK_W, G_DIR_W, G_DIR_W, G_DIR_W, G_DROP_W, G_AGAIN_W };
static int gc_tet_h[GC_TET_N] = { G_BACK_H, G_DIR_H, G_DIR_H, G_DIR_H, G_DIR_H, G_AGAIN_H };

static void gc_init(void)
{
    gc_back_x[0] = G_BACK_X;  gc_back_y[0] = G_BACK_Y;
    gc_back_x[1] = G_AGAIN_X; gc_back_y[1] = G_AGAIN_Y;

    gc_ms_x[0] = G_BACK_X;  gc_ms_y[0] = G_BACK_Y;
    gc_ms_x[1] = G_EXTRA_X; gc_ms_y[1] = G_EXTRA_Y;
    gc_ms_x[2] = G_AGAIN_X; gc_ms_y[2] = G_AGAIN_Y;

    gc_tet_x[0] = G_BACK_X;  gc_tet_y[0] = G_BACK_Y;
    gc_tet_x[1] = G_DL_X;    gc_tet_y[1] = G_DIR_Y;
    gc_tet_x[2] = G_ROT_X;   gc_tet_y[2] = G_DIR_Y;
    gc_tet_x[3] = G_DR_X;    gc_tet_y[3] = G_DIR_Y;
    gc_tet_x[4] = G_DROP_X;  gc_tet_y[4] = G_DIR_Y;
    gc_tet_x[5] = G_AGAIN_X; gc_tet_y[5] = G_AGAIN_Y;
}

static const char *KEY_IDS[12] = { "key_1", "key_2", "key_3",
                                   "key_4", "key_5", "key_6",
                                   "key_7", "key_8", "key_9",
                                   "key_del", "key_0", "key_ok" };

static const int OPTION_W[4] = { 220, 220, 220, 220 };
static const int OPTION_H[4] = { 150, 150, 150, 150 };
static const int OPTION_X0 = (1024 - (4 * 220 + 3 * 36)) / 2;
static const int OPTION_Y0 = 170;

static int g_opt_x[4], g_opt_y[4];

static void load_all_assets(void)
{
    layout_init();

    for (int i = 0; i < 4; i++) {
        g_opt_x[i] = OPTION_X0 + i * (220 + 36);
        g_opt_y[i] = OPTION_Y0;
    }

    /* 主界面 */
    {
        int w[6], h[6];
        for (int i = 0; i < 6; i++) { w[i] = MAIN_CARD_W; h[i] = MAIN_CARD_H; }
        ui_page_load(&g_page_main, g_asset_dir, "main_menu", MAIN_IDS,
                     main_btn_x, main_btn_y, w, h, 6);
    }
    /* 相册二级 */
    {
        int w[4], h[4];
        for (int i = 0; i < 4; i++) { w[i] = ALBUM_CARD_W; h[i] = ALBUM_CARD_H; }
        ui_page_load(&g_page_album, g_asset_dir, "album_menu", ALBUM_IDS,
                     album_btn_x, album_btn_y, w, h, 4);
    }
    /* 选项 */
    ui_page_load(&g_page_option, g_asset_dir, "option_menu", OPTION_IDS,
                 g_opt_x, g_opt_y, OPTION_W, OPTION_H, 4);
    /* 游戏菜单(六个入口) */
    {
        int w[6], h[6];
        for (int i = 0; i < 6; i++) { w[i] = GAME_CARD_W; h[i] = GAME_CARD_H; }
        ui_page_load(&g_page_game, g_asset_dir, "game_menu", GAME_IDS,
                     game_btn_x, game_btn_y, w, h, 6);
    }
    /* 登录 + 键盘 */
    ui_page_load(&g_page_login, g_asset_dir, "lock", KEY_IDS,
                 key_btn_x, key_btn_y, NULL, NULL, 12);
    ui_page_load(&g_page_pin, g_asset_dir, "lock", KEY_IDS,
                 key_btn_x, key_btn_y, NULL, NULL, 12);

    /* 五个新游戏的页面: 背景 + chrome 按钮 */
    gc_init();
    ui_page_load(&g_page_ttt, g_asset_dir, "ttt_bg", GC_BACK_IDS,
                 gc_back_x, gc_back_y, gc_back_w, gc_back_h, GC_BACK_N);
    ui_page_load(&g_page_ms, g_asset_dir, "ms_bg", GC_MS_IDS,
                 gc_ms_x, gc_ms_y, gc_ms_w, gc_ms_h, GC_MS_N);
    ui_page_load(&g_page_xq, g_asset_dir, "xq_bg", GC_BACK_IDS,
                 gc_back_x, gc_back_y, gc_back_w, gc_back_h, GC_BACK_N);
    ui_page_load(&g_page_tet, g_asset_dir, "tetris_bg", GC_TET_IDS,
                 gc_tet_x, gc_tet_y, gc_tet_w, gc_tet_h, GC_TET_N);
    ui_page_load(&g_page_snk, g_asset_dir, "snake_bg", GC_BACK_IDS,
                 gc_back_x, gc_back_y, gc_back_w, gc_back_h, GC_BACK_N);

    gs_set_dir(g_asset_dir);

    /* 2048 牌面 */
    for (int e = 0; e <= G2048_MAX_EXP; e++) {
        char name[32];
        snprintf(name, sizeof(name), "tile_%d.bmp", e == 0 ? 0 : (1 << e));
        g_tile[e] = load_asset(name);
        if (!g_tile[e] && e > 0) {
            /* 高数值可能没有专属图，用 4096 那张兜底 */
            g_tile[e] = g_tile[G2048_MAX_EXP] ? g_tile[G2048_MAX_EXP] : NULL;
        }
    }
    g_tile_bg = load_asset("tile_bg.bmp");
    g_2048_bg = load_asset("g2048_bg.bmp");
    g_no_photo = load_asset("no_photo.bmp");
    /* 相册播放页右下角常驻的"退出"键(手动/自动都有) */
    g_photo_exit = load_asset("btn_photo_exit.bmp");
    g_photo_exit_p = load_asset("btn_photo_exit_p.bmp");

    printf("[ui] 页面按钮加载情况: 主界面 %d/6, 相册 %d/4, 选项 %d/4, 游戏 %d/6, "
           "键盘 %d/12, 井字棋 %d/2, 扫雷 %d/3, 象棋 %d/2, 俄罗斯方块 %d/6, 贪吃蛇 %d/2\n",
           g_page_main.loaded, g_page_album.loaded, g_page_option.loaded,
           g_page_game.loaded, g_page_login.loaded,
           g_page_ttt.loaded, g_page_ms.loaded, g_page_xq.loaded,
           g_page_tet.loaded, g_page_snk.loaded);
    if (g_page_main.loaded == 0) {
        printf("[ui] 注意: 没有加载到独立按钮图，将只显示背景。\n"
               "     如果背景是烧好按钮的 main_menu.bmp，那也能正常用；\n"
               "     否则请先跑 tools/gen_buttons.py。\n");
    }
}

static void free_all_assets(void)
{
    ui_page_free(&g_page_login);
    ui_page_free(&g_page_pin);
    ui_page_free(&g_page_main);
    ui_page_free(&g_page_album);
    ui_page_free(&g_page_option);
    ui_page_free(&g_page_game);
    ui_page_free(&g_page_ttt);
    ui_page_free(&g_page_ms);
    ui_page_free(&g_page_xq);
    ui_page_free(&g_page_tet);
    ui_page_free(&g_page_snk);
    for (int i = 0; i < g_photo_count; i++) {
        image_free(g_photos[i]);
        g_photos[i] = NULL;
    }
    /* tile 数组里可能有别名(高数值兜底指向 4096 那张)，去重后再释放，
     * 否则会 double free —— 这种崩溃在板上只表现为"退出时死机"，很难查。 */
    for (int e = 0; e <= G2048_MAX_EXP; e++) {
        Image *im = g_tile[e];
        if (!im) {
            continue;
        }
        for (int j = e + 1; j <= G2048_MAX_EXP; j++) {
            if (g_tile[j] == im) {
                g_tile[j] = NULL;
            }
        }
        image_free(im);
        g_tile[e] = NULL;
    }
    image_free(g_tile_bg);
    image_free(g_no_photo);
    image_free(g_undo_icon);
    image_free(g_photo_exit);
    image_free(g_photo_exit_p);
}

/* ------------------------------------------------------------------ main */
static void usage(const char *argv0)
{
    printf("用法: %s [图片目录] [触摸设备] [账户配置]\n", argv0);
    printf("  图片目录 默认 %s\n", ASSET_DIR_DEFAULT);
    printf("  触摸设备 默认自动扫描 /dev/input/event* (也可用环境变量 TOUCH_DEV 指定)\n");
    printf("  账户配置 默认 %s\n", ACCOUNT_CFG_DEFAULT);
    printf("\n例: ./main                      # 全默认，触摸自动识别\n");
    printf("    ./main bmp /dev/input/event1  # 触摸强制指定\n");
}

int main(int argc, char **argv)
{
    const char *touch_dev = TOUCH_DEV_DEFAULT;
    const char *cfg = ACCOUNT_CFG_DEFAULT;

    /*
     * 行缓冲。
     * 程序日志是给现场排查用的：重定向到文件时默认是全缓冲，
     * 一旦异常退出或被 kill，最后一段日志就丢了，正好是需要的那段。
     */
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        g_asset_dir = argv[1];
    }
    if (argc > 2) {
        touch_dev = argv[2];
    }
    if (argc > 3) {
        cfg = argv[3];
    }

    printf("=== 电子相册 (RK1808 / DRM / 1024x600) ===\n");

    if (display_init() != 0) {
        fprintf(stderr, "显示初始化失败\n");
        return 1;
    }
    printf("[main] 屏幕 %dx%d pitch=%d\n", display_width(), display_height(),
           display_pitch());

    load_all_assets();

    if (g_page_main.bg == NULL && g_page_main.loaded == 0) {
        fprintf(stderr, "致命: 连主界面背景都加载不到，请检查图片目录 %s\n", g_asset_dir);
        display_close();
        return 1;
    }

    if (load_photo_list() != 0) {
        printf("[album] %s 里没有 photo_*.bmp，相册将显示占位提示\n", g_asset_dir);
        g_photo_count = 0;
    } else {
        printf("[album] 发现 %d 张照片\n", g_photo_count);
    }

    account_init(cfg);

    if (touch_init(touch_dev) != 0) {
        fprintf(stderr, "触摸初始化失败，仍继续(可用 Ctrl+C 结束)\n");
    } else {
        printf("[main] 触摸设备: %s\n", touch_device_path() ? touch_device_path() : "(未知)");
    }
    touch_set_screen(display_width(), display_height());

    /* 第一屏: 密码锁 */
    goto_login();

    while (g_running) {
        int x = 0, y = 0;
        TouchEvent ev = touch_poll(&x, &y, TICK_MS);

        if (ev == TOUCH_EV_ERROR) {
            printf("[touch] 设备读取失败，退出\n");
            break;
        }
        if (ev == TOUCH_EV_PRESS) {
            printf("[input] PRESS (%d,%d) screen=%d\n", x, y, (int)g_screen);
            on_press(x, y);
        } else if (ev == TOUCH_EV_RELEASE) {
            printf("[input] RELEASE (%d,%d) screen=%d\n", x, y, (int)g_screen);
            on_release(x, y);
        }

        /* 自动播放: 每 interval 毫秒切一张 */
        if (g_screen == SCREEN_PHOTO && g_mode == MODE_AUTO && g_photo_count > 0) {
            if (now_ms() - g_last_switch_ms >= g_interval_ms) {
                photo_step(+1);
            }
        }

        /* 五个新游戏的定时逻辑(俄罗斯方块下落、贪吃蛇移动) */
        if (g_game_ops) {
            switch (g_screen) {
            case SCREEN_TTT:
            case SCREEN_MS:
            case SCREEN_XQ:
            case SCREEN_TET:
            case SCREEN_SNK:
                g_game_ops->tick(now_ms());
                break;
            default:
                break;
            }
        }

        /* 提示条到点消失: 重画当前界面 */
        if (g_toast_until_ms && !toast_active()) {
            g_toast_until_ms = 0;
            switch (g_screen) {
            case SCREEN_LOGIN:  login_render();      break;
            case SCREEN_PIN:    pin_render();        break;
            case SCREEN_MAIN:
            case SCREEN_ALBUM:
            case SCREEN_OPTION:
            case SCREEN_GAME:   render_page();       break;
            case SCREEN_PHOTO:  photo_render();      break;
            case SCREEN_2048:   game2048_render();   break;
            case SCREEN_TTT:
            case SCREEN_MS:
            case SCREEN_XQ:
            case SCREEN_TET:
            case SCREEN_SNK:
                if (g_game_ops) {
                    g_game_ops->render(-1);
                }
                break;
            default: break;
            }
        }
    }

    printf("[main] 清理退出\n");
    fflush(stdout);
    touch_close();
    free_all_assets();
    display_close();
    return 0;
}

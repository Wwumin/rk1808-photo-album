/*
 * test/stub/host/touch_stub.c  —  触摸桩实现(PC 冒烟测试用)
 *
 * 顶掉真实的 touch.c（真实版本依赖 linux/input.h，Windows 上编不了）。
 *
 * 脚本格式 (clicks.txt): 每行 "x y 按下前等待毫秒数"
 * 每次脚本点击会被拆成两个事件：
 *   第一次 poll 返回 PRESS，隔 steps_press_to_release 次再返回 RELEASE。
 * 这样主程序里"按下高亮 -> 抬起执行"的流程在 PC 上也能完整走一遍。
 *
 * 另外支持一个特殊脚本: 如果一行写成 "swipe x0 y0 x1 y1 wait_ms"，
 * 就模拟一次滑动(2048 用)。
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "touch.h"
#include "trace.h"

typedef struct {
    int  x0, y0;      /* 按下点 */
    int  x1, y1;      /* 抬起点(点击时和按下点相同) */
    int  wait_ms;     /* 按下前等待 */
    int  is_swipe;
    int  is_wait;     /* "wait N" 行: 只等时不点屏幕 */
} Step;

static Step g_steps[128];
static int  g_n = 0;
static int  g_i = 0;
static int  g_phase = 0;          /* 0 = 该发 PRESS, 1 = 该发 RELEASE */
static int  g_hold_ticks = 2;     /* PRESS 之后隔几次 poll 再 RELEASE */

static int g_scr_w = 1024, g_scr_h = 600;
static int g_script_done_quit = 1;   /* 默认脚本跑完就结束程序(冒烟测试用) */
static int g_done_reported = 0;

void touch_set_screen(int w, int h)
{
    if (w > 0) { g_scr_w = w; }
    if (h > 0) { g_scr_h = h; }
}

int touch_init(const char *device)
{
    static const char *cands[] = { "clicks.txt", "../test/clicks.txt", "test/clicks.txt" };
    FILE *fp = NULL;
    const char *q;
    (void)device;

    /* 默认: 脚本跑完就发一个 ERROR 收尾，避免 make check 挂住。
     * 想手动观察常驻行为(比如调自动播放)时置 SMOKE_NO_END_QUIT=1。 */
    q = getenv("SMOKE_NO_END_QUIT");
    if (q != NULL && q[0] != '0') {
        g_script_done_quit = 0;
    }
    for (size_t k = 0; k < sizeof(cands) / sizeof(cands[0]) && !fp; k++) {
        fp = fopen(cands[k], "r");
        if (fp) {
            trace("    [ts] 点击脚本: %s", cands[k]);
        }
    }
    if (!fp) {
        trace("    [ts] 没有 clicks.txt，只做纯渲染测试");
        return 0;
    }

    g_n = 0;
    {
        char line[256];
        while (g_n < (int)(sizeof(g_steps) / sizeof(g_steps[0])) && fgets(line, sizeof(line), fp)) {
            char *p = line;
            int a, b, c, d, w;
            if ((unsigned char)p[0] == 0xEF) {
                p += 3;                        /* 跳过 UTF-8 BOM */
            }
            while (*p == ' ' || *p == '\t') {
                p++;
            }
            if (*p == '#' || *p == '\r' || *p == '\n' || *p == '\0') {
                continue;
            }
            if (strncmp(p, "wait", 4) == 0) {
                /* 只等待、不点屏幕。用来测"自动播放每 3 秒切换"这类不看触摸的行为 */
                if (sscanf(p + 4, "%d", &a) == 1) {
                    g_steps[g_n].x0 = -1; g_steps[g_n].y0 = -1;
                    g_steps[g_n].x1 = -1; g_steps[g_n].y1 = -1;
                    g_steps[g_n].wait_ms = a;
                    g_steps[g_n].is_swipe = 0;
                    g_steps[g_n].is_wait = 1;
                    g_n++;
                }
            } else if (strncmp(p, "swipe", 5) == 0) {
                if (sscanf(p + 5, "%d %d %d %d %d", &a, &b, &c, &d, &w) == 5) {
                    g_steps[g_n].x0 = a; g_steps[g_n].y0 = b;
                    g_steps[g_n].x1 = c; g_steps[g_n].y1 = d;
                    g_steps[g_n].wait_ms = w;
                    g_steps[g_n].is_swipe = 1;
                    g_steps[g_n].is_wait = 0;
                    g_n++;
                }
            } else if (sscanf(p, "%d %d %d", &a, &b, &w) == 3) {
                g_steps[g_n].x0 = a; g_steps[g_n].y0 = b;
                g_steps[g_n].x1 = a; g_steps[g_n].y1 = b;
                g_steps[g_n].wait_ms = w;
                g_steps[g_n].is_swipe = 0;
                g_steps[g_n].is_wait = 0;
                g_n++;
            }
        }
    }
    fclose(fp);
    trace("    [ts] 载入 %d 次脚本操作 (stub)", g_n);
    return 0;
}

void touch_close(void)
{
    trace("    [ts] 关闭 (stub)");
}

/* PC 桩里没有真实设备，固定返回一个假路径 */
const char *touch_device_path(void)
{
    return "/dev/input/event_stub";
}

/* 单调毫秒时钟 */
static long long monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

TouchEvent touch_poll(int *x, int *y, int timeout_ms)
{
    static int pending_release_ticks = 0;
    static long long deadline_ms = 0;      /* 下一次 PRESS 的绝对时刻 */

    if (g_i >= g_n) {
        (void)timeout_ms;
        /*
         * 脚本跑完就自动收尾。
         * 没有这个开关时，脚本最后一步不点"退出"的话主循环会一直空转
         * (程序本来就是常驻的)，冒烟测试只能等外部超时杀掉，日志还会丢尾巴。
         * 只给测试用：SMOKE_END_QUIT=1 时发一个 ERROR，让主循环走正常退出分支。
         */
        if (g_script_done_quit && !g_done_reported) {
            g_done_reported = 1;
            trace("    [ts] 脚本已跑完，按要求结束程序 (SMOKE_END_QUIT)");
            return TOUCH_EV_ERROR;
        }
        return TOUCH_EV_NONE;
    }

    if (g_steps[g_i].is_wait) {
        long long now = monotonic_ms();
        if (deadline_ms == 0) {
            deadline_ms = now + g_steps[g_i].wait_ms;
            trace("    [ts] 脚本 #%d 空等 %d ms", g_i + 1, g_steps[g_i].wait_ms);
        }
        if (now < deadline_ms) {
            long long left = deadline_ms - now;
            int slice = (int)(left > 50 ? 50 : left);
            struct timespec ts;
            ts.tv_sec  = slice / 1000;
            ts.tv_nsec = (long)(slice % 1000) * 1000000L;
            nanosleep(&ts, NULL);
            return TOUCH_EV_NONE;
        }
        g_i++;
        deadline_ms = 0;
        return TOUCH_EV_NONE;
    }

    if (g_phase == 0) {
        /*
         * 该发 PRESS。
         *
         * 两个要点，都是踩坑得出的:
         * 1) 等待不能一觉睡到底。主循环的"自动播放计时"在 touch_poll() 之外判断，
         *    这里睡死 3 秒，计时器就永远没机会触发，"每 3 秒自动切图"根本测不到。
         *    所以切成小片，每片返回 NONE，把控制权还给主循环。
         * 2) 等待要用**绝对时刻**判断，不要用"剩余时间累减"。
         *    累减的写法一旦遇到 nanosleep 提前返回(EINTR)，剩余时间就不会归零，
         *    于是日志里会看到"开始等待 3400 ms"反复刷屏、PRESS 永远发不出来。
         */
        long long now = monotonic_ms();
        if (deadline_ms == 0) {
            deadline_ms = now + g_steps[g_i].wait_ms;
            trace("    [ts] 脚本 #%d 开始等待 %d ms (截止 %lld)",
                  g_i + 1, g_steps[g_i].wait_ms, deadline_ms);
        }
        if (now < deadline_ms) {
            long long left = deadline_ms - now;
            int slice = (int)(left > 50 ? 50 : left);
            struct timespec ts;
            ts.tv_sec  = slice / 1000;
            ts.tv_nsec = (long)(slice % 1000) * 1000000L;
            nanosleep(&ts, NULL);
            return TOUCH_EV_NONE;
        }

        if (x) { *x = g_steps[g_i].x0; }
        if (y) { *y = g_steps[g_i].y0; }
        trace("    [ts] 脚本 #%d PRESS (%d,%d)%s", g_i + 1,
              g_steps[g_i].x0, g_steps[g_i].y0,
              g_steps[g_i].is_swipe ? " [滑动开始]" : "");
        g_phase = 1;
        pending_release_ticks = g_hold_ticks;
        return TOUCH_EV_PRESS;
    }

    /* 该发 RELEASE */
    if (pending_release_ticks-- > 0) {
        return TOUCH_EV_NONE;      /* 模拟"手按住不放"的几帧 */
    }
    if (x) { *x = g_steps[g_i].x1; }
    if (y) { *y = g_steps[g_i].y1; }
    if (g_steps[g_i].is_swipe) {
        trace("    [ts] 脚本 #%d RELEASE (%d,%d) [滑动结束]",
              g_i + 1, g_steps[g_i].x1, g_steps[g_i].y1);
    }
    g_i++;
    g_phase = 0;
    deadline_ms = 0;               /* 下一步重新计截止时刻 */
    return TOUCH_EV_RELEASE;
}

void touch_last(int *x, int *y)
{
    if (x) { *x = g_i > 0 ? g_steps[g_i - 1].x0 : -1; }
    if (y) { *y = g_i > 0 ? g_steps[g_i - 1].y0 : -1; }
}

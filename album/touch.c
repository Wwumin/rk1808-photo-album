/*
 * touch.c  —  触摸屏输入实现(evdev，非阻塞 + 超时 + 按下/抬起分离)
 *
 * 事件产生方式:
 *   按下                       -> 立刻返回 TOUCH_EV_PRESS
 *   按下期间移动               -> 只更新坐标，不外传(避免刷屏)
 *   抬起(在 SYN_REPORT 之后)    -> 返回 TOUCH_EV_RELEASE
 *
 * 注意: 必须先收到过 PRESS 才可能产生 RELEASE，
 * 否则有些设备开机时上报的一串状态会被误判成"抬手"。
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#include "touch.h"

#ifndef ABS_MT_SLOT
#define ABS_MT_SLOT 0x2f
#endif
#ifndef ABS_MT_POSITION_X
#define ABS_MT_POSITION_X 0x35
#endif
#ifndef ABS_MT_POSITION_Y
#define ABS_MT_POSITION_Y 0x36
#endif
#ifndef ABS_MT_TRACKING_ID
#define ABS_MT_TRACKING_ID 0x39
#endif
#ifndef INPUT_PROP_DIRECT
#define INPUT_PROP_DIRECT 0x01
#endif
#ifndef INPUT_PROP_MAX
#define INPUT_PROP_MAX 0x1f
#endif

#define DEFAULT_TS  "/dev/input/event2"
static int g_fd = -1;
static char g_dev_path[64];       /* 实际打开的设备路径，供日志/排查 */

/* 原始值 -> 屏幕坐标 的映射参数 */
static int g_raw_x0, g_raw_x1;
static int g_raw_y0, g_raw_y1;
static int g_swap_xy;
static int g_invert_x, g_invert_y;

static int g_scr_w = 1024, g_scr_h = 600;

static int g_last_x = -1, g_last_y = -1;

/* 事件状态机 */
static int g_is_down = 0;         /* 手指是否按着 */
static int g_pending_press = 0;   /* 本次 SYN 之后要上报按下 */
static int g_pending_release = 0; /* 本次 SYN 之后要上报抬起 */
static int g_cur_x = -1, g_cur_y = -1;
static int g_down_x = -1, g_down_y = -1;

static int clamp_int(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void touch_read_range(int code, int *lo, int *hi)
{
    unsigned long absbit[((ABS_MAX + 1) + 8 * sizeof(long) - 1) / (8 * sizeof(long))];
    struct input_absinfo info;

    memset(absbit, 0, sizeof(absbit));
    if (ioctl(g_fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0) {
        return;
    }
    if (!(absbit[code / (8 * sizeof(long))] & (1UL << (code % (8 * sizeof(long)))))) {
        return;
    }
    memset(&info, 0, sizeof(info));
    if (ioctl(g_fd, EVIOCGABS(code), &info) < 0) {
        return;
    }
    *lo = info.minimum;
    *hi = info.maximum;
}

void touch_set_screen(int w, int h)
{
    if (w > 0) { g_scr_w = w; }
    if (h > 0) { g_scr_h = h; }
}

/* ------------------------------------------------------------------ 设备自动识别 */
/*
 * 为什么要自动找:
 *     /dev/input/ 下通常有键盘、鼠标、触摸好几个 event 设备，编号还跟
 *     接线/驱动加载顺序有关。硬编码 /dev/input/event2 换一块板子
 *     很可能就是鼠标，程序看起来"完全没反应"。
 *
 * 判定标准(和 test/autodetect.c 里单测过的逻辑一致):
 *     有相对坐标轴(REL_X/REL_Y)        -> 鼠标，一票否决
 *     一个绝对轴都没有                  -> 键盘之类，否决
 *     有 ABS_MT_POSITION_X/Y           -> +100 (多点协议)
 *     有 ABS_X 和 ABS_Y                -> +70  (单点协议)
 *     有 BTN_TOUCH                     -> +20
 * 分数最高的那个就是触摸屏；全是否决则返回失败，让调用方用参数手动指定。
 */

static int bits_long_count(int max_bit)
{
    return (max_bit + 8 * (int)sizeof(unsigned long)) / (8 * (int)sizeof(unsigned long));
}

/*
 * 位掩码数组长度宏: 和设备能力位数组声明处保持一致的算法，
 * 这样 ABS_MAX/KEY_MAX/REL_MAX 变化时不会静默漏位。
 */
#define BITS_LONGS_OF(max_bit) \
    (((max_bit) + 8 * (int)sizeof(unsigned long)) / (8 * (int)sizeof(unsigned long)))

static int bit_is_set(const unsigned long *bits, int bit)
{
    return (bits[bit / (8 * (int)sizeof(unsigned long))] &
            (1UL << (bit % (8 * (int)sizeof(unsigned long))))) ? 1 : 0;
}

/* 探测某个设备的能力，算出"像不像触摸屏"。可以打开的返回 >=0 的分数 */
static int touch_probe_device(const char *path, char *name_out, int name_size)
{
    unsigned long absbit[BITS_LONGS_OF(ABS_MAX)];
    unsigned long keybit[BITS_LONGS_OF(KEY_MAX)];
    unsigned long relbit[BITS_LONGS_OF(REL_MAX)];
    char name[256];
    int fd;
    int has_rel_xy = 0;
    int has_abs_x, has_abs_y, has_mt_x, has_mt_y, has_btn_touch;

    fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return -1;                      /* 不存在或没权限 */
    }

    if (name_out && name_size > 0) {
        name_out[0] = '\0';
        if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
            snprintf(name_out, (size_t)name_size, "%s", name);
        }
    }

    memset(absbit, 0, sizeof(absbit));
    memset(keybit, 0, sizeof(keybit));
    memset(relbit, 0, sizeof(relbit));
    (void)ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit);
    (void)ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit);
    (void)ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relbit)), relbit);
    close(fd);

    has_rel_xy = bit_is_set(relbit, REL_X) || bit_is_set(relbit, REL_Y);
    has_abs_x = bit_is_set(absbit, ABS_X);
    has_abs_y = bit_is_set(absbit, ABS_Y);
    has_mt_x = bit_is_set(absbit, ABS_MT_POSITION_X);
    has_mt_y = bit_is_set(absbit, ABS_MT_POSITION_Y);
    has_btn_touch = bit_is_set(keybit, BTN_TOUCH);

    if (has_rel_xy) {
        return -1;                      /* 鼠标 */
    }
    if (!has_abs_x && !has_abs_y && !has_mt_x && !has_mt_y) {
        return -1;                      /* 键盘 */
    }

    {
        int score = 0;
        if (has_mt_x && has_mt_y) {
            score += 100;
        }
        if (has_abs_x && has_abs_y) {
            score += 70;
        }
        if (has_btn_touch) {
            score += 20;
        }
        return score;
    }
}

/* 扫描 /dev/input/event0..N，返回分数最高的设备路径(静态缓冲)，找不到返回 NULL */
static const char *touch_scan_best(void)
{
    static char best_path[64];
    int best_score = -1;
    int seen = 0;

    best_path[0] = '\0';
    printf("[touch] 未指定设备，扫描 /dev/input/event0..%d 找触摸屏:\n", TOUCH_SCAN_MAX - 1);

    for (int i = 0; i < TOUCH_SCAN_MAX; i++) {
        char path[64], name[256];
        int score;

        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        score = touch_probe_device(path, name, sizeof(name));
        if (score < 0 && name[0] == '\0' && access(path, F_OK) != 0) {
            continue;                   /* 设备不存在，不打印噪音 */
        }
        seen++;
        printf("[touch]   %-22s %-28s %s\n", path,
               name[0] ? name : "(读不到名字)",
               score < 0 ? "不是触摸屏(鼠标/键盘)" : "像触摸屏");
        if (score > best_score) {
            best_score = score;
            snprintf(best_path, sizeof(best_path), "%s", path);
        }
    }

    if (seen == 0) {
        fprintf(stderr, "[touch] /dev/input/ 下没有可读的 event 设备。\n"
                        "        确认内核有 input 子系统，且当前用户能读 /dev/input/event*。\n");
        return NULL;
    }
    if (best_score < 0) {
        fprintf(stderr, "[touch] 扫到的设备里没有触摸屏。\n"
                        "        请用第二个参数手动指定，例如: ./main bmp /dev/input/event1\n"
                        "        查设备用: cat /proc/bus/input/devices\n");
        return NULL;
    }
    printf("[touch] 自动选中 %s\n", best_path);
    return best_path;
}

int touch_init(const char *device)
{
    const char *dev;
    unsigned long propbit[((INPUT_PROP_MAX + 1) + 8 * sizeof(long) - 1) / (8 * sizeof(long))];

    /* 显式指定优先；否则扫描。也认环境变量 TOUCH_DEV，方便不改命令就换设备 */
    if (device && *device && strcmp(device, "auto") != 0) {
        dev = device;
    } else if (device && strcmp(device, "auto") == 0) {
        dev = touch_scan_best();
    } else {
        const char *env = getenv("TOUCH_DEV");
        if (env && *env) {
            printf("[touch] 使用环境变量 TOUCH_DEV=%s\n", env);
            dev = env;
        } else {
            dev = touch_scan_best();
        }
    }
    if (!dev) {
        return -1;
    }
    snprintf(g_dev_path, sizeof(g_dev_path), "%s", dev);

    g_fd = open(dev, O_RDONLY | O_NONBLOCK);
    if (g_fd < 0) {
        fprintf(stderr, "打开触摸设备 %s 失败: %s\n", dev, strerror(errno));
        return -1;
    }

    g_raw_x0 = g_raw_x1 = 0;
    g_raw_y0 = g_raw_y1 = 0;
    touch_read_range(ABS_X, &g_raw_x0, &g_raw_x1);
    touch_read_range(ABS_Y, &g_raw_y0, &g_raw_y1);
    if (g_raw_x1 <= g_raw_x0) {
        touch_read_range(ABS_MT_POSITION_X, &g_raw_x0, &g_raw_x1);
    }
    if (g_raw_y1 <= g_raw_y0) {
        touch_read_range(ABS_MT_POSITION_Y, &g_raw_y0, &g_raw_y1);
    }

    g_swap_xy = g_invert_x = g_invert_y = 0;
    g_is_down = 0;
    g_pending_press = g_pending_release = 0;
    g_cur_x = g_cur_y = -1;

    memset(propbit, 0, sizeof(propbit));
    if (ioctl(g_fd, EVIOCGPROP(sizeof(propbit)), propbit) >= 0) {
        if (!(propbit[INPUT_PROP_DIRECT / (8 * sizeof(long))] &
              (1UL << (INPUT_PROP_DIRECT % (8 * sizeof(long)))))) {
            printf("[touch] 该设备未声明 INPUT_PROP_DIRECT，可能来自鼠标模拟\n");
        }
    }

    printf("[touch] %s 量程 X[%d..%d] Y[%d..%d]\n",
           dev, g_raw_x0, g_raw_x1, g_raw_y0, g_raw_y1);
    if (g_raw_x1 > g_raw_x0) {
        double ratio = (double)(g_raw_y1 - g_raw_y0) / (double)(g_raw_x1 - g_raw_x0);
        printf("[touch] 竖/横量程比 %.3f (屏幕比 %.3f)。\n"
               "        如果点左边亮右边、或者上下颠倒，\n"
               "        改本文件里的 g_swap_xy / g_invert_x / g_invert_y。\n",
               ratio, (double)g_scr_h / (double)g_scr_w);
    }
    return 0;
}

void touch_close(void)
{
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
}

const char *touch_device_path(void)
{
    return g_dev_path[0] ? g_dev_path : NULL;
}

static void map_to_screen(int *x, int *y)
{
    int rx = *x, ry = *y;

    if (g_swap_xy) {
        int t = rx; rx = ry; ry = t;
    }
    if (g_raw_x1 > g_raw_x0) {
        rx = (rx - g_raw_x0) * (g_scr_w - 1) / (g_raw_x1 - g_raw_x0);
    }
    if (g_raw_y1 > g_raw_y0) {
        ry = (ry - g_raw_y0) * (g_scr_h - 1) / (g_raw_y1 - g_raw_y0);
    }
    if (g_invert_x) {
        rx = (g_scr_w - 1) - rx;
    }
    if (g_invert_y) {
        ry = (g_scr_h - 1) - ry;
    }
    *x = clamp_int(rx, 0, g_scr_w - 1);
    *y = clamp_int(ry, 0, g_scr_h - 1);
}

TouchEvent touch_poll(int *x, int *y, int timeout_ms)
{
    struct pollfd pfd;
    struct input_event ev;
    int rc;

    if (g_fd < 0) {
        return TOUCH_EV_ERROR;
    }

    pfd.fd = g_fd;
    pfd.events = POLLIN;
    rc = poll(&pfd, 1, timeout_ms > 0 ? timeout_ms : (timeout_ms < 0 ? -1 : 0));
    if (rc == 0) {
        return TOUCH_EV_NONE;
    }
    if (rc < 0) {
        if (errno == EINTR) {
            return TOUCH_EV_NONE;
        }
        perror("poll(touch)");
        return TOUCH_EV_ERROR;
    }

    /* 把设备里排队的事件一次读干 */
    for (;;) {
        ssize_t n = read(g_fd, &ev, sizeof(ev));
        if (n != (ssize_t)sizeof(ev)) {
            break;
        }

        if (ev.type == EV_ABS) {
            switch (ev.code) {
            case ABS_X:
            case ABS_MT_POSITION_X:
                g_cur_x = ev.value;
                break;
            case ABS_Y:
            case ABS_MT_POSITION_Y:
                g_cur_y = ev.value;
                break;
            case ABS_MT_TRACKING_ID:
                if (ev.value < 0) {
                    if (g_is_down) {
                        g_pending_release = 1;
                    }
                } else if (!g_is_down) {
                    g_pending_press = 1;
                }
                break;
            default:
                break;
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            if (ev.value) {
                if (!g_is_down) {
                    g_pending_press = 1;
                }
            } else if (g_is_down) {
                g_pending_release = 1;
            }
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
            /* 一帧结束: 到这里坐标已经是最新的，可以决定上报什么 */
            if (g_pending_press && !g_is_down) {
                int sx = (g_cur_x >= 0) ? g_cur_x : 0;
                int sy = (g_cur_y >= 0) ? g_cur_y : 0;
                map_to_screen(&sx, &sy);
                g_is_down = 1;
                g_down_x = sx;
                g_down_y = sy;
                g_last_x = sx;
                g_last_y = sy;
                g_pending_press = 0;
                g_pending_release = 0;
                if (x) { *x = sx; }
                if (y) { *y = sy; }
                return TOUCH_EV_PRESS;
            }
            if (g_pending_release && g_is_down) {
                int sx = (g_cur_x >= 0) ? g_cur_x : g_down_x;
                int sy = (g_cur_y >= 0) ? g_cur_y : g_down_y;
                map_to_screen(&sx, &sy);
                g_is_down = 0;
                g_last_x = sx;
                g_last_y = sy;
                g_pending_release = 0;
                g_cur_x = g_cur_y = -1;
                if (x) { *x = sx; }
                if (y) { *y = sy; }
                return TOUCH_EV_RELEASE;
            }
        }
    }
    return TOUCH_EV_NONE;
}

void touch_last(int *x, int *y)
{
    if (x) { *x = g_last_x; }
    if (y) { *y = g_last_y; }
}

/*
 * touch.h  —  触摸屏输入 (电子相册)
 *
 * 和 share 里 ts.c 的两个关键区别:
 *
 * 1) ts.c 的 get_xy() 是"死等"的 —— 不碰屏幕就永远卡在 read() 里，
 *    因此做不到"每 3 秒自动切图，同时随时响应触摸"。
 *    这里改成 poll() 带超时返回，主循环才能一边计时一边收触摸。
 *
 * 2) 这里把触摸拆成"按下"和"抬起"两个事件，而不是"按下并抬起"一次上报。
 *    因为按钮要有按下高亮的反馈，就必须知道手指什么时候按下去；
 *    而且拆开之后，"按住拖动 = 滑动手势"(2048 要用) 才有可能实现。
 *
 * 兼容两种协议:
 *    A 协议(单点) : ABS_X / ABS_Y (+ BTN_TOUCH)
 *    B 协议(多点) : ABS_MT_POSITION_X / ABS_MT_POSITION_Y / ABS_MT_TRACKING_ID
 * 并读取设备上报的量程，把原始值线性映射到屏幕坐标。
 */

#ifndef ALBUM_TOUCH_H
#define ALBUM_TOUCH_H

typedef enum {
    TOUCH_EV_NONE = 0,      /* 超时，没有事件 */
    TOUCH_EV_PRESS,         /* 手指按下 */
    TOUCH_EV_RELEASE,       /* 手指抬起 */
    TOUCH_EV_MOVE,          /* 按下期间移动(当前实现会合并掉，只做滑动判定用) */
    TOUCH_EV_ERROR
} TouchEvent;

/* 打开触摸设备。device 传 NULL 或 "auto" 时自动扫描 /dev/input/event* 找触摸屏 */
int  touch_init(const char *device);
void touch_close(void);

/* 当前实际打开的设备路径(自动识别后可以打印出来)。没打开返回 NULL */
const char *touch_device_path(void);

#define TOUCH_SCAN_MAX 32      /* 自动扫描 /dev/input/event0..N 的上限 */

/*
 * 取一个触摸事件。
 *   timeout_ms : 0 立即返回，<0 一直等
 *   (x,y)      : 屏幕坐标(PRESS / RELEASE / MOVE 时有效)
 */
TouchEvent touch_poll(int *x, int *y, int timeout_ms);

/* 最近一次事件的坐标(调试用) */
void touch_last(int *x, int *y);

/* 注入屏幕尺寸，供原始坐标线性映射(必须在 touch_init 后、使用前调用) */
void touch_set_screen(int w, int h);

#endif /* ALBUM_TOUCH_H */

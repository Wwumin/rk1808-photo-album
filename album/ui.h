/*
 * ui.h  —  界面骨架: 背景图 + 独立按钮(带按下态)
 *
 * 设计意图(为什么要单独做一层):
 *   1. 最初所有按钮都"烧"在背景大图里，加一个按钮就得重出一整张 1024x600 的图，
 *      而且没法做"按下高亮"。现在按钮是独立小图(296x190 / 452x190 等)，
 *      按下时叠一张 _p 图，手感立刻不一样，改文案也不用动背景。
 *   2. 背景图只画标题和光带，按钮全部由这一层负责，布局集中在一处。
 *   3. 为了兼容，背景既支持 xxx.bg.bmp(纯背景 + 独立按钮)，也支持
 *      老的 main_menu.bmp(按钮烧在图里，此时按钮列表为空)。
 *
 * 热区坐标系: 屏幕绝对坐标，和 tools/gen_buttons.py 打印的布局完全一致。
 */

#ifndef ALBUM_UI_H
#define ALBUM_UI_H

#include <stdint.h>
#include "image.h"

#define UI_MAX_BTN 24

typedef struct {
    const char *id;        /* 按钮 id，对应 btn_<id>.bmp / btn_<id>_p.bmp */
    int         x, y;
    int         w, h;      /* 0 表示用图片自带尺寸 */
    int         has_image; /* 0 = 只有热区，没有独立按钮图(背景里已经烧好了) */
    Image      *normal;
    Image      *press;     /* 可能为 NULL(没有按下态图) */
} UiButton;

typedef struct {
    Image     *bg;
    UiButton   btn[UI_MAX_BTN];
    int        n;          /* 热区个数，始终等于传入的 n */
    int        loaded;     /* 真正加载到按钮图的个数 */
} UiPage;

/* 从 <dir>/<name> 加载一张图，失败返回 NULL(只打印一次警告) */
Image *ui_load(const char *dir, const char *name);

/* 加载一个页面的背景与按钮。btn_ids 为 NULL 或 n==0 时只加载背景 */
int  ui_page_load(UiPage *p, const char *dir, const char *bg_name,
                  const char *const *btn_ids, const int *btn_x, const int *btn_y,
                  const int *btn_w, const int *btn_h, int n);

void ui_page_free(UiPage *p);

/* 把页面画到帧缓冲。active: 当前被按住的按钮下标，-1 表示没有 */
void ui_page_draw(const UiPage *p, int active);

/*
 * 拆开的两步，给小游戏用:
 *   小游戏的画面是"背景 -> 动态内容(棋盘/蛇/方块) -> 按钮"，
 *   动态内容必须夹在中间，不能像普通页面那样一步画完。
 *   这两步都不刷新屏幕，调用方最后自己 display_present()。
 */
void ui_page_draw_bg(const UiPage *p);
void ui_page_draw_buttons(const UiPage *p, int active);

/* 命中测试，返回按钮下标，未命中返回 -1 */
int  ui_page_hit(const UiPage *p, int x, int y);

/* 取按钮信息(越界返回 NULL) */
const UiButton *ui_page_button(const UiPage *p, int i);

#endif /* ALBUM_UI_H */

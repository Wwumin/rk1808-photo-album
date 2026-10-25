/*
 * ui.c  —  界面骨架实现
 */

#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "display.h"

#define PATH_MAX_LEN 512

static void make_path(char *out, size_t n, const char *dir, const char *name)
{
    snprintf(out, n, "%s/%s", dir, name);
}

Image *ui_load(const char *dir, const char *name)
{
    char path[PATH_MAX_LEN];
    make_path(path, sizeof(path), dir, name);
    return image_load_bmp(path);
}

/*
 * 先探一下文件在不在。
 * 背景图有 "xxx.bg.bmp 优先、xxx.bmp 兜底" 的两段式查找，
 * 第一段落空是正常情况；直接调加载器会往 stderr 刷一堆
 * "No such file or directory"，把真正的错误刷没了。
 */
static int ui_file_exists(const char *dir, const char *name)
{
    char path[PATH_MAX_LEN];
    FILE *fp;

    make_path(path, sizeof(path), dir, name);
    fp = fopen(path, "rb");
    if (fp) {
        fclose(fp);
        return 1;
    }
    return 0;
}

int ui_page_load(UiPage *p, const char *dir, const char *bg_name,
                 const char *const *btn_ids, const int *btn_x, const int *btn_y,
                 const int *btn_w, const int *btn_h, int n)
{
    char name[128];

    memset(p, 0, sizeof(*p));
    if (n > UI_MAX_BTN) {
        n = UI_MAX_BTN;
    }

    /* 背景: 先找 xxx.bg.bmp(纯背景)，找不到再退回老的整体图 */
    if (bg_name) {
        snprintf(name, sizeof(name), "%s.bg.bmp", bg_name);
        if (ui_file_exists(dir, name)) {
            p->bg = ui_load(dir, name);
        }
        if (!p->bg) {
            snprintf(name, sizeof(name), "%s.bmp", bg_name);
            p->bg = ui_load(dir, name);
        }
    }

    for (int i = 0; i < n; i++) {
        UiButton *b = &p->btn[p->n];
        b->id = btn_ids[i];
        b->x = btn_x[i];
        b->y = btn_y[i];
        b->w = btn_w ? btn_w[i] : 0;
        b->h = btn_h ? btn_h[i] : 0;

        snprintf(name, sizeof(name), "btn_%s.bmp", btn_ids[i]);
        b->normal = ui_load(dir, name);
        snprintf(name, sizeof(name), "btn_%s_p.bmp", btn_ids[i]);
        b->press = ui_load(dir, name);

        if (b->normal) {
            b->has_image = 1;
            if (!btn_w || btn_w[i] == 0) {
                b->w = b->normal->w;
            }
            if (!btn_h || btn_h[i] == 0) {
                b->h = b->normal->h;
            }
            p->loaded++;
        } else {
            /* 没有独立按钮图(例如只有"按钮烧在图里"的老背景)，
             * 热区照样保留，只是画的时候跳过 —— 见 ui_page_draw() */
            b->has_image = 0;
            if (b->w == 0 || b->h == 0) {
                fprintf(stderr, "[ui] 警告: 按钮 %s 无图且无尺寸，热区无效\n", btn_ids[i]);
            }
        }
        p->n++;
    }
    return 0;
}

void ui_page_free(UiPage *p)
{
    image_free(p->bg);
    p->bg = NULL;
    for (int i = 0; i < p->n; i++) {
        image_free(p->btn[i].normal);
        image_free(p->btn[i].press);
        p->btn[i].normal = NULL;
        p->btn[i].press = NULL;
    }
    p->n = 0;
    p->loaded = 0;
}

void ui_page_draw_bg(const UiPage *p)
{
    if (p->bg) {
        image_blit(display_backbuffer(), display_pitch(),
                   display_width(), display_height(), p->bg, 0, 0);
    }
}

void ui_page_draw_buttons(const UiPage *p, int active)
{
    uint8_t *fb = display_backbuffer();
    int pitch = display_pitch();
    int fw = display_width(), fh = display_height();

    for (int i = 0; i < p->n; i++) {
        const UiButton *b = &p->btn[i];
        if (!b->has_image) {
            continue;
        }
        const Image *im = (i == active && b->press) ? b->press : b->normal;
        if (im) {
            image_blit_alpha(fb, pitch, fw, fh, im, b->x, b->y);
        }
    }
}

void ui_page_draw(const UiPage *p, int active)
{
    ui_page_draw_bg(p);
    ui_page_draw_buttons(p, active);
    display_present();
}

int ui_page_hit(const UiPage *p, int x, int y)
{
    for (int i = 0; i < p->n; i++) {
        const UiButton *b = &p->btn[i];
        if (x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h) {
            return i;
        }
    }
    return -1;
}

const UiButton *ui_page_button(const UiPage *p, int i)
{
    if (i < 0 || i >= p->n) {
        return NULL;
    }
    return &p->btn[i];
}

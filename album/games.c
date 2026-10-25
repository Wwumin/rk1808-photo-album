/*
 * games.c  —  小游戏公共小工具
 *
 * 这里的三个函数都很薄，存在的意义是保证六个游戏的状态行位置、配色、
 * 资源目录拼法完全一致 —— 否则改一次就要改六遍。
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "games.h"
#include "display.h"
#include "draw.h"
#include "image.h"

#define GS_STATUS_MAX 96

static char g_status[GS_STATUS_MAX];

void gs_status(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_status, sizeof(g_status), fmt, ap);
    va_end(ap);
}

void gs_draw_status(void)
{
    if (g_status[0] == '\0') {
        return;
    }
    draw_fill_rect(display_backbuffer(), display_pitch(), display_width(),
                   display_height(), 0, G_STATUS_Y - 4, display_width(), 20,
                   BGRA(0, 0, 0), 150);
    draw_text(display_backbuffer(), display_pitch(), display_width(),
              display_height(), 12, G_STATUS_Y, g_status, BGRA(210, 220, 235), 1);
}

/* 保留给各游戏: 从资源目录加载一张图(路径拼法统一) */
static const char *g_dir = "bmp";

void gs_set_dir(const char *dir)
{
    g_dir = dir;
}

Image *gs_load(const char *dir, const char *name)
{
    char path[512];
    const char *d = dir ? dir : g_dir;
    snprintf(path, sizeof(path), "%s/%s", d, name);
    return image_load_bmp(path);
}

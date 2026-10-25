/*
 * display.c  —  DRM 显示模块实现
 */

/*
 * O_CLOEXEC 在 glibc 上属于 POSIX 2008 接口, -std=c11 且没打开 POSIX 宏时
 * 不会声明(编译报 'O_CLOEXEC' undeclared)。Makefile 里已经统一加了
 * -D_POSIX_C_SOURCE=200809L, 这里再写一道, 单独编译本文件时也不会挂。
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/*
 * O_CLOEXEC: glibc 在 POSIX 2008 下才有; 个别环境(老 glibc / MinGW 测桩)
 * 完全没有定义。本程序是常驻进程、从不 fork/exec, 这个标志纯属顺手,
 * 拿不到就退化成 0, 不影响功能, 也免得为一个装饰性标志卡住编译。
 */
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#include "display.h"
#include "inc/DRMwrap.h"

#define DRM_DEVICE  "/dev/dri/card0"

static int  g_fd = -1;
static struct drmHandle g_drm;
static int  g_ready = 0;

int display_init(void)
{
#if defined(DRM_STUB_BUILD)
    /* PC 端测试: 显存是一块普通内存，没有真的 /dev/dri 可开 */
    g_fd = 0;
#else
    g_fd = open(DRM_DEVICE, O_RDWR | O_CLOEXEC);
    if (g_fd < 0) {
        perror("open " DRM_DEVICE " failed");
        return -1;
    }
#endif

    DRMinit(g_fd);

    if (DRMcreateFB(g_fd, &g_drm) != 0) {
        fprintf(stderr, "DRMcreateFB 失败\n");
        close(g_fd);
        g_fd = -1;
        return -1;
    }

    g_ready = 1;
    printf("[display] 分辨率 %u x %u，行距 %u 字节，色深 %d bpp\n",
           g_drm.width, g_drm.height, g_drm.pitch,
           (int)(g_drm.pitch / g_drm.width * 8));

    if (g_drm.pitch != g_drm.width * 4u) {
        printf("[display] 注意: pitch(%u) != width*4(%u)，代码已按 stride 处理\n",
               g_drm.pitch, g_drm.width * 4u);
    }
    return 0;
}

void display_close(void)
{
    if (g_ready) {
        DRMfreeResources(g_fd, &g_drm);
        g_ready = 0;
    }
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
}

int display_width(void)  { return (int)g_drm.width;  }
int display_height(void) { return (int)g_drm.height; }
int display_pitch(void)  { return (int)g_drm.pitch;  }
int display_bpp(void)    { return 4; }

uint8_t *display_backbuffer(void)
{
    return (uint8_t *)g_drm.vaddr;
}

void display_present(void)
{
    DRMshowUp(g_fd, &g_drm);
}

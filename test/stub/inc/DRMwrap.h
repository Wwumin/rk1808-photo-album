/*
 * test/stub/inc/DRMwrap.h  —  仅供 PC 端冒烟测试的桩头文件
 *
 * 板上真正用的是 share/DRM库2/DRM库/inc/DRMwrap.h，
 * 这里只是让 display.c 能在 Windows/MinGW 下编译并跑通逻辑。
 */

#ifndef STUB_DRMWRAP_H
#define STUB_DRMWRAP_H

#include <stdint.h>

/* 让 display.c 知道自己在桩环境里: 不需要打开 /dev/dri/card0 */
#define DRM_STUB_BUILD 1

struct drmHandle {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t handle;
    uint32_t size;
    uint8_t *vaddr;
    uint32_t fb_id;
};

void DRMinit(int fd);
int  DRMcreateFB(int fd, struct drmHandle *drm);
int  DRMshowUp(int fd, struct drmHandle *drm);
void DRMfreeResources(int fd, struct drmHandle *drm);

#endif

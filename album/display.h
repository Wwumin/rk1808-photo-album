/*
 * display.h  —  DRM 显示模块 (电子相册)
 *
 * 封装 share/DRM库2 的 libDRMwrap，向业务代码只暴露"帧缓冲"这一概念。
 *
 * 关键约定(实测确认，勿随意改动):
 *     屏幕分辨率 : 由 DRM connector 读出，实测 1024 x 600
 *     像素格式   : XRGB8888，小端内存序 => 每像素 4 字节 [B][G][R][X]
 *     行距 pitch : 必须用 drm.pitch，不要假设等于 width*4
 */

#ifndef ALBUM_DISPLAY_H
#define ALBUM_DISPLAY_H

#include <stdint.h>

/* 打开 /dev/dri/card0 + DRMinit + DRMcreateFB，成功返回 0 */
int  display_init(void);

/* 释放显存与 FB，关闭设备 */
void display_close(void);

/* 显示的宽 / 高 / 行距(字节) / 每像素字节数 */
int  display_width(void);
int  display_height(void);
int  display_pitch(void);
int  display_bpp(void);

/* 显存首地址，仅供 draw.c 与 blit 使用 */
uint8_t *display_backbuffer(void);

/* 把显存内容推送到屏幕(内部调用 DRMshowUp) */
void display_present(void);

#endif /* ALBUM_DISPLAY_H */

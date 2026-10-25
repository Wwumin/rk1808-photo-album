/*
 * draw.h  —  在显存上画点/矩形/文字 (电子相册 HUD)
 *
 * 背景图里的中文按钮是"烧在图里"的(见 README 的说明)，所以这里只需要
 * ASCII 点阵字库，用来画页码、状态条、文件名这类动态内容。
 */

#ifndef ALBUM_DRAW_H
#define ALBUM_DRAW_H

#include <stdint.h>
#include "image.h"

/* 单个像素，坐标为帧缓冲绝对坐标；越界自动忽略 */
void draw_pixel(uint8_t *dst, int pitch, int fb_w, int fb_h, int x, int y, uint32_t bgra);

/* 填充矩形，带 alpha 混合(就地把 dst 当背景混) */
void draw_fill_rect(uint8_t *dst, int pitch, int fb_w, int fb_h,
                    int x, int y, int w, int h, uint32_t bgra, uint8_t alpha);

/* 1 像素描边矩形 */
void draw_rect(uint8_t *dst, int pitch, int fb_w, int fb_h,
               int x, int y, int w, int h, uint32_t bgra);

/* ASCII 文字，scale 为点阵放大倍数(1 = 8x16) */
void draw_text(uint8_t *dst, int pitch, int fb_w, int fb_h,
               int x, int y, const char *s, uint32_t bgra, int scale);

/* 居中对齐的 ASCII 文字 */
void draw_text_center(uint8_t *dst, int pitch, int fb_w, int fb_h,
                      int cx, int y, const char *s, uint32_t bgra, int scale);

/* 文字像素宽度，便于自己排版 */
int  draw_text_width(const char *s, int scale);

/* 组装一个 BGRA 颜色(小端序下的 32bit 值) */
#define BGRA(b, g, r) ((uint32_t)((b) & 0xFF) | ((uint32_t)((g) & 0xFF) << 8) | \
                       ((uint32_t)((r) & 0xFF) << 16) | 0xFF000000u)

#endif /* ALBUM_DRAW_H */

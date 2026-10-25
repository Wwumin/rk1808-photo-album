/*
 * draw.c  —  基础绘图实现
 */

#include <string.h>

#include "draw.h"
#include "font8x16.h"

static inline int in_bounds(int x, int y, int fb_w, int fb_h)
{
    return x >= 0 && y >= 0 && x < fb_w && y < fb_h;
}

void draw_pixel(uint8_t *dst, int pitch, int fb_w, int fb_h, int x, int y, uint32_t bgra)
{
    if (!dst || !in_bounds(x, y, fb_w, fb_h)) {
        return;
    }
    uint8_t *p = dst + (size_t)y * pitch + (size_t)x * 4;
    p[0] = (uint8_t)(bgra);
    p[1] = (uint8_t)(bgra >> 8);
    p[2] = (uint8_t)(bgra >> 16);
    p[3] = 0x00;
}

void draw_fill_rect(uint8_t *dst, int pitch, int fb_w, int fb_h,
                    int x, int y, int w, int h, uint32_t bgra, uint8_t alpha)
{
    if (!dst || w <= 0 || h <= 0 || alpha == 0) {
        return;
    }
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > fb_w ? fb_w : x + w;
    int y1 = y + h > fb_h ? fb_h : y + h;

    uint8_t sb = (uint8_t)(bgra);
    uint8_t sg = (uint8_t)(bgra >> 8);
    uint8_t sr = (uint8_t)(bgra >> 16);

    for (int yy = y0; yy < y1; yy++) {
        uint8_t *row = dst + (size_t)yy * pitch;
        for (int xx = x0; xx < x1; xx++) {
            uint8_t *p = row + (size_t)xx * 4;
            if (alpha >= 255) {
                p[0] = sb; p[1] = sg; p[2] = sr;
            } else {
                p[0] = (uint8_t)((sb * alpha + p[0] * (255 - alpha)) / 255);
                p[1] = (uint8_t)((sg * alpha + p[1] * (255 - alpha)) / 255);
                p[2] = (uint8_t)((sr * alpha + p[2] * (255 - alpha)) / 255);
            }
            p[3] = 0x00;
        }
    }
}

void draw_rect(uint8_t *dst, int pitch, int fb_w, int fb_h,
               int x, int y, int w, int h, uint32_t bgra)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    draw_fill_rect(dst, pitch, fb_w, fb_h, x, y, w, 1, bgra, 255);
    draw_fill_rect(dst, pitch, fb_w, fb_h, x, y + h - 1, w, 1, bgra, 255);
    draw_fill_rect(dst, pitch, fb_w, fb_h, x, y, 1, h, bgra, 255);
    draw_fill_rect(dst, pitch, fb_w, fb_h, x + w - 1, y, 1, h, bgra, 255);
}

int draw_text_width(const char *s, int scale)
{
    if (!s) {
        return 0;
    }
    if (scale < 1) {
        scale = 1;
    }
    return (int)strlen(s) * FONT8X16_W * scale;
}

void draw_text(uint8_t *dst, int pitch, int fb_w, int fb_h,
               int x, int y, const char *s, uint32_t bgra, int scale)
{
    if (!dst || !s) {
        return;
    }
    if (scale < 1) {
        scale = 1;
    }

    int cx = x;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c < FONT8X16_FIRST || c > FONT8X16_LAST) {
            c = '?';
        }
        const unsigned char *glyph = font8x16[c - FONT8X16_FIRST];
        for (int gy = 0; gy < FONT8X16_H; gy++) {
            unsigned char bits = glyph[gy];
            if (!bits) {
                continue;
            }
            for (int gx = 0; gx < FONT8X16_W; gx++) {
                if (bits & (1u << (7 - gx))) {
                    if (scale == 1) {
                        draw_pixel(dst, pitch, fb_w, fb_h, cx + gx, y + gy, bgra);
                    } else {
                        draw_fill_rect(dst, pitch, fb_w, fb_h,
                                       cx + gx * scale, y + gy * scale,
                                       scale, scale, bgra, 255);
                    }
                }
            }
        }
        cx += FONT8X16_W * scale;
    }
}

void draw_text_center(uint8_t *dst, int pitch, int fb_w, int fb_h,
                      int cx, int y, const char *s, uint32_t bgra, int scale)
{
    draw_text(dst, pitch, fb_w, fb_h, cx - draw_text_width(s, scale) / 2, y, s, bgra, scale);
}

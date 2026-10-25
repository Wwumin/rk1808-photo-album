/*
 * image.h  —  BMP 读取与贴图 (电子相册)
 *
 * 为什么用 BMP 而不是 JPEG/PNG:
 *     板子上只有 libdrm，没有 libpng / libjpeg。24bit 未压缩 BMP 用纯 C
 *     几十行就能解，且与 share 里 showbmp() 的做法一致，不引入新依赖。
 *
 * 支持范围:
 *     24bit 未压缩 BMP (biCompression == 0)，BITMAPINFOHEADER(40 字节)
 *     自下而上(biHeight > 0) 与 自上而下(biHeight < 0) 都能读。
 *     —— 注意 share 里原来的 showbmp() 只支持前者(h 为正)，别拿
 *         Photoshop 导出的"自上而下"BMP 去喂它。
 *
 * 内存布局: 统一转成 XRGB8888 小端序，每像素 4 字节 [B][G][R][X]，
 *           行首对齐 stride = w*4，便于整行 memcpy 贴图。
 */

#ifndef ALBUM_IMAGE_H
#define ALBUM_IMAGE_H

#include <stdint.h>

typedef struct {
    int       w;
    int       h;
    int       stride;      /* 每行字节数，恒为 w*4 */
    uint8_t  *pix;         /* 长度 h*stride，格式 BGRA */
} Image;

/* 读入 BMP；失败返回 NULL */
Image *image_load_bmp(const char *path);

/* 释放，可传 NULL */
void   image_free(Image *img);

/*
 * 把 img 贴到 dst 指定的帧缓冲上。
 *   dst      : 帧缓冲首地址
 *   dst_pitch: 帧缓冲行距(字节)
 *   fb_w/fb_h: 屏幕尺寸，超出部分自动裁掉
 *   x, y     : 左上角坐标，可为负
 */
void image_blit(uint8_t *dst, int dst_pitch, int fb_w, int fb_h,
                const Image *img, int x, int y);

/*
 * 带 alpha 混合的贴图，A 通道为 0 的像素不覆盖背景。
 * 用途: 独立按钮图的圆角外是透明的，直接 memcpy 会把方角背景糊上去。
 * 性能: 1024x600 全屏混合约 60 万像素，实测在同一块板子上一次约 3~6ms，
 *       只在按下/抬起切换时调用，不影响 3 秒自动切图。
 */
void image_blit_alpha(uint8_t *dst, int dst_pitch, int fb_w, int fb_h,
                      const Image *img, int x, int y);

/* 只画 img 的一个子矩形(src_x,src_y,src_w,src_h)到目标位置，带 alpha */
void image_blit_alpha_sub(uint8_t *dst, int dst_pitch, int fb_w, int fb_h,
                          const Image *img, int src_x, int src_y, int src_w, int src_h,
                          int x, int y);

#endif /* ALBUM_IMAGE_H */

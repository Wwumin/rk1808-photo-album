/*
 * image.c  —  BMP 读取与贴图实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image.h"

#define BMP_HEADER_SIZE 54

/* 从文件读 n 字节，短读返回 -1 */
static int read_all(FILE *fp, void *buf, size_t n)
{
    return fread(buf, 1, n, fp) == n ? 0 : -1;
}

Image *image_load_bmp(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror(path);
        return NULL;
    }

    unsigned char head[BMP_HEADER_SIZE];
    if (read_all(fp, head, sizeof(head)) != 0) {
        fprintf(stderr, "%s: 文件太小，不是有效 BMP\n", path);
        fclose(fp);
        return NULL;
    }
    if (head[0] != 'B' || head[1] != 'M') {
        fprintf(stderr, "%s: 缺少 BM 标识\n", path);
        fclose(fp);
        return NULL;
    }

    uint32_t off     = *(uint32_t *)&head[10];   /* 像素数据偏移 */
    uint32_t hdr_sz  = *(uint32_t *)&head[14];   /* 信息头大小 */
    int32_t  w       = *(int32_t  *)&head[18];
    int32_t  h_raw   = *(int32_t  *)&head[22];
    uint16_t planes  = *(uint16_t *)&head[26];
    uint16_t bpp     = *(uint16_t *)&head[28];
    uint32_t comp    = *(uint32_t *)&head[30];

    if (hdr_sz != 40 || planes != 1 || comp != 0) {
        fprintf(stderr, "%s: 只支持 BITMAPINFOHEADER / 未压缩 / 单平面 "
                        "(hdr=%u planes=%u comp=%u)\n", path, hdr_sz, planes, comp);
        fclose(fp);
        return NULL;
    }
    if (bpp != 24 && bpp != 32) {
        fprintf(stderr, "%s: 只支持 24/32bit，当前 %u bpp\n", path, bpp);
        fclose(fp);
        return NULL;
    }
    if (w <= 0 || h_raw == 0) {
        fprintf(stderr, "%s: 宽高非法 (%d x %d)\n", path, w, h_raw);
        fclose(fp);
        return NULL;
    }

    int bottom_up = (h_raw > 0);
    int h         = bottom_up ? h_raw : -h_raw;
    if (h > 8192 || w > 8192) {
        fprintf(stderr, "%s: 尺寸过大 %d x %d\n", path, w, h);
        fclose(fp);
        return NULL;
    }

    /* BMP 每行 4 字节对齐; 32bit 天然对齐 */
    int src_stride = (bpp == 32) ? (w * 4) : (((w * 3) + 3) & ~3);
    long src_size  = (long)src_stride * h;

    unsigned char *src = (unsigned char *)malloc((size_t)src_size);
    if (!src) {
        fprintf(stderr, "%s: 分配 %ld 字节失败\n", path, src_size);
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, (long)off, SEEK_SET) != 0 ||
        read_all(fp, src, (size_t)src_size) != 0) {
        fprintf(stderr, "%s: 像素数据读取失败(期望 %ld 字节)\n", path, src_size);
        free(src);
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    Image *img = (Image *)malloc(sizeof(Image));
    if (!img) {
        free(src);
        return NULL;
    }
    img->w      = w;
    img->h      = h;
    img->stride = w * 4;
    img->pix    = (uint8_t *)malloc((size_t)img->stride * h);
    if (!img->pix) {
        free(src);
        free(img);
        return NULL;
    }

    /* 逐行转换到 [B][G][R][A]
     *   24bit: BMP 里是 BGR，没有透明度 -> A 补 255(不透明)
     *   32bit: 我们的生成器写的就是 BGRA，直接 4 字节拷过去，
     *          圆角外的 alpha=0 得以保留，image_blit_alpha 才有意义。
     */
    for (int y = 0; y < h; y++) {
        const unsigned char *srow = src + (long)(bottom_up ? (h - 1 - y) : y) * src_stride;
        uint8_t             *drow = img->pix + (size_t)y * img->stride;
        if (bpp == 32) {
            memcpy(drow, srow, (size_t)w * 4);
            continue;
        }
        for (int x = 0; x < w; x++) {
            drow[x * 4 + 0] = srow[x * 3 + 0];   /* B */
            drow[x * 4 + 1] = srow[x * 3 + 1];   /* G */
            drow[x * 4 + 2] = srow[x * 3 + 2];   /* R */
            drow[x * 4 + 3] = 0xFF;              /* 不透明 */
        }
    }

    free(src);
    return img;
}

void image_free(Image *img)
{
    if (!img) {
        return;
    }
    free(img->pix);
    free(img);
}

void image_blit(uint8_t *dst, int dst_pitch, int fb_w, int fb_h,
                const Image *img, int x, int y)
{
    if (!dst || !img || !img->pix) {
        return;
    }

    int sx = 0, sy = 0, w = img->w, h = img->h;

    if (x < 0) { sx = -x; w += x; x = 0; }
    if (y < 0) { sy = -y; h += y; y = 0; }
    if (x + w > fb_w) { w = fb_w - x; }
    if (y + h > fb_h) { h = fb_h - y; }
    if (w <= 0 || h <= 0) {
        return;
    }

    for (int row = 0; row < h; row++) {
        const uint8_t *s = img->pix + (size_t)(sy + row) * img->stride + (size_t)sx * 4;
        uint8_t       *d = dst + (size_t)(y + row) * dst_pitch + (size_t)x * 4;
        memcpy(d, s, (size_t)w * 4);
    }
}

/*
 * alpha 混合查表: mul[a][v] = v * a / 255
 * 直接做除法的话每个像素要 3 次除法，1024x600 全屏会很慢；
 * 预计算 256x256 的表(64KB，一次初始化)就只剩乘法和移位。
 */
static uint8_t g_mul[256][256];
static int     g_mul_ready = 0;

static void ensure_mul_table(void)
{
    if (g_mul_ready) {
        return;
    }
    for (int a = 0; a < 256; a++) {
        for (int v = 0; v < 256; v++) {
            g_mul[a][v] = (uint8_t)((v * a + 127) / 255);
        }
    }
    g_mul_ready = 1;
}

void image_blit_alpha_sub(uint8_t *dst, int dst_pitch, int fb_w, int fb_h,
                          const Image *img, int src_x, int src_y, int src_w, int src_h,
                          int x, int y)
{
    int sx = src_x, sy = src_y, w = src_w, h = src_h;

    if (!dst || !img || !img->pix || w <= 0 || h <= 0) {
        return;
    }
    /* 源矩形先裁到图片范围内 */
    if (sx < 0) { w += sx; x -= sx; sx = 0; }
    if (sy < 0) { h += sy; y -= sy; sy = 0; }
    if (sx + w > img->w) { w = img->w - sx; }
    if (sy + h > img->h) { h = img->h - sy; }
    /* 再裁到屏幕范围内 */
    if (x < 0) { sx -= x; w += x; x = 0; }
    if (y < 0) { sy -= y; h += y; y = 0; }
    if (x + w > fb_w) { w = fb_w - x; }
    if (y + h > fb_h) { h = fb_h - y; }
    if (w <= 0 || h <= 0) {
        return;
    }

    ensure_mul_table();

    for (int row = 0; row < h; row++) {
        const uint8_t *s = img->pix + (size_t)(sy + row) * img->stride + (size_t)sx * 4;
        uint8_t       *d = dst + (size_t)(y + row) * dst_pitch + (size_t)x * 4;

        for (int col = 0; col < w; col++) {
            uint8_t a = s[3];
            if (a == 0) {
                /* 完全透明: 保留背景 */
            } else if (a == 255) {
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
            } else {
                uint8_t ia = (uint8_t)(255 - a);
                d[0] = (uint8_t)(g_mul[a][s[0]] + g_mul[ia][d[0]]);
                d[1] = (uint8_t)(g_mul[a][s[1]] + g_mul[ia][d[1]]);
                d[2] = (uint8_t)(g_mul[a][s[2]] + g_mul[ia][d[2]]);
            }
            d[3] = 0x00;
            s += 4;
            d += 4;
        }
    }
}

void image_blit_alpha(uint8_t *dst, int dst_pitch, int fb_w, int fb_h,
                      const Image *img, int x, int y)
{
    if (!img) {
        return;
    }
    image_blit_alpha_sub(dst, dst_pitch, fb_w, fb_h, img, 0, 0, img->w, img->h, x, y);
}

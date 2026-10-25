/*
 * test/stub/host/drm_stub.c  —  DRM 桩实现(PC 冒烟测试用)
 *
 * 申请一块 1024x600x4 的普通内存当"显存"，
 * DRMshowUp() 把这块内存导出成 out/frame_XXXX.bmp，方便在 PC 上肉眼检查画面。
 */

#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif
#include <stdlib.h>
#include <string.h>

#include "inc/DRMwrap.h"
#include "trace.h"

#define STUB_W 1024
#define STUB_H 600

static uint8_t *g_buf = NULL;
static struct drmHandle g_h;
static int g_frame = 0;
static int g_saved = 0;

/*
 * 默认不是每一帧都导出，也不是原尺寸导出。
 * 原因: 全流程冒烟测试会画几百帧，每帧 1024x600x24bit 的 BMP 是 1.76MB，
 *       光写磁盘就要几百兆，整个测试被 IO 拖成几分钟，还很难看出哪里慢。
 * 做法: 每 FRAME_SKIP 帧导出一张，并且缩小到 1/2(512x300)。
 *       要看细节就把环境变量 DRM_STUB_ALL=1 打开，恢复全帧全尺寸。
 */
#define FRAME_SKIP 20
#define DOWNSCALE  2

static int save_all(void)
{
    static int cached = -1;
    if (cached < 0) {
        const char *e = getenv("DRM_STUB_ALL");
        cached = (e && *e && *e != '0') ? 1 : 0;
    }
    return cached;
}

static void write_bmp24(const char *path, const uint8_t *bgra, int w, int h, int pitch)
{
    int sw = save_all() ? 1 : DOWNSCALE;
    int dw = w / sw;
    int dh = h / sw;
    int dst_stride = (dw * 3 + 3) & ~3;
    long pix_bytes = (long)dst_stride * dh;
    long total = 54 + pix_bytes;

    unsigned char head[54];
    memset(head, 0, sizeof(head));
    head[0] = 'B'; head[1] = 'M';
    *(uint32_t *)&head[2]  = (uint32_t)total;
    *(uint32_t *)&head[10] = 54;
    *(uint32_t *)&head[14] = 40;
    *(int32_t  *)&head[18] = dw;
    *(int32_t  *)&head[22] = dh;
    *(uint16_t *)&head[26] = 1;
    *(uint16_t *)&head[28] = 24;
    *(uint32_t *)&head[34] = (uint32_t)pix_bytes;

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror(path);
        return;
    }
    fwrite(head, 1, sizeof(head), fp);

    unsigned char *row = (unsigned char *)malloc((size_t)dst_stride);
    for (int y = dh - 1; y >= 0; y--) {           /* BMP 自下而上 */
        const uint8_t *s = bgra + (size_t)(y * sw) * pitch;
        memset(row, 0, (size_t)dst_stride);
        for (int x = 0; x < dw; x++) {
            const uint8_t *p = s + (size_t)(x * sw) * 4;
            row[x * 3 + 0] = p[0];       /* B */
            row[x * 3 + 1] = p[1];       /* G */
            row[x * 3 + 2] = p[2];       /* R */
        }
        fwrite(row, 1, (size_t)dst_stride, fp);
    }
    free(row);
    fclose(fp);

    /* 顺手算个校验和，确保每次贴图内容确实变了 */
    unsigned long sum = 0;
    for (long i = 0; i < (long)pitch * h; i += 997) {
        sum += bgra[i];
    }
    trace("    [drm] 帧 %d 导出 %s  校验和=%lu", g_frame - 1, path, sum);
}

void DRMinit(int fd)
{
    (void)fd;
    trace("    [drm] DRMinit (stub)");
}

int DRMcreateFB(int fd, struct drmHandle *drm)
{
    (void)fd;

    g_buf = (uint8_t *)calloc((size_t)STUB_W * STUB_H * 4, 1);
    if (!g_buf) {
        return -1;
    }
    g_h.width  = STUB_W;
    g_h.height = STUB_H;
    g_h.pitch  = STUB_W * 4;
    g_h.size   = STUB_W * STUB_H * 4;
    g_h.vaddr  = g_buf;
    g_h.fb_id  = 1;
    *drm = g_h;
    trace("    [drm] 显存 %ux%u pitch=%u (stub)", STUB_W, STUB_H, g_h.pitch);
    return 0;
}

/* out/ 是抽帧输出目录，第一次跑之前它可能不存在(比如 make clean 之后)。
 * 不建的话所有帧都静默写不进去，测试日志一切正常、但 out/ 里空空的。 */
static void ensure_out_dir(void)
{
    static int done = 0;
    if (done) {
        return;
    }
    done = 1;
#ifdef _WIN32
    _mkdir("out");
#else
    mkdir("out", 0755);
#endif
}

int DRMshowUp(int fd, struct drmHandle *drm)
{
    char path[128];
    (void)fd;

    g_frame++;
    if (!save_all() && (g_frame % FRAME_SKIP) != 0) {
        return 0;                   /* 抽帧: 大部分帧不落盘 */
    }
    ensure_out_dir();                /* make clean 之后 out/ 不存在，先建出来 */
    snprintf(path, sizeof(path), "out/frame_%04d.bmp", g_frame);
    write_bmp24(path, drm->vaddr, (int)drm->width, (int)drm->height, (int)drm->pitch);
    g_saved++;
    return 0;
}

void DRMfreeResources(int fd, struct drmHandle *drm)
{
    (void)fd;
    (void)drm;
    free(g_buf);
    g_buf = NULL;
    trace("    [drm] 释放显存 (stub)");
}

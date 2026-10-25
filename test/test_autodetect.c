/*
 * test/test_autodetect.c  —  验证"哪个 event 设备是触摸屏"的判定逻辑
 *
 * 编译: mingw32-make -f Makefile unit
 *
 * 覆盖的场景(都是真实板子上会遇到的):
 *   1. 单点电阻触摸屏   (ABS_X/ABS_Y + BTN_TOUCH)
 *   2. 多点电容触摸屏   (ABS_MT_POSITION_X/Y + BTN_TOUCH)
 *   3. 鼠标             (REL_X/REL_Y，可能也报 BTN_TOUCH) -> 必须排除
 *   4. 键盘             (只有按键，没有任何轴)             -> 必须排除
 *   5. 触摸屏和鼠标同时存在 -> 必须挑中触摸屏，且与顺序无关
 */

#include <stdio.h>
#include <string.h>

#include "autodetect.h"

static int g_fail = 0;

#define CHECK(cond, fmt, ...) do {                                        \
    if (!(cond)) {                                                        \
        printf("  [FAIL] " fmt "\n", ##__VA_ARGS__);                      \
        g_fail++;                                                         \
    } else {                                                              \
        printf("  [ok]   " fmt "\n", ##__VA_ARGS__);                      \
    }                                                                     \
} while (0)

#define N_ABS BITS_LONGS(ABS_MAX)
#define N_KEY BITS_LONGS(KEY_MAX)
#define N_REL BITS_LONGS(REL_Y)

/* 造一个"设备能力" */
typedef struct {
    unsigned long abs[N_ABS];
    unsigned long key[N_ABS > N_KEY ? N_ABS : N_KEY];
    unsigned long rel[N_REL];
    int have_rel;
    const char *name;
} FakeDev;

static void fake_zero(FakeDev *d, const char *name)
{
    memset(d, 0, sizeof(*d));
    d->name = name;
}

static int score_of(const FakeDev *d)
{
    return input_device_is_touch(d->abs, d->key, d->rel, d->have_rel);
}

int main(void)
{
    printf("=== 触摸设备自动识别 逻辑测试 ===\n\n");

    /* ---- 1. 单点电阻触摸屏 ---- */
    printf("单点电阻触摸屏:\n");
    {
        FakeDev d;
        fake_zero(&d, "single-touch");
        input_bits_set(d.abs, ABS_X);
        input_bits_set(d.abs, ABS_Y);
        input_bits_set(d.key, BTN_TOUCH);
        int s = score_of(&d);
        printf("      分数 = %d\n", s);
        CHECK(s > 0, "应当被识别为触摸屏");
        CHECK(s == 90, "分数应为 70(ABS_X/Y) + 20(BTN_TOUCH) = 90");
    }

    /* ---- 2. 多点电容触摸屏 ---- */
    printf("\n多点电容触摸屏:\n");
    {
        FakeDev d;
        fake_zero(&d, "multi-touch");
        input_bits_set(d.abs, ABS_MT_POSITION_X);
        input_bits_set(d.abs, ABS_MT_POSITION_Y);
        input_bits_set(d.abs, ABS_MT_TRACKING_ID);
        input_bits_set(d.key, BTN_TOUCH);
        int s = score_of(&d);
        printf("      分数 = %d\n", s);
        CHECK(s == 120, "分数应为 100(MT) + 20(BTN_TOUCH) = 120");
    }

    /* ---- 3. 鼠标: 必须排除 ---- */
    printf("\n鼠标(有相对轴):\n");
    {
        FakeDev d;
        fake_zero(&d, "mouse");
        input_bits_set(d.rel, REL_X);
        input_bits_set(d.rel, REL_Y);
        input_bits_set(d.key, BTN_TOUCH);      /* 有些驱动就是会乱报 */
        d.have_rel = 1;
        int s = score_of(&d);
        printf("      分数 = %d\n", s);
        CHECK(s < 0, "鼠标必须被排除，不能被当成触摸屏");
    }

    /* ---- 3b. 触摸屏偶发也报相对轴的情况 ---- */
    printf("\n同时有绝对轴和相对轴的设备:\n");
    {
        FakeDev d;
        fake_zero(&d, "hybrid");
        input_bits_set(d.abs, ABS_X);
        input_bits_set(d.abs, ABS_Y);
        input_bits_set(d.rel, REL_X);
        d.have_rel = 1;
        int s = score_of(&d);
        printf("      分数 = %d\n", s);
        CHECK(s < 0, "报了相对轴的一律排除(宁可用参数手动指定，也不要认错设备)");
    }

    /* ---- 4. 键盘: 必须排除 ---- */
    printf("\n键盘(没有任何轴):\n");
    {
        FakeDev d;
        fake_zero(&d, "keyboard");
        input_bits_set(d.key, 0x1e);           /* KEY_A */
        input_bits_set(d.key, 0x39);           /* KEY_SPACE */
        int s = score_of(&d);
        printf("      分数 = %d\n", s);
        CHECK(s < 0, "键盘必须被排除");
    }

    /* ---- 5. 候选挑优: 顺序无关 ---- */
    printf("\n从多个候选里挑最优:\n");
    {
        FakeDev kb, mouse, touch;
        int scores[3];

        fake_zero(&kb, "keyboard");
        input_bits_set(kb.key, 0x1e);
        fake_zero(&mouse, "mouse");
        input_bits_set(mouse.rel, REL_X);
        input_bits_set(mouse.rel, REL_Y);
        mouse.have_rel = 1;
        fake_zero(&touch, "touch");
        input_bits_set(touch.abs, ABS_X);
        input_bits_set(touch.abs, ABS_Y);

        /* 顺序 A: 键盘, 鼠标, 触摸 */
        scores[0] = score_of(&kb);
        scores[1] = score_of(&mouse);
        scores[2] = score_of(&touch);
        int pick = input_pick_best(scores, 3);
        printf("      顺序[键盘,鼠标,触摸] -> 选中下标 %d (%s)\n", pick,
               pick >= 0 ? "触摸" : "无");
        CHECK(pick == 2, "应当挑中触摸屏(下标 2)");

        /* 顺序 B: 触摸, 键盘, 鼠标 —— 结果必须一致 */
        scores[0] = score_of(&touch);
        scores[1] = score_of(&kb);
        scores[2] = score_of(&mouse);
        pick = input_pick_best(scores, 3);
        printf("      顺序[触摸,键盘,鼠标] -> 选中下标 %d (%s)\n", pick,
               pick >= 0 ? "触摸" : "无");
        CHECK(pick == 0, "顺序换了也要挑中触摸屏(下标 0)");

        /* 顺序 C: 只有鼠标和键盘 -> 挑不出来 */
        scores[0] = score_of(&mouse);
        scores[1] = score_of(&kb);
        pick = input_pick_best(scores, 2);
        printf("      顺序[鼠标,键盘] -> 选中下标 %d\n", pick);
        CHECK(pick == -1, "没有触摸屏时应当明确失败(而不是随便挑一个)");

        /* 单点 vs 多点同时存在 -> 选多点 */
        scores[0] = 90;    /* 单点 */
        scores[1] = 120;   /* 多点 */
        CHECK(input_pick_best(scores, 2) == 1, "单点和多点都有时选多点");
    }

    /* ---- 6. 位操作辅助函数本身 ---- */
    printf("\n位掩码辅助函数:\n");
    {
        unsigned long bits[N_ABS];
        input_bits_zero(bits, N_ABS);
        CHECK(!input_bits_test(bits, ABS_X), "清零后应当读不到");
        input_bits_set(bits, ABS_MT_POSITION_X);
        CHECK(input_bits_test(bits, ABS_MT_POSITION_X), "置位后应当读得到");
        CHECK(!input_bits_test(bits, ABS_MT_POSITION_Y), "没置的位不能被误读");
        CHECK(N_ABS >= 2, "位掩码数组长度 %d 够装下 ABS 位", N_ABS);
    }

    printf("\n=== 结果: %s (%d 项失败) ===\n", g_fail ? "失败" : "全部通过", g_fail);
    return g_fail ? 1 : 0;
}

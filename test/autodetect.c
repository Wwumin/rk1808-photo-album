/*
 * test/autodetect.c  --  decide which /dev/input/eventN is the touchscreen
 *
 * Why this is split out:
 *     A board usually has several event devices (keyboard, mouse, touchscreen,
 *     sometimes two interfaces of the same panel). Hard-coding /dev/input/event2
 *     means it may well be a mouse on someone else's board and nothing responds.
 *
 *     The "is this a touchscreen" decision only depends on capability bitmasks,
 *     not on a real device, so it lives here as a pure function and can be fully
 *     exercised on the PC with synthetic masks. The actual open()/ioctl()/read()
 *     part stays in album/touch.c.
 */

#include <string.h>

#include "autodetect.h"

/*
 * Scoring (higher = more likely a touchscreen):
 *
 *   +100  has ABS_MT_POSITION_X/Y   -> multitouch protocol (modern panels)
 *    +70  has ABS_X and ABS_Y       -> single-touch protocol (resistive panels)
 *    +20  has BTN_TOUCH             -> explicitly claims to be touch
 *   -100  has REL_X or REL_Y        -> relative axes: that is a mouse, reject
 *   -100  has no absolute axis      -> keyboard / power button, reject
 *
 * Why mice are rejected so hard:
 *     Mice also report BTN_TOUCH on some drivers, and many report ABS as well.
 *     Judging by BTN_TOUCH alone would pick a mouse, so REL_X/REL_Y is a veto.
 */
int input_device_is_touch(const unsigned long *abs_bits, const unsigned long *key_bits,
                          const unsigned long *rel_bits, int have_rel)
{
    int has_rel_xy = 0;
    int has_abs_x = 0, has_abs_y = 0;
    int has_mt_x = 0, has_mt_y = 0;
    int has_btn_touch = 0;

    if (have_rel && rel_bits) {
        has_rel_xy = TEST_BIT(rel_bits, REL_X) || TEST_BIT(rel_bits, REL_Y);
    }
    if (abs_bits) {
        has_abs_x = TEST_BIT(abs_bits, ABS_X);
        has_abs_y = TEST_BIT(abs_bits, ABS_Y);
        has_mt_x  = TEST_BIT(abs_bits, ABS_MT_POSITION_X);
        has_mt_y  = TEST_BIT(abs_bits, ABS_MT_POSITION_Y);
    }
    if (key_bits) {
        has_btn_touch = TEST_BIT(key_bits, BTN_TOUCH);
    }

    /* Mouse: it has relative axes -> reject outright */
    if (has_rel_xy) {
        return -1;
    }
    /* No absolute axis at all -> keyboard or similar */
    if (!has_abs_x && !has_abs_y && !has_mt_x && !has_mt_y) {
        return -1;
    }

    {
        int score = 0;
        if (has_mt_x && has_mt_y) {
            score += 100;
        }
        if (has_abs_x && has_abs_y) {
            score += 70;
        }
        if (has_btn_touch) {
            score += 20;
        }
        return score;
    }
}

int input_pick_best(const int *scores, int n)
{
    int best = -1;
    int best_score = -1;

    for (int i = 0; i < n; i++) {
        if (scores[i] > best_score) {
            best_score = scores[i];
            best = i;
        }
    }
    return best;
}

void input_bits_zero(unsigned long *bits, int n_longs)
{
    memset(bits, 0, (size_t)n_longs * sizeof(unsigned long));
}

void input_bits_set(unsigned long *bits, int bit)
{
    bits[bit / (8 * (int)sizeof(unsigned long))] |=
        (1UL << (bit % (8 * (int)sizeof(unsigned long))));
}

int input_bits_test(const unsigned long *bits, int bit)
{
    return (bits[bit / (8 * (int)sizeof(unsigned long))] &
            (1UL << (bit % (8 * (int)sizeof(unsigned long))))) ? 1 : 0;
}

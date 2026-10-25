/*
 * test/autodetect.h  --  input device classification (pure functions, unit-testable)
 *
 * NOTE: the absolute-axis, relative-axis and BTN_ constants come from
 *       linux/input.h on the board, and from test/stub/linux/input.h on the PC.
 *       The numeric values must match between the two.
 *
 * Careful when writing comments here: a sequence like "ABS_STAR/REL_STAR"
 * written with a literal asterisk closes the comment early and the next line
 * gets parsed as code. That mistake was made once in this very file.
 */

#ifndef TEST_AUTODETECT_H
#define TEST_AUTODETECT_H

#include <linux/input.h>

#define TEST_BIT(bits, bit) input_bits_test((bits), (bit))

/* Returns a "looks like a touchscreen" score; < 0 means rejected (not a touchscreen) */
int input_device_is_touch(const unsigned long *abs_bits, const unsigned long *key_bits,
                          const unsigned long *rel_bits, int have_rel);

/* Pick the highest score; returns index, or -1 if all are negative */
int input_pick_best(const int *scores, int n);

void input_bits_zero(unsigned long *bits, int n_longs);
void input_bits_set(unsigned long *bits, int bit);
int  input_bits_test(const unsigned long *bits, int bit);

/* How many unsigned long are needed to hold bits 0..max_bit */
#define BITS_LONGS(max_bit) (((max_bit) + 8 * (int)sizeof(unsigned long)) / \
                             (8 * (int)sizeof(unsigned long)))

#endif

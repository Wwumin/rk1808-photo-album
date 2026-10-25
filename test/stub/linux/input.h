/*
 * test/stub/linux/input.h  --  minimal linux/input.h for PC-side testing only
 *
 * Windows has no such header, but autodetect.c needs the absolute-axis,
 * relative-axis and BTN_ constants. Only the ones the tests actually use are
 * defined here, and the numeric values MUST match the Linux kernel
 * (see include/uapi/linux/input-event-codes.h), or board behaviour would differ.
 *
 * Real device access (open/ioctl/read) does not go through this header;
 * that lives in album/touch.c.
 */

#ifndef STUB_LINUX_INPUT_H
#define STUB_LINUX_INPUT_H

#include <stdint.h>
#include <sys/time.h>

/* ---- event types ---- */
#define EV_SYN          0x00
#define EV_KEY          0x01
#define EV_REL          0x02
#define EV_ABS          0x03

/* ---- synchronisation ---- */
#define SYN_REPORT      0

/* ---- relative axes (mouse) ---- */
#define REL_X           0x00
#define REL_Y           0x01

/* ---- absolute axes (touch) ---- */
#define ABS_X             0x00
#define ABS_Y             0x01
#define ABS_MT_SLOT       0x2f
#define ABS_MT_POSITION_X 0x35
#define ABS_MT_POSITION_Y 0x36
#define ABS_MT_TRACKING_ID 0x39
#define ABS_MAX           0x3f

/* ---- keys ---- */
#define BTN_TOUCH       0x14a
#define KEY_MAX         0x2ff

/* ---- device properties ---- */
#define INPUT_PROP_DIRECT 0x01
#define INPUT_PROP_MAX    0x1f

/* ---- ioctl numbers (unused in tests, placeholders only) ---- */
#define EVIOCGBIT(ev, len)  (0)
#define EVIOCGABS(abs)      (0)
#define EVIOCGPROP(len)     (0)
#define EVIOCGNAME(len)     (0)

struct input_absinfo {
    int32_t value;
    int32_t minimum;
    int32_t maximum;
    int32_t fuzz;
    int32_t flat;
    int32_t resolution;
};

struct input_event {
    struct timeval time;
    uint16_t type;
    uint16_t code;
    int32_t  value;
};

#endif /* STUB_LINUX_INPUT_H */

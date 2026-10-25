/*
 * rng.c — 小游戏共用的可复现随机源，见 rng.h
 *
 * 用标准 rand()/srand()：random()/srandom() 在 Windows(MinGW) 上没有，
 * 而 PC 端冒烟测试必须在 Windows 和 Linux 上都能编。这些游戏对随机数质量
 * 要求很低，rand() 足够。
 */
#include <stdlib.h>
#include <time.h>

#include "rng.h"

static int      s_seeded = 0;
static unsigned s_override = 0;

void album_rng_seed(unsigned seed)
{
    s_override = seed;
    s_seeded = 0;              /* 下次取数时用新种子重新播种 */
}

static void seed_once(void)
{
    unsigned seed;

    if (s_seeded) {
        return;
    }
    if (s_override) {
        seed = s_override;
    } else {
        const char *env = getenv("ALBUM_SEED");
        seed = (env && *env) ? (unsigned)strtoul(env, NULL, 10)
                             : (unsigned)time(NULL);
    }
    srand(seed);
    s_seeded = 1;
}

int album_rand_below(int n)
{
    if (n <= 1) {
        return 0;
    }
    seed_once();
    return (int)(rand() % n);
}

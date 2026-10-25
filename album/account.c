/*
 * account.c  —  密码锁实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "account.h"

#define CFG_DEFAULT     "account.cfg"
#define PIN_DEFAULT     "1234"
#define SALT            "dsh-album-2024"

static char g_hash[32];         /* 当前生效的哈希(32 位哈希的十六进制串) */
static char g_cfg_path[512];
static int  g_from_file;

/* ------------------------------------------------------------------ 哈希 */
/*
 * FNV-1a 32bit + 盐。
 * 说清楚它的强度: 32 位哈希、无迭代，用普通电脑几秒钟就能暴力跑完
 * 10^4 种 4 位数字密码。它挡的是"别人看到明文"和"随手乱试"，
 * 不是有备而来的攻击。要做实验够用；真上产品应该换 SHA-256 + 多次迭代，
 * 并且加失败次数锁定 —— 锁定逻辑见 main.c 的 login 界面(错误 3 次回主界面)。
 */
void account_hash_pin(const char *pin, char *out, int out_size)
{
    unsigned int h = 2166136261u;        /* FNV offset basis */
    const char *p;

    for (p = SALT; *p; p++) {
        h ^= (unsigned char)*p;
        h *= 16777619u;
    }
    for (p = pin; p && *p; p++) {
        h ^= (unsigned char)*p;
        h *= 16777619u;
    }
    /* 再混一轮，把长度也编进去，避免 "12" 和 "12\0\0" 撞车 */
    h ^= (unsigned int)strlen(pin ? pin : "");
    h *= 16777619u;

    snprintf(out, (size_t)out_size, "%08X", h);
}

/* ------------------------------------------------------------------ 文件 */
/*
 * 配置格式(一行一个 key=value，故意做得简单，方便手改):
 *     version=1
 *     pin_hash=1A2B3C4D
 */
static int load_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    char line[256];
    int found = 0;

    if (!fp) {
        return 0;
    }
    while (fgets(line, sizeof(line), fp)) {
        char *eq = strchr(line, '=');
        char *v;
        if (!eq) {
            continue;
        }
        *eq = '\0';
        v = eq + 1;
        while (*v == ' ' || *v == '\t') {
            v++;
        }
        /* 去掉行尾空白 */
        {
            size_t n = strlen(v);
            while (n > 0 && (v[n - 1] == '\n' || v[n - 1] == '\r' || v[n - 1] == ' ')) {
                v[--n] = '\0';
            }
        }
        if (strcmp(line, "pin_hash") == 0 && *v) {
            snprintf(g_hash, sizeof(g_hash), "%s", v);
            found = 1;
        }
    }
    fclose(fp);
    return found;
}

static int save_file(const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        perror(path);
        return -1;
    }
    fprintf(fp, "# 电子相册账户配置 (由程序生成)\n");
    fprintf(fp, "version=1\n");
    fprintf(fp, "pin_hash=%s\n", g_hash);
    fclose(fp);
    printf("[account] 已写入 %s\n", path);
    return 0;
}

/* ------------------------------------------------------------------ 接口 */
int account_init(const char *cfg_path)
{
    if (cfg_path && *cfg_path) {
        snprintf(g_cfg_path, sizeof(g_cfg_path), "%s", cfg_path);
    } else {
        snprintf(g_cfg_path, sizeof(g_cfg_path), "%s", CFG_DEFAULT);
    }

    if (load_file(g_cfg_path)) {
        g_from_file = 1;
        printf("[account] 从 %s 读取密码\n", g_cfg_path);
    } else {
        account_hash_pin(PIN_DEFAULT, g_hash, sizeof(g_hash));
        g_from_file = 0;
        printf("[account] 未找到 %s，使用默认密码 %s\n", g_cfg_path, PIN_DEFAULT);
    }
    return 0;
}

int account_verify(const char *pin)
{
    char h[32];

    if (!pin || (int)strlen(pin) != ACCOUNT_PIN_LEN) {
        return 0;
    }
    account_hash_pin(pin, h, sizeof(h));
    return strcmp(h, g_hash) == 0 ? 1 : 0;
}

int account_change_pin(const char *new_pin)
{
    if (!new_pin || (int)strlen(new_pin) != ACCOUNT_PIN_LEN) {
        return -1;
    }
    account_hash_pin(new_pin, g_hash, sizeof(g_hash));
    g_from_file = 1;
    return save_file(g_cfg_path);
}

int account_from_file(void)
{
    return g_from_file;
}

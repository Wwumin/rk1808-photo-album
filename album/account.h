/*
 * account.h  —  账户登录 / 密码锁 (附加题)
 *
 * 设计要点:
 *   1. 不存明文密码。存的是一串带盐的哈希(见 account.c 的 hash_pin)。
 *      板上没有 openssl，所以用一个简单但足够的 FNV-1a 变体 —— 目的是
 *      "别人直接打开配置文件看不到明文"，不是抗暴力破解，这一点在 README 里说明。
 *   2. 密码文件路径可配，默认和图片同一个目录下的 account.cfg。
 *      文件不存在时用默认密码 1234，并在首次修改密码时写盘。
 */

#ifndef ALBUM_ACCOUNT_H
#define ALBUM_ACCOUNT_H

#define ACCOUNT_PIN_LEN 4

/* 加载密码配置。cfg_path 为 NULL 时用默认路径。返回 0 成功 */
int  account_init(const char *cfg_path);

/* 校验。ok 返回 1，错返回 0 */
int  account_verify(const char *pin);

/* 改密码(要求新密码长度为 ACCOUNT_PIN_LEN)，成功返回 0 */
int  account_change_pin(const char *new_pin);

/* 当前配置是否来自文件(1) 还是默认值(0)，用来在界面上给个提示 */
int  account_from_file(void);

/* 暴露哈希函数，测试里要和预期的十六进制串比对 */
void account_hash_pin(const char *pin, char *out, int out_size);

#endif /* ALBUM_ACCOUNT_H */

/*
 * font8x16.h  —  8x16 ASCII 点阵字库声明
 */

#ifndef ALBUM_FONT8X16_H
#define ALBUM_FONT8X16_H

/* 95 个字符: ASCII 32(' ') .. 126('~')，每字符 16 字节 */
extern const unsigned char font8x16[95][16];

#define FONT8X16_FIRST 32
#define FONT8X16_LAST  126
#define FONT8X16_W     8
#define FONT8X16_H     16

#endif /* ALBUM_FONT8X16_H */

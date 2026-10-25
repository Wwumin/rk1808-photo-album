# 电子相册 —— 交叉编译 Makefile
#
# 目标平台: RK1808 (aarch64) + Buildroot SDK + DRM 直显 1024x600 + 触摸
#
# 与 share/DRM库2/DRM库/Makefile 的差异:
#   1. 把 ts.c 换成了 album/touch.c —— poll 带超时 + 按下/抬起分离，
#      自动播放才不会卡死，按钮才能有按下高亮
#   2. 多了 ui.c(界面骨架) / game2048.c(小游戏) / account.c(密码锁)
#   3. inc/ 与 lib/ 仍按原样直接复用
#
# 用法(在板子的交叉编译环境里，本目录下执行):
#   make            # 生成 main
#   make clean
#
# 如果 SDK 路径和你机器上不一样，改下面这行。

CC       = aarch64-linux-gnu-gcc

SYSROOT  = /opt/rk1808-sdk/buildroot/output/rockchip_rk1808/host/aarch64-buildroot-linux-gnu/sysroot

CPPFLAGS += --sysroot=$(SYSROOT)
CPPFLAGS += -I $(SYSROOT)
CPPFLAGS += -I .
# 为什么要有 -I . :
#   album/display.c 里写的是 #include "inc/DRMwrap.h"，这个路径是从
#   **工程根目录**算起的。编译器只会自动搜索"包含它的那个文件"所在目录
#   (也就是 ./album)，再搜 -I 列表；只给 -I ./inc 的话会去找
#   ./inc/inc/DRMwrap.h，必然找不到。加上 -I . 就能命中 ./inc/DRMwrap.h。
CPPFLAGS += -I ./album
CPPFLAGS += -I ./inc
CPPFLAGS += -L ./lib

CFLAGS   += -std=c11 -Wall -Wextra -O2 -g
# 用 -std=c11 时 glibc 处于 __STRICT_ANSI__, clock_gettime / CLOCK_MONOTONIC /
# strcasecmp 这些 POSIX 接口不会自动声明, 必须显式打开 POSIX 2008。
# (album/main.c 顶部也有一份同样的 #define, 这里再加一道保险)
CFLAGS   += -D_POSIX_C_SOURCE=200809L

LDFLAGS  += -ldrm
LDFLAGS  += -lDRMwrap
LDFLAGS  += -Wl,-rpath,./lib

SRCS     = album/main.c \
           album/display.c \
           album/image.c \
           album/draw.c \
           album/font8x16.c \
           album/ui.c \
           album/touch.c \
           album/game2048.c \
           album/account.c \
           album/games.c \
           album/ttt.c \
           album/ms.c \
           album/xq.c \
           album/tet.c \
           album/snk.c

OBJS     = $(SRCS:.c=.o)

TARGET   = main

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(CPPFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# 只做语法检查，不链接(PC 上也能跑，用来快速发现笔误)
check-syntax:
	$(CC) -fsyntax-only $(CFLAGS) $(CPPFLAGS) $(SRCS)

clean:
	$(RM) $(OBJS) $(TARGET)

# 生成全部素材（需要 python3 + Pillow），产物落在 ./bmp/ —— 仓库里不含 BMP
# PYTHON 可覆盖：Windows 上通常是 "python"，Debian/Ubuntu 上通常只有 "python3"
PYTHON ?= python
assets:
	$(PYTHON) tools/gen_assets.py
	$(PYTHON) tools/gen_buttons.py
	$(PYTHON) tools/gen_extra.py
	$(PYTHON) tools/gen_games.py
	$(PYTHON) tools/make_landscapes.py
	$(PYTHON) tools/gen_font.py

# PC 端测试：不需要板子、不需要交叉工具链，只要本机 gcc
test:
	$(MAKE) -C test check

.PHONY: all clean check-syntax assets test

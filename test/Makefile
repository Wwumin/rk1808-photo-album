# PC 端测试与冒烟验证
#
#   mingw32-make -f Makefile unit    # 逻辑单测(秒级): 2048 规则 + 触摸设备识别
#   mingw32-make -f Makefile run     # 跑整个相册的状态机(会导出每一帧)
#   mingw32-make -f Makefile check   # 先单测再整机，一条命令全绿
#   mingw32-make -f Makefile clean
#
# Windows 用 mingw32-make，Linux 用 make，参数一样。
# 关键点: 把真实 touch.c 换成不依赖 linux/input.h 的桩，
#         把 DRM 换成一块普通内存(drm_stub.c)。

ALBUM_SRCS = ../album/display.c ../album/image.c ../album/draw.c ../album/font8x16.c \
             ../album/ui.c ../album/game2048.c ../album/account.c \
             ../album/games.c ../album/ttt.c ../album/ms.c ../album/xq.c \
             ../album/tet.c ../album/snk.c \
             stub/host/touch_stub.c stub/host/drm_stub.c \
             ../album/main.c

UNIT_2048     = ../album/game2048.c test2048.c
UNIT_DETECT   = autodetect.c test_autodetect.c

CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wno-unused-parameter -O1 -g \
          -DO_CLOEXEC=0 \
          -I../album -Istub/host -Istub -I.

all: album_test unit_2048 unit_detect

album_test: $(ALBUM_SRCS)
	$(CC) $(CFLAGS) $^ -o $@

unit_2048: $(UNIT_2048)
	$(CC) $(CFLAGS) $^ -o $@

unit_detect: $(UNIT_DETECT)
	$(CC) $(CFLAGS) $^ -o $@

# 先跑两个单测，再跑整机冒烟
#
# 三件事都不用再传环境变量了:
#   1) 桩环境由 display.c 里的 #if DRM_STUB_BUILD 自己判断
#   2) 脚本跑完自动收尾，由 touch_stub.c 默认打开
#   3) Windows 的 cmd.exe 不认 "./名字"，所以可执行名分平台写
ifeq ($(OS),Windows_NT)
EXE := .exe
RUN :=
else
EXE :=
RUN := ./
endif

check: unit_2048 unit_detect album_test
	$(RUN)unit_2048$(EXE)
	$(RUN)unit_detect$(EXE)
	$(RUN)album_test$(EXE) ../bmp /dev/null

run: album_test
	$(RUN)album_test$(EXE) ../bmp /dev/null

unit: unit_2048 unit_detect
	$(RUN)unit_2048$(EXE)
	$(RUN)unit_detect$(EXE)

# Windows 的 cmd.exe 没有 rm/rmdir -rf。注意不能用 $(RM):
# mingw32-make 里它的默认值就是 "rm -f"，在 cmd 下根本不存在这个命令。
ifeq ($(OS),Windows_NT)
DEL = del /q
CLEAN_OUT = if exist out rmdir /s /q out
else
DEL = rm -f
CLEAN_OUT = rm -rf out
endif

clean:
	-$(DEL) album_test$(EXE) unit_2048$(EXE) unit_detect$(EXE)
	-$(DEL) *.o
	-$(CLEAN_OUT)

.PHONY: all check run unit clean

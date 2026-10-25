#!/bin/sh
# list_input_devices.sh  --  在板子上跑一遍，看清楚有哪些输入设备、哪个是触摸屏
#
# 用法(板子上):
#     sh list_input_devices.sh
#
# 这个脚本只读信息，不改任何东西。程序本身也会自动识别触摸屏，
# 跑这个脚本只是为了"确认一下"和排查异常情况(比如自动识别没选对)。

echo "=============================================="
echo " 1. 内核登记的输入设备"
echo "=============================================="
if [ -r /proc/bus/input/devices ]; then
    cat /proc/bus/input/devices
else
    echo "读不到 /proc/bus/input/devices"
fi

echo
echo "=============================================="
echo " 2. /dev/input/ 下的设备节点"
echo "=============================================="
ls -l /dev/input/ 2>/dev/null || echo "没有 /dev/input/ 目录"

echo
echo "=============================================="
echo " 3. 逐个 event 设备的能力位 (需要 evtest 或 hexdump)"
echo "=============================================="
if command -v evtest >/dev/null 2>&1; then
    echo "用 evtest --query 逐个探测:"
    for dev in /dev/input/event*; do
        [ -e "$dev" ] || continue
        printf '  %-24s ' "$dev"
        if evtest --query "$dev" EV_ABS ABS_MT_POSITION_X >/dev/null 2>&1; then
            echo "多点触摸屏 (有 ABS_MT_POSITION_X)"
        elif evtest --query "$dev" EV_ABS ABS_X >/dev/null 2>&1; then
            echo "单点触摸屏 (有 ABS_X)"
        elif evtest --query "$dev" EV_REL REL_X >/dev/null 2>&1; then
            echo "鼠标 (有 REL_X)"
        else
            echo "其它(键盘/按键)"
        fi
    done
else
    echo "没有 evtest，跳过。语法: evtest /dev/input/event2  (Ctrl+C 退出)"
fi

echo
echo "=============================================="
echo " 4. 结论 / 怎么用"
echo "=============================================="
echo "  main 默认会自动扫描 /dev/input/event*，按"
echo "  \"有没有绝对坐标轴\" 挑出触摸屏，一般不用手动指定。"
echo
echo "  如果自动识别选错了，或者想强制指定:"
echo "    ./main bmp /dev/input/event1        # 第二个参数"
echo "    TOUCH_DEV=/dev/input/event1 ./main  # 环境变量"
echo
echo "  启动日志里会打印实际选中的设备:"
echo "    [touch] 自动选中 /dev/input/event2"
echo "    [main] 触摸设备: /dev/input/event2"

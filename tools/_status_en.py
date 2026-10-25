"""
把游戏状态栏里的中文全部换成英文。

原因: 状态栏是 draw_text() 画的，而 font8x16.c 里只有 ASCII 32..126，
中文直接变成 '?'（实测扫雷底栏显示成 "?????? ?? ????"）。
中文标题和按钮文字都烘在 PNG 背景图里，不受影响，所以只需要改运行时字符串。
"""
import io
import os
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "album")

REPL = {
    "ms.c": [
        ('"翻开模式  ·  剩余雷数 %d"', '"OPEN MODE  ·  MINES %d"'),
        ('"踩到雷了  ·  点重来再来一局"', '"BOOM!  TAP RESTART"'),
        ('"全部排完  ·  点重来再来一局"', '"CLEARED!  TAP RESTART"'),
        ('"插旗模式  ·  剩余雷数 %d"', '"FLAG MODE  ·  MINES %d"'),
        ('"翻开模式  ·  剩余雷数 %d  ·  已开 %d"', '"OPEN MODE  ·  MINES %d  ·  OPENED %d"'),
        ('"%s模式  ·  剩余雷数 %d"', '"%s MODE  ·  MINES %d"'),
        ('s_flag_mode ? "插旗" : "翻开"', 's_flag_mode ? "FLAG" : "OPEN"'),
    ],
    "snk.c": [
        ('"长度 %d  ·  点哪边往哪边拐"', '"LEN %d  ·  TAP A SIDE TO TURN"'),
        ('"撞墙了  ·  长度 %d  ·  得分 %d  ·  点重来"', '"HIT WALL  ·  LEN %d  ·  SCORE %d"'),
        ('"咬到自己了  ·  长度 %d  ·  得分 %d  ·  点重来"', '"ATE ITSELF  ·  LEN %d  ·  SCORE %d"'),
        ('"长度 %d  ·  得分 %d  ·  点重来"', '"LEN %d  ·  SCORE %d  ·  RESTART"'),
    ],
    "tet.c": [
        ('"游戏结束  ·  得分 %d  ·  点重来再来一局"', '"GAME OVER  ·  SCORE %d  ·  TAP RESTART"'),
        ('"已消 %d 行  ·  左/转/右/落到底"', '"LINES %d  ·  LEFT / ROT / RIGHT / DROP"'),
        ('"得分 %d  ·  第 %d 级  ·  已消 %d 行"', '"SCORE %d  ·  LEVEL %d  ·  LINES %d"'),
    ],
    "ttt.c": [
        ('"你执 X, 点空格落子"', '"YOU ARE X  ·  TAP AN EMPTY CELL"'),
        ('"你赢了!  点重来再来一局"', '"YOU WIN!  TAP RESTART"'),
        ('"AI 赢了  点重来再来一局"', '"AI WINS  ·  TAP RESTART"'),
        ('"平局  点重来再来一局"', '"DRAW  ·  TAP RESTART"'),
        ('"AI 已落子, 轮到你了"', '"AI MOVED  ·  YOUR TURN"'),
    ],
    "xq.c": [
        ('"红方先行   ·   点自己的棋子选中"', '"RED FIRST  ·  TAP YOUR PIECE"'),
        ('"黑方吃将   ·   点重来再来一局"', '"BLACK WINS  ·  TAP RESTART"'),
        ('"将军!   ·   黑方被将"', '"CHECK!  BLACK IN CHECK"'),
        ('"你的帅被将军了   ·   必须先解将"', '"YOUR KING IS IN CHECK"'),
        ('"红方走子   ·   点自己的棋子选中"', '"RED TO MOVE  ·  TAP YOUR PIECE"'),
        ('"已取消选择"', '"SELECTION CLEARED"'),
        ('"红方吃将, 你赢了!   ·   点重来再来一局"', '"RED WINS!  TAP RESTART"'),
        ('"AI 思考中..."', '"AI THINKING..."'),
        ('"已选中 (%d,%d)   ·   点绿点落子"', '"PICKED (%d,%d)  ·  TAP A GREEN DOT"'),
    ],
}

miss = []
for fn, pairs in REPL.items():
    p = os.path.join(ROOT, fn)
    s = io.open(p, encoding="utf-8").read()
    n = 0
    for old, new in pairs:
        if old in s:
            s = s.replace(old, new)
            n += 1
        else:
            miss.append((fn, old))
    io.open(p, "w", encoding="utf-8", newline="\n").write(s)
    print("%-8s 替换 %d/%d 处" % (fn, n, len(pairs)))

if miss:
    print("\n未命中的锚点(需人工看一下):")
    for fn, old in miss:
        print("  %s: %s" % (fn, old))
    sys.exit(1)
print("\n全部替换完成")

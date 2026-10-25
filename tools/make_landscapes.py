# -*- coding: utf-8 -*-
"""
生成 3 张原创风景图 (1024x600)，直接输出到 bmp/ 作为相册图片。

为什么用"画"的而不是从网上下载:
    下载的图有版权风险，而且尺寸/比例不可控。这里用程序合成，
    完全原创、尺寸精确 1024x600、可随时重跑调色。

三张图刻意选了三种完全不同的色调和构图，
这样"手动翻页/自动播放"时一眼就能看出图确实换了，不会看串:
    photo_11 日落雪山湖   —— 暖色(橙红紫)
    photo_12 极光雪山湖   —— 冷色(青蓝紫)，夜景
    photo_13 海岛沙滩     —— 亮色(青绿黄)，日景

注意: 底部约 60px 保持深色/简洁，程序会在那里画文件名 HUD 信息栏。

运行:  python make_landscapes.py
"""

import os
import math
import random

from PIL import Image, ImageDraw, ImageFilter, ImageChops

W, H = 1024, 600
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "bmp")

assert (W, H) == (1024, 600)


# ------------------------------------------------------------------ 通用工具
def lerp(a, b, t):
    return a + (b - a) * t


def lerp3(c1, c2, t):
    return tuple(int(round(lerp(c1[i], c2[i], t))) for i in range(3))


def vgrad(size, stops):
    """竖向多段渐变。stops = [(位置0..1, (r,g,b)), ...] 必须按位置升序。"""
    w, h = size
    img = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        for i in range(len(stops) - 1):
            p0, c0 = stops[i]
            p1, c1 = stops[i + 1]
            if p0 <= t <= p1:
                k = 0.0 if p1 == p0 else (t - p0) / (p1 - p0)
                d.line([(0, y), (w, y)], fill=lerp3(c0, c1, k))
                break
        else:
            d.line([(0, y), (w, y)], fill=stops[-1][1])
    return img


def _value_noise_1d(x, seed):
    """一维值噪声：整数格点取随机值，段内用 smoothstep 平滑插值。"""
    i = int(math.floor(x))
    f = x - i
    k = f * f * (3 - 2 * f)
    a = random.Random(seed * 7919 + i * 104729).random()
    b = random.Random(seed * 7919 + (i + 1) * 104729).random()
    return a + (b - a) * k


def _fbm(x, seed, octaves=6, roughness=0.52):
    total, norm, amp, freq = 0.0, 0.0, 1.0, 1.0
    for o in range(octaves):
        total += _value_noise_1d(x * freq, seed + o * 131) * amp
        norm += amp
        amp *= roughness
        freq *= 2.07
    return total / norm if norm else 0.0


def ridge(width, base_y, amp, seed, octaves=6, roughness=0.52):
    """
    生成一条山脊线 y(x)。

    用 fbm 值噪声而不是简单正弦叠加: 正弦叠出来是圆滚滚的波浪，
    而 1/f 噪声才有真实的尖峰和沟谷。再用 (1-n)^2 做 ridged 变换，
    峰顶更锐利、谷底更平缓。
    """
    vals = []
    for x in range(width):
        n = _fbm(x / width * 5.3, seed, octaves, roughness)
        n = 1.0 - n
        vals.append(base_y - (n * n) * amp)
    return vals


def apply_vertical_fade(img, top_alpha, bottom_alpha):
    """给整张图叠加一个"上淡下浓"的黑色蒙版，用来做大气透视。"""
    w, h = img.size
    mask = Image.new("L", (w, h))
    md = ImageDraw.Draw(mask)
    for y in range(h):
        t = y / max(1, h - 1)
        md.line([(0, y), (w, y)], fill=int(lerp(top_alpha, bottom_alpha, t)))
    black = Image.new("RGB", (w, h), (0, 0, 0))
    return Image.composite(black, img, mask)


def add_grain(img, amount=4, seed=7):
    """轻微噪点，避免大面积渐变出现色带。"""
    rnd = random.Random(seed)
    w, h = img.size
    noise = Image.new("L", (w, h))
    noise.putdata([128 + rnd.randint(-amount, amount) for _ in range(w * h)])
    noise = noise.convert("RGB")
    return ImageChops.overlay(img, noise)


def save(img, name):
    os.makedirs(OUT_DIR, exist_ok=True)
    if img.size != (W, H):
        raise AssertionError("%s 尺寸不对: %s，应为 %s" % (name, img.size, (W, H)))
    path = os.path.join(OUT_DIR, name)
    img.convert("RGB").save(path, "BMP")
    print("  [OK] %-22s %6.2f MB  %dx%d" %
          (name, os.path.getsize(path) / 1048576.0, img.width, img.height))
    return path


# ------------------------------------------------------------------ 图 1: 日落雪山湖
def landscape_sunset():
    w, h = W, H
    HORIZON = 400
    SUN = (505, 322, 40)

    img = vgrad((w, HORIZON), [
        (0.00, (36, 30, 82)),
        (0.22, (108, 58, 126)),
        (0.45, (206, 92, 108)),
        (0.66, (243, 146, 92)),
        (0.84, (252, 196, 116)),
        (1.00, (255, 228, 164)),
    ])
    d = ImageDraw.Draw(img, "RGBA")

    # 碎云：横向拉长的椭圆
    rnd = random.Random(11)
    for i in range(26):
        cy = rnd.uniform(60, 300)
        cx = rnd.uniform(-80, w + 80)
        cw = rnd.uniform(90, 320)
        ch = rnd.uniform(6, 20)
        t = cy / 300.0
        col = lerp3((120, 70, 120), (255, 214, 160), t)
        alpha = int(rnd.uniform(40, 105))
        d.ellipse([cx, cy, cx + cw, cy + ch], fill=col + (alpha,))
    img = img.filter(ImageFilter.GaussianBlur(3))

    # 三层山脉，越远越亮(大气透视)
    # base_y 必须落在 HORIZON 以上，否则山的本体跑到水里，倒影会穿帮
    layers = [
        (HORIZON - 84, 52, 101, (158, 136, 176), 0.58, False),
        (HORIZON - 42, 42, 202, (112,  90, 132), 0.38, False),
        (HORIZON - 12, 26, 303, ( 52,  40,  70), 0.16, True),
    ]
    for base_y, amp, seed, col, keep, snowy in layers:
        top = ridge(w, base_y, amp, seed, octaves=6, roughness=0.5)
        poly = [(x, top[x]) for x in range(w)] + [(w, HORIZON + 10), (0, HORIZON + 10)]
        layer = Image.new("RGBA", (w, HORIZON + 10), col + (255,))
        mask = Image.new("L", (w, HORIZON + 10), 0)
        ImageDraw.Draw(mask).polygon(poly, fill=int(255 * keep))
        img.paste(layer, (0, 0), mask)
        if snowy:
            sd = ImageDraw.Draw(img, "RGBA")
            for x in range(0, w, 2):
                if top[x] < base_y - amp * 0.22:
                    sd.point((x, top[x]), fill=(255, 240, 225, 150))
                    sd.point((x, top[x] + 1), fill=(240, 214, 210, 90))

    # 太阳改用"叠加"的方式，避免出现一大团渐变的死白
    sun = Image.new("RGB", (w, HORIZON), (0, 0, 0))
    sd = ImageDraw.Draw(sun, "RGBA")
    for r, a in ((320, 10), (230, 16), (150, 28), (92, 46), (54, 255)):
        col = (255, 236, 178) if a == 255 else (255, 168, 96)
        sd.ellipse([SUN[0] - r, SUN[1] - r, SUN[0] + r, SUN[1] + r],
                   fill=col + (255,) if a == 255 else col + (a,))
    sun = sun.filter(ImageFilter.GaussianBlur(13))
    sd = ImageDraw.Draw(sun, "RGBA")
    sd.ellipse([SUN[0] - 48, SUN[1] - 48, SUN[0] + 48, SUN[1] + 48], fill=(255, 250, 226, 255))
    img = ImageChops.add(img, sun)

    # 大气雾霭: 把整幅天空往暖橙方向提一点，山才不像贴上去的
    haze = vgrad((w, HORIZON), [(0.0, (26, 14, 40)), (0.62, (74, 34, 40)),
                                (0.86, (120, 62, 40)), (1.0, (170, 104, 62))])
    img = Image.blend(img, ImageChops.add(img, haze), 0.42)

    # 倒影必须在"山已经画完"之后做，否则镜像里只有天空没有山
    # 注意: PIL 的 paste() 不会自动扩大画布，越界部分直接丢弃，
    #       所以要先建好 1024x600 的画布再拼，不能往天空画布上贴。
    sky = img.crop((0, 0, w, HORIZON))
    refl = sky.transpose(Image.FLIP_TOP_BOTTOM)
    # 水面压暗 + 偏冷: 只压暗不偏色的话，倒影会跟天空一样亮，
    # 看上去"不像水"；水里要掺一点深蓝，越远越深。
    water_base = Image.new("RGB", (w, h - HORIZON))
    water_base.paste(refl, (0, 0))
    water = Image.blend(water_base, Image.new("RGB", water_base.size, (22, 32, 62)), 0.34)
    tint = Image.new("RGBA", (w, h - HORIZON))
    td = ImageDraw.Draw(tint)
    for y in range(h - HORIZON):
        t = y / float(h - HORIZON - 1)
        td.line([(0, y), (w, y)], fill=(6, 12, 30, int(lerp(86, 196, t ** 0.85))))
    water = Image.alpha_composite(water.convert("RGBA"), tint).convert("RGB")
    water = water.filter(ImageFilter.GaussianBlur(2))

    canvas = Image.new("RGB", (w, h))
    canvas.paste(sky, (0, 0))
    canvas.paste(water, (0, HORIZON))
    img = canvas
    assert img.size == (w, h), img.size

    # 湖面横向波纹(细一些、淡一些，否则会糊成一片雾)
    rnd = random.Random(11)
    d = ImageDraw.Draw(img, "RGBA")
    for y in range(HORIZON, H):
        t = (y - HORIZON) / float(H - HORIZON)
        for _ in range(int(lerp(2, 7, t))):
            x0 = rnd.uniform(0, w)
            ln = rnd.uniform(30, 170) * lerp(0.5, 1.5, t)
            a = int(lerp(20, 6, t))
            d.line([(x0, y), (x0 + ln, y)], fill=(226, 236, 250, a), width=1)

    # 夕阳在水面的光路
    # 之前用一个半透明矩形做"光柱"，边缘很硬、看着像一块楔形贴片，
    # 而且会把被压暗的太阳镜像重新提亮成三角。
    # 改成"碎光": 沿水面随机画横向短亮线，宽度/亮度随距离衰减，
    # 这才像阳光在水面被打碎后反上来的样子。
    glare = Image.new("RGBA", (w, H - HORIZON), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glare)
    grnd = random.Random(29)
    for y in range(2, H - HORIZON):
        t = y / float(H - HORIZON)
        # 离太阳正下方越远越弱
        hw = lerp(30, 250, t ** 0.62)
        a_base = lerp(150, 8, t ** 0.5)
        for _ in range(int(lerp(3, 13, t))):
            off = grnd.uniform(-1, 1) * hw
            x = SUN[0] + off
            falloff = 1.0 - abs(off) / max(1.0, hw)
            ln = grnd.uniform(10, 54) * lerp(0.6, 1.35, t)
            a = int(a_base * (0.30 + 0.70 * falloff) * grnd.uniform(0.6, 1.0))
            if a <= 3:
                continue
            gd.line([(x, y), (x + ln, y)], fill=(255, 232, 184, a),
                    width=1 if t < 0.6 else 2)
    glare = glare.filter(ImageFilter.GaussianBlur(1.4))
    img.paste(Image.alpha_composite(img.crop((0, HORIZON, w, H)).convert("RGBA"), glare).convert("RGB"),
              (0, HORIZON))

    # 远处飞鸟
    d = ImageDraw.Draw(img, "RGBA")
    for (bx, by, s) in ((300, 150, 11), (345, 128, 8), (272, 178, 7), (372, 172, 6), (250, 118, 5)):
        for sgn in (-1, 1):
            d.line([(bx, by), (bx + sgn * s, by - s * 0.6)], fill=(58, 40, 62, 210), width=2)
            d.line([(bx + sgn * s, by - s * 0.6), (bx + sgn * s * 1.7, by - s * 0.15)],
                   fill=(58, 40, 62, 210), width=2)

    return save(add_grain(img, 4, 71), "photo_11.bmp")


# ------------------------------------------------------------------ 图 2: 极光雪山湖
def landscape_aurora():
    w, h = W, H
    HORIZON = 415

    img = vgrad((w, HORIZON), [
        (0.00, (5, 9, 28)),
        (0.35, (10, 24, 58)),
        (0.66, (16, 46, 82)),
        (0.86, (26, 78, 102)),
        (1.00, (44, 104, 116)),
    ])
    d = ImageDraw.Draw(img, "RGBA")

    # 星空：越靠上越密
    rnd = random.Random(23)
    for _ in range(520):
        x = rnd.uniform(0, w)
        y = rnd.uniform(0, HORIZON - 40) ** 1.0
        y = rnd.uniform(0, 1) ** 1.7 * (HORIZON - 60)
        r = rnd.choice((0.6, 0.8, 1.0, 1.3, 1.8))
        a = int(rnd.uniform(90, 235) * (1 - y / HORIZON) + 40)
        d.ellipse([x - r, y - r, x + r, y + r], fill=(215, 232, 255, min(255, a)))
    # 几颗亮星带十字光芒
    for (x, y) in ((150, 70), (612, 48), (860, 120), (400, 160), (960, 66)):
        d.ellipse([x - 2.4, y - 2.4, x + 2.4, y + 2.4], fill=(255, 255, 255, 255))
        d.line([(x - 9, y), (x + 9, y)], fill=(200, 225, 255, 130))
        d.line([(x, y - 9), (x, y + 9)], fill=(200, 225, 255, 130))

    # 极光：用"叠加"混合的柔和光幕
    aurora = Image.new("RGB", (w, HORIZON), (0, 0, 0))
    ad = ImageDraw.Draw(aurora, "RGBA")
    bands = [
        (0.30, (60, 255, 170), 150, 44),
        (0.44, (40, 210, 235), 120, 38),
        (0.56, (150, 120, 255), 96, 30),
    ]
    for pos, col, alpha, thick in bands:
        y0 = HORIZON * pos
        for i in range(24):
            t = i / 23.0
            yy = y0 + math.sin(t * math.tau * 1.6 + pos * 9) * 34 + (t - 0.5) * 70
            hh = thick * (0.4 + 0.9 * math.sin(t * math.pi))
            ad.ellipse([t * w - 90, yy - hh, t * w + 90, yy + hh],
                       fill=col + (int(alpha * 0.5),))
    aurora = aurora.filter(ImageFilter.GaussianBlur(34))
    # 竖直拉丝
    streaks = Image.new("RGB", (w, HORIZON), (0, 0, 0))
    sd = ImageDraw.Draw(streaks, "RGBA")
    for _ in range(190):
        x = rnd.uniform(0, w)
        y = rnd.uniform(30, HORIZON * 0.72)
        ln = rnd.uniform(30, 150)
        col = rnd.choice(((90, 255, 190), (70, 220, 240), (150, 140, 255)))
        sd.line([(x, y), (x + rnd.uniform(-8, 8), y + ln)], fill=col + (int(rnd.uniform(14, 46)),), width=2)
    streaks = streaks.filter(ImageFilter.GaussianBlur(11))
    aurora = ImageChops.add(aurora, streaks)
    img = ImageChops.add(img, aurora)

    # 两层雪山(base_y 都要在水面之上，山的本体不能伸进水里)
    d = ImageDraw.Draw(img, "RGBA")
    for base_y, amp, seed, col, keep in (
            (HORIZON - 58, 74, 507, (104, 136, 176), 0.74),
            (HORIZON - 16, 46, 601, (26, 34, 62), 0.95)):
        top = ridge(w, base_y, amp, seed, octaves=6, roughness=0.5)
        poly = [(x, top[x]) for x in range(w)] + [(w, HORIZON + 12), (0, HORIZON + 12)]
        layer = Image.new("RGBA", (w, HORIZON + 12), col + (255,))
        mask = Image.new("L", (w, HORIZON + 12), 0)
        ImageDraw.Draw(mask).polygon(poly, fill=int(255 * keep))
        img.paste(layer, (0, 0), mask)
        # 雪线：山顶一小段提亮
        sd2 = ImageDraw.Draw(img, "RGBA")
        for x in range(0, w, 2):
            if top[x] < base_y - amp * 0.25:
                sd2.point((x, top[x]), fill=(226, 238, 255, 190))
                sd2.point((x, top[x] + 1), fill=(200, 220, 250, 120))

    # 湖面镜像（同上：先裁天空，再在完整画布上拼装）
    sky = img.crop((0, 0, w, HORIZON))
    refl = sky.transpose(Image.FLIP_TOP_BOTTOM)
    water_base = Image.new("RGB", (w, h - HORIZON))
    water_base.paste(refl, (0, 0))
    tint = Image.new("RGBA", (w, h - HORIZON))
    td = ImageDraw.Draw(tint)
    for y in range(h - HORIZON):
        t = y / float(h - HORIZON - 1)
        td.line([(0, y), (w, y)], fill=(4, 10, 26, int(lerp(88, 186, t))))
    water = Image.alpha_composite(water_base.convert("RGBA"), tint).convert("RGB")
    water = water.filter(ImageFilter.GaussianBlur(2))

    canvas = Image.new("RGB", (w, h))
    canvas.paste(sky, (0, 0))
    canvas.paste(water, (0, HORIZON))
    img = canvas
    assert img.size == (w, h), img.size

    d = ImageDraw.Draw(img, "RGBA")
    for y in range(HORIZON, H):
        t = (y - HORIZON) / float(H - HORIZON)
        for _ in range(int(lerp(2, 8, t))):
            x0 = rnd.uniform(0, w)
            ln = rnd.uniform(50, 260)
            d.line([(x0, y), (x0 + ln, y)], fill=(180, 240, 255, int(lerp(22, 6, t))), width=1)

    # 前景两棵云杉剪影
    def spruce(cx, base, height, color):
        trunk_w = max(3, height // 26)
        d.rectangle([cx - trunk_w // 2, base - height * 0.28, cx + trunk_w // 2, base], fill=color)
        tiers = 9
        for i in range(tiers):
            t = i / float(tiers - 1)
            y = base - height * (0.20 + 0.80 * t)
            half = (height * 0.30) * (1.0 - t) + height * 0.045
            y2 = y - height * 0.16
            d.polygon([(cx - half, y), (cx + half, y), (cx, y2)], fill=color)

    spruce(112, H - 24, 250, (6, 12, 24))
    spruce(206, H - 12, 178, (8, 16, 30))
    spruce(916, H - 18, 214, (6, 12, 24))

    return save(add_grain(img, 3, 97), "photo_12.bmp")


# ------------------------------------------------------------------ 图 3: 海岛沙滩
def landscape_island():
    w, h = W, H
    HORIZON = 372

    img = vgrad((w, HORIZON), [
        (0.00, (28, 132, 208)),
        (0.40, (86, 178, 228)),
        (0.74, (158, 214, 240)),
        (1.00, (216, 238, 248)),
    ])
    d = ImageDraw.Draw(img, "RGBA")

    # 云朵：一簇圆叠出体积感
    def cloud(cx, cy, scale, alpha):
        puffs = [(-1.5, 0.15, 0.9), (-0.7, -0.35, 1.25), (0.15, -0.5, 1.45),
                 (1.0, -0.15, 1.1), (1.8, 0.2, 0.8), (0.5, 0.25, 1.2), (-0.2, 0.3, 1.05)]
        for (dx, dy, r) in puffs:
            rr = 34 * r * scale
            x = cx + dx * 42 * scale
            y = cy + dy * 34 * scale
            d.ellipse([x - rr, y - rr * 0.82, x + rr, y + rr * 0.82], fill=(255, 255, 255, alpha))
        d.rectangle([cx - 62 * scale, cy + 6 * scale, cx + 70 * scale, cy + 26 * scale],
                    fill=(255, 255, 255, alpha))

    cloud(210, 96, 1.35, 232)
    cloud(560, 62, 0.95, 205)
    cloud(830, 132, 1.15, 220)
    cloud(980, 70, 0.7, 185)
    cloud(415, 178, 0.6, 150)
    img = img.filter(ImageFilter.GaussianBlur(1))

    # 天空先落到完整画布上，后面海/沙滩直接贴在 1024x600 的画布上
    # （PIL 的 paste() 不会自动扩大画布，越界会静默丢弃，所以不能贴在小画布上）
    canvas = Image.new("RGB", (w, h))
    canvas.paste(img, (0, 0))
    img = canvas
    assert img.size == (w, h), img.size

    # 远岛
    d = ImageDraw.Draw(img, "RGBA")
    d.polygon([(60, HORIZON), (150, HORIZON - 46), (250, HORIZON - 12), (330, HORIZON)],
              fill=(126, 158, 176, 190))
    d.polygon([(700, HORIZON), (790, HORIZON - 26), (900, HORIZON)], fill=(138, 168, 184, 165))

    # 海：上深下浅 + 阳光带
    sea_h = HORIZON + 74
    sea = vgrad((w, sea_h - HORIZON), [
        (0.00, (34, 132, 168)),
        (0.35, (46, 168, 190)),
        (0.70, (86, 206, 202)),
        (1.00, (150, 232, 214)),
    ])
    img.paste(sea, (0, HORIZON))
    d = ImageDraw.Draw(img, "RGBA")
    for y in range(HORIZON, sea_h):
        t = (y - HORIZON) / float(sea_h - HORIZON)
        col = lerp3((255, 255, 255), (208, 250, 255), t)
        for _ in range(int(lerp(5, 16, t))):
            x0 = random.Random(y * 31 + _).uniform(0, w)
            ln = random.Random(y * 17 + _).uniform(24, 150) * lerp(0.6, 1.5, t)
            d.line([(x0, y), (x0 + ln, y)], fill=col + (int(lerp(46, 96, t)),), width=1)

    # 浪花线
    rnd = random.Random(5)
    for k, (yy, alpha, th) in enumerate(((sea_h - 22, 120, 7), (sea_h - 10, 175, 11), (sea_h - 1, 235, 15))):
        pts = [(x, yy + math.sin(x / 90.0 + k * 2) * 4 + rnd.uniform(-2, 2)) for x in range(0, w + 10, 10)]
        d.line(pts, fill=(255, 255, 255, alpha), width=th, joint="curve")

    # 沙滩
    beach = vgrad((w, H - sea_h), [
        (0.00, (247, 229, 188)),
        (0.30, (240, 214, 164)),
        (1.00, (206, 174, 126)),
    ])
    img.paste(beach, (0, sea_h))
    d = ImageDraw.Draw(img, "RGBA")
    d.polygon([(0, sea_h), (w, sea_h), (w, sea_h + 16), (0, sea_h + 26)], fill=(255, 250, 228, 190))
    # 湿沙反光 + 砂粒
    for _ in range(2600):
        x = rnd.uniform(0, w)
        y = rnd.uniform(sea_h + 12, H)
        s = rnd.choice((0.7, 1.0, 1.4))
        col = (255, 246, 214) if rnd.random() < 0.5 else (176, 146, 104)
        d.ellipse([x - s, y - s, x + s, y + s], fill=col + (int(rnd.uniform(40, 120)),))

    # 小帆船
    bx, by = 742, HORIZON + 26
    d.polygon([(bx - 30, by), (bx + 30, by), (bx + 20, by + 13), (bx - 20, by + 13)], fill=(238, 238, 242, 255))
    d.line([(bx, by), (bx, by - 62)], fill=(90, 78, 68, 255), width=3)
    d.polygon([(bx + 3, by - 60), (bx + 3, by - 6), (bx + 42, by - 14)], fill=(252, 250, 244, 255))
    d.polygon([(bx - 3, by - 54), (bx - 3, by - 6), (bx - 34, by - 12)], fill=(240, 152, 92, 255))
    d.ellipse([bx - 34, by + 8, bx + 34, by + 20], fill=(255, 255, 255, 90))

    # 前景椰子树(两棵，一大一小)
    def palm(px, base_y, height, lean, scale, color):
        seg = 22
        pts = []
        for i in range(seg + 1):
            t = i / float(seg)
            x = px + lean * height * (t ** 1.7)
            y = base_y - height * t
            pts.append((x, y))
        for i in range(seg):
            wdt = int(scale * lerp(15, 6, i / float(seg)))
            d.line([pts[i], pts[i + 1]], fill=color, width=wdt)
        tx, ty = pts[-1]
        # 椰子
        for (ox, oy) in ((-11, 6), (0, 11), (12, 5)):
            d.ellipse([tx + ox - 6, ty + oy - 6, tx + ox + 6, ty + oy + 6], fill=(72, 56, 40, 255))

        def frond(angle, L):
            """
            单片叶子: 沿一条向下弯曲的中轴，画一个宽度递增再收尖的多边形。
            之前用"线 + 锯齿"画，远看像刺不像叶子；填充面片才有叶子的体积感。
            """
            ex = tx + math.cos(angle) * L
            ey = ty + math.sin(angle) * L * 0.55 + L * 0.30          # 叶尖下垂
            cx = tx + math.cos(angle) * L * 0.55
            cy = ty + math.sin(angle) * L * 0.30 - L * 0.16          # 叶片先扬后垂

            n = 16
            spine = []
            for i in range(n + 1):
                u = i / float(n)
                # 二次贝塞尔: 起点(tx,ty) -> 控制点(cx,cy) -> 叶尖(ex,ey)
                bx = (1 - u) ** 2 * tx + 2 * (1 - u) * u * cx + u ** 2 * ex
                by = (1 - u) ** 2 * ty + 2 * (1 - u) * u * cy + u ** 2 * ey
                spine.append((bx, by))

            upper, lower = [], []
            for i, (sx, sy) in enumerate(spine):
                u = i / float(n)
                # 宽度: 根部窄、中段最宽、叶尖收成尖
                hw = L * 0.085 * math.sin(math.pi * (0.18 + 0.82 * u)) * (1 - 0.72 * u) + scale * 1.6
                if i < n:
                    dx = spine[i + 1][0] - sx
                    dy = spine[i + 1][1] - sy
                else:
                    dx = sx - spine[i - 1][0]
                    dy = sy - spine[i - 1][1]
                ln = math.hypot(dx, dy) or 1
                nx, ny = -dy / ln, dx / ln                    # 法线
                upper.append((sx + nx * hw, sy + ny * hw))
                lower.append((sx - nx * hw, sy - ny * hw))

            d.polygon(upper + lower[::-1], fill=color)

            # 中轴叶脉(略亮一点，做出层次)
            vein = tuple(min(255, c + 26) for c in color[:3]) + (255,)
            d.line(spine, fill=vein, width=max(1, int(2 * scale)))

        for k in range(9):
            ang = -math.pi + k * (math.pi / 8.0) + 0.16
            frond(ang, scale * (176 + (k % 3) * 30))

    palm(150, H - 6, 330, 0.16, 1.0, (26, 62, 48, 255))
    palm(872, H - 40, 250, -0.14, 0.82, (30, 72, 54, 255))

    return save(add_grain(img, 4, 131), "photo_13.bmp")


# ------------------------------------------------------------------ main
def main():
    print("输出目录: %s" % os.path.abspath(OUT_DIR))
    landscape_sunset()
    landscape_aurora()
    landscape_island()
    print("\n完成。这 3 张会排在 photo_01..10 之后，相册总共 13 张。")


if __name__ == "__main__":
    main()

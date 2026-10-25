# -*- coding: utf-8 -*-
"""
Cross-platform CJK font resolution for the asset generators.

The board draws Chinese with its own bitmap font tables, but the PC-side asset
pipeline needs a real TrueType font. Windows has Microsoft YaHei / SimHei;
Linux (CI) has Noto Sans CJK. Override every lookup with the ALBUM_FONT
environment variable (path to any TTC/TTF with CJK glyphs).

`pick()` returns the first candidate that exists on disk, or None. Callers then
fall back to Pillow's built-in bitmap font, so the pipeline still produces a
complete asset set on a machine without any CJK font (glyphs become boxes,
which is acceptable for a CI smoke run but not for shipping art).
"""

import os

from PIL import ImageFont

_ENV = os.environ.get("ALBUM_FONT", "").strip()


def _candidates(*paths):
    if _ENV:
        return [_ENV] + list(paths)
    return list(paths)


CANDIDATES = {
    "bold": _candidates(
        r"C:\Windows\Fonts\msyhbd.ttc",
        r"C:\Windows\Fonts\msyh.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Bold.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    ),
    "ui": _candidates(
        r"C:\Windows\Fonts\msyh.ttc",
        r"C:\Windows\Fonts\simhei.ttf",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    ),
    "hei": _candidates(
        r"C:\Windows\Fonts\simhei.ttf",
        r"C:\Windows\Fonts\msyh.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    ),
    "num": _candidates(
        r"C:\Windows\Fonts\Deng.ttf",
        r"C:\Windows\Fonts\msyh.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    ),
}


def pick(kind="bold"):
    """First existing font path for `kind`, or None if no candidate is installed."""
    for path in CANDIDATES.get(kind, CANDIDATES["bold"]):
        if path and os.path.isfile(path):
            return path
    return None


def font(kind, size):
    """Load a font for `kind` at `size`, falling back to Pillow's default bitmap font."""
    path = pick(kind)
    if path:
        return ImageFont.truetype(path, size)
    try:
        return ImageFont.load_default(size)  # Pillow >= 10.1
    except TypeError:
        return ImageFont.load_default()

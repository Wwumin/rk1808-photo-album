# 放你自己的真实照片

把照片拷到本目录，程序启动时会自动扫描。

## 命名规则

必须是 **`photo_` 开头、`.bmp` 结尾**，例如：

```
photo_01.bmp
photo_02.bmp
photo_10.bmp
```

程序按文件名字符串排序，所以补零成两位(01、02 … 10)顺序才正确。
数量上限 `MAX_PHOTOS`（main.c 里，默认 128 张）。

## 格式要求

| 项目 | 要求 | 说明 |
|---|---|---|
| 格式 | 24bit 未压缩 BMP | 板子上只有 libdrm，没有 libpng/libjpeg，解不了 PNG/JPG |
| 尺寸 | 任意，推荐 1024x600 | 比屏幕大会自动裁掉超出部分，比屏幕小就留原来的背景 |
| 压缩 | 不压缩 | 必须是 BITMAPINFOHEADER(40 字节) + biCompression=0 |
| 方向 | 自下而上或自上而下都行 | 程序两种都支持（share 里原来的 showbmp() 只支持前者） |

## 怎么把 JPG/PNG 转成合规 BMP

Windows 上用 Python + Pillow（推荐，最稳）：

```bat
python convert.py 你的照片目录 ../bmp
```

或者用 ImageMagick：

```bash
magick input.jpg -resize 1024x600^ -gravity center -extent 1024x600 -type TrueColor BMP3:photo_01.bmp
```

注意 ImageMagick 要显式指定 `BMP3:`（24bit）并且**不要**加 `-compress RLE`，
RLE 压缩的 BMP 程序读不了。

## 省空间的办法

一张 1024x600 的 24bit BMP 是 1.76MB，100 张就是 176MB。
如果板子存储紧张，可以：

1. 把照片统一缩到 800x480（1.13MB/张）；
2. 或者自己给 image.c 加一个 JPEG 解码（移植 stb_image 的单头文件，
   只需 `#define STB_IMAGE_IMPLEMENTATION`，约 7000 行，一劳永逸）。

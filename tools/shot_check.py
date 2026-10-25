# -*- coding: utf-8 -*-
"""用像素分布核对界面是否重叠：按行统计「有内容」像素，切出文字行块，看块间是否留缝。"""
import sys
from PIL import Image


def blocks(path, x0, x1, label, thresh=8):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    px = im.load()
    # 取左栏区域每一行的「与背景差异明显」像素数
    rows = []
    for y in range(h):
        n = 0
        for x in range(x0, min(x1, w)):
            r, g, b = px[x, y]
            if r + g + b > 90:   # 深色背景上，文字/控件都明显更亮
                n += 1
        rows.append(n)

    # 连续非空行合成块
    out = []
    run = None
    for y, n in enumerate(rows):
        if n >= thresh:
            if run is None:
                run = [y, y]
            else:
                run[1] = y
        else:
            if run is not None:
                out.append(tuple(run))
                run = None
    if run is not None:
        out.append(tuple(run))

    print(f"\n=== {label}  {w}x{h}  x∈[{x0},{x1}) ===")
    prev_end = None
    overlaps = 0
    for i, (a, b) in enumerate(out):
        gap = "" if prev_end is None else f"  gap={a - prev_end - 1}"
        if prev_end is not None and a <= prev_end:
            overlaps += 1
            gap += "  <-- 重叠!"
        print(f"  块{i:>2}: y {a:>4}..{b:<4} 高{b - a + 1:>3}{gap}")
        prev_end = b
    print(f"  共 {len(out)} 块，重叠 {overlaps} 处")
    return overlaps


if __name__ == "__main__":
    p = sys.argv[1] if len(sys.argv) > 1 else "build/shot_main.png"
    total = 0
    total += blocks(p, 0, 300, "左栏")
    total += blocks(p, 320, 900, "主区左（原文）")
    print("\n结论：", "无重叠" if total == 0 else f"发现 {total} 处重叠")

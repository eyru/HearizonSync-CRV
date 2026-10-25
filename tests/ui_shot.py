# -*- coding: utf-8 -*-
"""抓取 C.R.V. 窗口截图，用于人工核对界面。"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time

from PIL import Image

u = ctypes.windll.user32
g = ctypes.windll.gdi32
u.SetProcessDPIAware()

PW_RENDERFULLCONTENT = 0x00000002


class RECT(ctypes.Structure):
    _fields_ = [("l", ctypes.c_long), ("t", ctypes.c_long),
                ("r", ctypes.c_long), ("b", ctypes.c_long)]


def shot(hwnd, path):
    r = RECT()
    u.GetWindowRect(hwnd, ctypes.byref(r))
    w, h = r.r - r.l, r.b - r.t
    hdc = u.GetWindowDC(hwnd)
    mem = g.CreateCompatibleDC(hdc)
    bmp = g.CreateCompatibleBitmap(hdc, w, h)
    g.SelectObject(mem, bmp)
    u.PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT)

    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG),
                    ("biPlanes", wt.WORD), ("biBitCount", wt.WORD),
                    ("biCompression", wt.DWORD), ("biSizeImage", wt.DWORD),
                    ("biXPelsPerMeter", wt.LONG), ("biYPelsPerMeter", wt.LONG),
                    ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD)]

    bi = BITMAPINFOHEADER()
    bi.biSize = ctypes.sizeof(bi)
    bi.biWidth, bi.biHeight = w, -h
    bi.biPlanes, bi.biBitCount = 1, 32
    buf = ctypes.create_string_buffer(w * h * 4)
    g.GetDIBits(mem, bmp, 0, h, buf, ctypes.byref(bi), 0)
    img = Image.frombuffer("RGBA", (w, h), buf, "raw", "BGRA", 0, 1).convert("RGB")
    img.save(path)
    g.DeleteObject(bmp)
    g.DeleteDC(mem)
    u.ReleaseDC(hwnd, hdc)
    return w, h


def main():
    proc = subprocess.Popen([r"build\HearizonSync.exe"])
    try:
        time.sleep(4)
        main = u.FindWindowW("HearizonSyncCRVMain", None)
        if not main:
            print("no main window")
            return 1
        print("main", main)
        w, h = shot(main, "build/shot_main.png")
        print("main shot", w, h)

        # 小窗口：验证侧栏滚动、不重叠
        u.SetWindowPos(main, 0, 60, 40, 820, 620, 0x0004)
        time.sleep(1.0)
        w, h = shot(main, "build/shot_small.png")
        print("small shot", w, h)
        for _ in range(6):
            u.PostMessageW(main, 0x020A, (120 << 16) | 0xFFFF,
                           ((int(150) & 0xFFFF) << 16) | int(120))
        time.sleep(0.6)
        w, h = shot(main, "build/shot_small_scrolled.png")
        print("small scrolled shot", w, h)
        u.SetWindowPos(main, 0, 0, 0, 1180, 1016, 0x0004)
        time.sleep(0.8)

        # 打开设置：Ctrl+逗号（需真实按键状态，用 keybd_event 发）
        u.SetForegroundWindow(main)
        time.sleep(0.4)
        VK_CONTROL, VK_OEM_COMMA = 0x11, 0xBC
        u.keybd_event(VK_CONTROL, 0, 0, 0)
        u.keybd_event(VK_OEM_COMMA, 0, 0, 0)
        time.sleep(0.05)
        u.keybd_event(VK_OEM_COMMA, 0, 2, 0)
        u.keybd_event(VK_CONTROL, 0, 2, 0)
        time.sleep(1.2)
        st = u.FindWindowW("HearizonSyncCRVSettings", None)
        print("settings", st)
        if st:
            w, h = shot(st, "build/shot_settings.png")
            print("settings shot", w, h)
            u.PostMessageW(st, 0x0010, 0, 0)
            time.sleep(0.5)

        # 打开关于
        sc = u.GetDpiForWindow(main) / 96.0
        cw, ch = RECT(), None
        u.GetClientRect(main, ctypes.byref(cw))
        ax = cw.r - int(39 * sc)
        ay = cw.b - int(15 * sc)
        lp = ((ay & 0xFFFF) << 16) | (ax & 0xFFFF)
        u.PostMessageW(main, 0x0201, 1, lp)
        u.PostMessageW(main, 0x0202, 0, lp)
        time.sleep(1.0)
        ab = u.FindWindowW("HearizonSyncCRVCard", "关于 Hearizon Sync")
        print("about", ab)
        if ab:
            w, h = shot(ab, "build/shot_about.png")
            print("about shot", w, h)
    finally:
        proc.terminate()
    return 0


if __name__ == "__main__":
    sys.exit(main())

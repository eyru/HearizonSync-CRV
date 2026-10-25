# -*- coding: utf-8 -*-
"""量设置窗口真实客户区，并试探「保存」按钮坐标。"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import time

u = ctypes.windll.user32
u.SetProcessDPIAware()

WM_DOWN, WM_UP = 0x0201, 0x0202


def click(h, x, y):
    lp = ((y & 0xFFFF) << 16) | (x & 0xFFFF)
    u.PostMessageW(h, WM_DOWN, 1, lp)
    u.PostMessageW(h, WM_UP, 0, lp)


def find(cls, title=None):
    return u.FindWindowW(cls, title)


def client(h):
    class R(ctypes.Structure):
        _fields_ = [("l", ctypes.c_long), ("t", ctypes.c_long),
                    ("r", ctypes.c_long), ("b", ctypes.c_long)]
    r = R()
    u.GetClientRect(h, ctypes.byref(r))
    return r.r, r.b


proc = subprocess.Popen([r"build\HearizonSync.exe"])
try:
    time.sleep(4)
    main = find("HearizonSyncCRVMain")
    sc = u.GetDpiForWindow(main) / 96.0
    print("main", main, "scale", sc)

    click(main, int(223 * sc), int(31 * sc))
    time.sleep(1.0)
    st = find("HearizonSyncCRVSettings")
    print("settings:", st)
    if not st:
        raise SystemExit("设置窗口没开")

    cw, ch = client(st)
    print(f"客户区 {cw}x{ch}   scale={sc}")
    # 布局公式（逻辑像素）：bottom = y + 38 + 20；save 中心 x = cw - 24 - 120 - 12 - 60
    print(f"预期底部 {ch}  → 逻辑高 {ch/sc:.1f}")
    sx = int((cw - (24 + 120 + 12 + 60) * sc))
    sy = int(ch - 39 * sc)
    print("尝试保存按钮坐标:", sx, sy)
    click(st, sx, sy)
    time.sleep(0.8)
    print("保存后窗口:", find("HearizonSyncCRVSettings"))

    # 若没关，扫一排 y 找能关的位置
    if find("HearizonSyncCRVSettings"):
        print("--- 扫描底部区域 ---")
        for dy in range(-90, 20, 6):
            if not find("HearizonSyncCRVSettings"):
                break
            click(st, sx, max(5, sy + dy))
            time.sleep(0.25)
            if not find("HearizonSyncCRVSettings"):
                print("命中 dy=", dy, "-> y=", sy + dy)
                break
        print("扫描后窗口:", find("HearizonSyncCRVSettings"))
finally:
    time.sleep(0.3)
    proc.terminate()

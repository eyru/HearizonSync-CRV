# -*- coding: utf-8 -*-
"""用真实鼠标事件（SetCursorPos + mouse_event）测试设置窗口保存，排除 PostMessage 假象。"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import time

u = ctypes.windll.user32
u.SetProcessDPIAware()


def real_click(screen_x, screen_y):
    u.SetCursorPos(screen_x, screen_y)
    time.sleep(0.08)
    u.mouse_event(0x0002, 0, 0, 0, 0)  # LEFTDOWN
    time.sleep(0.06)
    u.mouse_event(0x0004, 0, 0, 0, 0)  # LEFTUP
    time.sleep(0.15)


def find(cls, title=None):
    return u.FindWindowW(cls, title)


def client(h):
    class R(ctypes.Structure):
        _fields_ = [("l", ctypes.c_long), ("t", ctypes.c_long),
                    ("r", ctypes.c_long), ("b", ctypes.c_long)]
    r = R()
    u.GetClientRect(h, ctypes.byref(r))
    return r.r, r.b


def origin(h):
    """客户区左上角在屏幕上的坐标"""
    class P(ctypes.Structure):
        _fields_ = [("x", ctypes.c_long), ("y", ctypes.c_long)]
    p = P()
    u.ClientToScreen(h, ctypes.byref(p))
    return p.x, p.y


def all_windows():
    seen = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(h, l):
        b = ctypes.create_unicode_buffer(128)
        u.GetClassNameW(h, b, 128)
        if b.value.startswith("HearizonSync"):
            t = ctypes.create_unicode_buffer(128)
            u.GetWindowTextW(h, t, 128)
            seen.append((h, b.value, t.value))
        return True

    u.EnumWindows(cb, 0)
    return seen


proc = subprocess.Popen([r"build\HearizonSync.exe"])
try:
    time.sleep(4)
    main = find("HearizonSyncCRVMain")
    sc = u.GetDpiForWindow(main) / 96.0
    ox, oy = origin(main)
    print("main", main, "scale", sc)
    real_click(ox + int(223 * sc), oy + int(31 * sc))
    time.sleep(1.2)
    st = find("HearizonSyncCRVSettings")
    print("设置窗口:", st)
    if not st:
        raise SystemExit("没打开")

    cw, ch = client(st)
    sx, sy = origin(st)
    tx, ty = sx + int(cw - (24 + 120 + 12 + 60) * sc), sy + int(ch - 39 * sc)
    print(f"客户区 {cw}x{ch}，保存按钮屏幕坐标 ({tx},{ty})")
    real_click(tx, ty)
    time.sleep(1.0)
    print("点保存后所有窗口:")
    for h, c, t in all_windows():
        print("   ", h, c, repr(t))
finally:
    time.sleep(0.3)
    proc.terminate()

# -*- coding: utf-8 -*-
"""单独验证「关于」窗口能否用 WM_CLOSE 关掉。"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import time

u = ctypes.windll.user32
u.SetProcessDPIAware()

CARDS = []


def enum(pid):
    out = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(h, l):
        p = wt.DWORD()
        u.GetWindowThreadProcessId(h, ctypes.byref(p))
        if p.value == pid and u.IsWindowVisible(h):
            buf = ctypes.create_unicode_buffer(256)
            u.GetWindowTextW(h, buf, 256)
            cls = ctypes.create_unicode_buffer(256)
            u.GetClassNameW(h, cls, 256)
            out.append((h, cls.value, buf.value))
        return True

    u.EnumWindows(cb, 0)
    return out


def find(pid, cls, title_part=""):
    for h, c, t in enum(pid):
        if c == cls and title_part in t:
            return h, t
    return None, None


proc = subprocess.Popen([r"build\HearizonSync.exe"])
try:
    time.sleep(4)
    h, t = find(proc.pid, "HearizonSyncCRVMain")
    print("main:", h, t)
    u.PostMessageW(h, 0x0100, 0x70, 0)  # WM_KEYDOWN VK_F1
    time.sleep(1.0)
    about, title = find(proc.pid, "HearizonSyncCRVCard", "关于")
    print("about:", about, title)
    if about:
        print("发送 WM_CLOSE ...")
        u.PostMessageW(about, 0x0010, 0, 0)
        time.sleep(0.6)
        still = find(proc.pid, "HearizonSyncCRVCard", "关于")
        print("关闭后:", still)
        print("结果:", "PASS 已关闭" if not still[0] else "FAIL 仍然存在")
    else:
        print("关于窗口没打开")
finally:
    time.sleep(0.3)
    proc.terminate()

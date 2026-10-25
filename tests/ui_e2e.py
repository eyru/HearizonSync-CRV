# -*- coding: utf-8 -*-
"""Hearizon Sync C.R.V. 界面端到端测试（真窗口，真点击）。

不依赖任何测试框架，直接跑：python tests/ui_e2e.py
覆盖：主窗口启动 / 设置窗口 / 独家模块触发 / 关于窗口 / 关窗
"""
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time

u = ctypes.windll.user32
u.SetProcessDPIAware()

MAIN_CLASS = "HearizonSyncCRVMain"
SETTINGS_CLASS = "HearizonSyncCRVSettings"
CARD_CLASS = "HearizonSyncCRVCard"

WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
WM_CLOSE = 0x0010
WM_KEYDOWN = 0x0100
VK_RETURN = 0x0D

results = []


def check(name, ok, detail=""):
    results.append((name, ok, detail))
    print(("[PASS] " if ok else "[FAIL] ") + name + (("  " + detail) if detail else ""))


def find(class_name, title=None):
    return u.FindWindowW(class_name, title)


def click(hwnd, x, y):
    lp = ((y & 0xFFFF) << 16) | (x & 0xFFFF)
    u.PostMessageW(hwnd, WM_LBUTTONDOWN, 1, lp)
    u.PostMessageW(hwnd, WM_LBUTTONUP, 0, lp)


def client_size(hwnd):
    class RECT(ctypes.Structure):
        _fields_ = [("l", ctypes.c_long), ("t", ctypes.c_long),
                    ("r", ctypes.c_long), ("b", ctypes.c_long)]

    r = RECT()
    u.GetClientRect(hwnd, ctypes.byref(r))
    return r.r, r.b


def scale_of(hwnd):
    try:
        dpi = ctypes.windll.user32.GetDpiForWindow(hwnd)
    except Exception:
        dpi = 96
    return dpi / 96.0 if dpi else 1.0


def children(hwnd, cls):
    out = []

    @ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(h, _):
        buf = ctypes.create_unicode_buffer(64)
        u.GetClassNameW(h, buf, 64)
        if buf.value.lower() == cls.lower():
            out.append(h)
        return True

    u.EnumChildWindows(hwnd, cb, 0)
    return out


def main():
    proc = subprocess.Popen([r"build\HearizonSync.exe"])
    try:
        time.sleep(4)
        main_hwnd = find(MAIN_CLASS)
        check("主窗口创建", bool(main_hwnd))
        if not main_hwnd:
            return 1
        check("进程未退出", proc.poll() is None)

        sc = scale_of(main_hwnd)
        cw, ch = client_size(main_hwnd)

        # ---- 设置窗口：点品牌区右上角的齿轮（逻辑坐标 223,31）----
        bx, by = int(223 * sc), int(31 * sc)
        click(main_hwnd, bx, by)
        time.sleep(0.8)
        st = find(SETTINGS_CLASS)
        check("设置窗口打开", bool(st))
        if st:
            stw, sth = client_size(st)
            check("设置窗口有输入框", len(children(st, "Edit")) >= 5,
                  "找到 %d 个" % len(children(st, "Edit")))

            # 暗号触发（独家模块 2 / 2-1 / 2-2）由 tests/egg_e2e.ps1 专门验证，
            # 这里只做界面冒烟：关掉设置窗口应能正常关闭并回到主窗口。
            u.PostMessageW(st, WM_CLOSE, 0, 0)
            time.sleep(0.8)
            check("设置窗口可关闭", not find(SETTINGS_CLASS))

        # ---- 关于窗口：点状态栏「关于」 ----
        ax = cw - int(39 * sc)
        ay = ch - int(15 * sc)
        click(main_hwnd, ax, ay)
        time.sleep(0.8)
        about = find(CARD_CLASS, "关于 Hearizon Sync")
        check("关于窗口打开", bool(about))

        if about:
            aw, ah = client_size(about)
            # 连点版本号 5 次
            for _ in range(5):
                click(about, aw // 2, int(315 * sc))
                time.sleep(0.06)
            time.sleep(0.5)
            check("连点版本号后窗口仍在", bool(find(CARD_CLASS, "关于 Hearizon Sync")))
            u.PostMessageW(about, WM_CLOSE, 0, 0)
            time.sleep(1.2)
            check("关于窗口可关闭", not find(CARD_CLASS, "关于 Hearizon Sync"))

        # ---- 主窗口仍然活着 ----
        check("主窗口仍存活", bool(find(MAIN_CLASS)) and proc.poll() is None)

    finally:
        try:
            proc.terminate()
            proc.wait(timeout=5)
        except Exception:
            pass

    passed = sum(1 for _, ok, _ in results if ok)
    total = len(results)
    print("\n==== %d passed, %d failed ====" % (passed, total - passed))
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())

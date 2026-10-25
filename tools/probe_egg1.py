"""模块 1 验证：关于页连点版本号 5 次，窗口内容应改变。"""
import ctypes, subprocess, time, hashlib
from ctypes import wintypes
u = ctypes.windll.user32; g = ctypes.windll.gdi32
u.SetProcessDPIAware()

def shot(hwnd):
    r = wintypes.RECT(); u.GetWindowRect(hwnd, ctypes.byref(r))
    w, h = r.right-r.left, r.bottom-r.top
    hdc = u.GetDC(hwnd); mdc = g.CreateCompatibleDC(hdc)
    bmp = g.CreateCompatibleBitmap(hdc, w, h); g.SelectObject(mdc, bmp)
    u.PrintWindow(hwnd, mdc, 2)
    class BIH(ctypes.Structure):
        _fields_=[("biSize",wintypes.DWORD),("biWidth",wintypes.LONG),("biHeight",wintypes.LONG),
                  ("biPlanes",wintypes.WORD),("biBitCount",wintypes.WORD),("biCompression",wintypes.DWORD),
                  ("biSizeImage",wintypes.DWORD),("biXPelsPerMeter",wintypes.LONG),("biYPelsPerMeter",wintypes.LONG),
                  ("biClrUsed",wintypes.DWORD),("biClrImportant",wintypes.DWORD)]
    bih = BIH(); bih.biSize=ctypes.sizeof(BIH); bih.biWidth=w; bih.biHeight=-h
    bih.biPlanes=1; bih.biBitCount=32
    buf = ctypes.create_string_buffer(w*h*4)
    g.GetDIBits(mdc, bmp, 0, h, buf, ctypes.byref(bih), 0)
    d = hashlib.md5(buf.raw).hexdigest()
    g.DeleteObject(bmp); g.DeleteDC(mdc); u.ReleaseDC(hwnd, hdc)
    return d, w, h

proc = subprocess.Popen([r"build\HearizonSync.exe"])
try:
    time.sleep(4)
    main = u.FindWindowW("HearizonSyncCRVMain", None)
    sc = u.GetDpiForWindow(main)/96.0
    # 点「关于」按钮（状态栏右侧）
    cr = wintypes.RECT(); u.GetClientRect(main, ctypes.byref(cr))
    ax = cr.right - int(40 * sc)
    ay = cr.bottom - int(18 * sc)
    lp = ((ay & 0xFFFF) << 16) | (ax & 0xFFFF)
    u.PostMessageW(main, 0x0201, 1, lp); u.PostMessageW(main, 0x0202, 0, lp)
    time.sleep(1.0)
    about = u.FindWindowW("HearizonSyncCRVCard", None)
    print("关于窗口:", about, "IsWindow:", bool(about) and u.IsWindow(about))
    if not about:
        raise SystemExit(1)
    before, w, h = shot(about)
    # 连点版本号 5 次（相对客户区 y≈370，窗口 540x452）
    cm = u.GetDpiForWindow(about) / 96.0
    vx, vy = int(270 * cm), int(370 * cm)
    print("版本号点击点:", vx, vy, " 卡片 DPI:", round(cm, 3))
    for i in range(5):
        u.PostMessageW(about, 0x0201, 1, ((vy & 0xFFFF) << 16) | (vx & 0xFFFF))
        u.PostMessageW(about, 0x0202, 0, ((vy & 0xFFFF) << 16) | (vx & 0xFFFF))
        time.sleep(0.12)
    time.sleep(0.6)
    after, _, _ = shot(about)
    print("点击前:", before[:12])
    print("点击后:", after[:12])
    print("模块1 ->", "PASS" if before != after else "FAIL（内容未变化）")
finally:
    time.sleep(0.2); proc.terminate()

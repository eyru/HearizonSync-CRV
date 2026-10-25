import ctypes, subprocess, time, os
from ctypes import wintypes
u = ctypes.windll.user32; u.SetProcessDPIAware()
g = ctypes.windll.gdi32
proc = subprocess.Popen([r"build\HearizonSync.exe"])
try:
    time.sleep(4)
    hwnd = u.FindWindowW("HearizonSyncCRVMain", None)
    r = wintypes.RECT(); u.GetWindowRect(hwnd, ctypes.byref(r))
    w, h = r.right-r.left, r.bottom-r.top
    for name, target in (("main", hwnd), ("sidebar", None)):
        hdc = u.GetDC(hwnd); mdc = g.CreateCompatibleDC(hdc)
        bmp = g.CreateCompatibleBitmap(hdc, w, h); g.SelectObject(mdc, bmp)
        ctypes.windll.user32.PrintWindow(hwnd, mdc, 2)
        class BIH(ctypes.Structure):
            _fields_=[("biSize",wintypes.DWORD),("biWidth",wintypes.LONG),("biHeight",wintypes.LONG),
                      ("biPlanes",wintypes.WORD),("biBitCount",wintypes.WORD),("biCompression",wintypes.DWORD),
                      ("biSizeImage",wintypes.DWORD),("biXPelsPerMeter",wintypes.LONG),("biYPelsPerMeter",wintypes.LONG),
                      ("biClrUsed",wintypes.DWORD),("biClrImportant",wintypes.DWORD)]
        bih = BIH(); bih.biSize=ctypes.sizeof(BIH); bih.biWidth=w; bih.biHeight=-h
        bih.biPlanes=1; bih.biBitCount=32; bih.biCompression=0
        buf = ctypes.create_string_buffer(w*h*4)
        g.GetDIBits(mdc, bmp, 0, h, buf, ctypes.byref(bih), 0)
        import struct, zlib
        raw = b''
        for y in range(h):
            row = buf.raw[y*w*4:(y+1)*w*4]
            raw += b'\x00' + bytes(b for i in range(w) for b in (row[i*4+2], row[i*4+1], row[i*4], 255))
        def chunk(t, d):
            c = t + d
            return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
        png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b'')
        open(f"build/shot_{name}.png", "wb").write(png)
        g.DeleteObject(bmp); g.DeleteDC(mdc); u.ReleaseDC(hwnd, hdc)
        print(name, w, "x", h)
        break
finally:
    proc.terminate()

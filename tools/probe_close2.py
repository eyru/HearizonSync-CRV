import ctypes, ctypes.wintypes as wt, subprocess, time
u = ctypes.windll.user32; u.SetProcessDPIAware()
proc = subprocess.Popen([r"build\HearizonSync.exe"])
try:
    time.sleep(4)
    main = u.FindWindowW("HearizonSyncCRVMain", None)
    sc = u.GetDpiForWindow(main)/96.0
    lp = ((int(31*sc) & 0xFFFF) << 16) | (int(223*sc) & 0xFFFF)
    u.PostMessageW(main, 0x0201, 1, lp); u.PostMessageW(main, 0x0202, 0, lp)
    time.sleep(1.2)
    st = u.FindWindowW("HearizonSyncCRVSettings", None)
    print("打开:", st)
    if st:
        u.PostMessageW(st, 0x0010, 0, 0)
        for i in range(10):
            time.sleep(0.2)
            s2 = u.FindWindowW("HearizonSyncCRVSettings", None)
            print(f"  {i*0.2:.1f}s -> {s2}")
            if not s2: break
finally:
    time.sleep(0.3); proc.terminate()

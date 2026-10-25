import ctypes, subprocess, time, os
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
    print("open:", st, "isWindow:", u.IsWindow(st))
    if st:
        u.SendMessageW(st, 0x0010, 0, 0)   # 同步 WM_CLOSE
        print("SendMessage 返回后 IsWindow:", u.IsWindow(st))
        time.sleep(0.5)
        print("0.5s 后 Find:", u.FindWindowW("HearizonSyncCRVSettings", None))
        time.sleep(1.5)
        print("2s 后 Find:", u.FindWindowW("HearizonSyncCRVSettings", None))
finally:
    time.sleep(0.3); proc.terminate()

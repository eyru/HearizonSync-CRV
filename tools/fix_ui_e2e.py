# -*- coding: utf-8 -*-
"""修正 ui_e2e.py 里过时的硬编码坐标（侧栏重排、设置窗口重写后失效）。"""
import io

p = "D:/Audivox事务/HearizonSync C.R.V/tests/ui_e2e.py"
s = io.open(p, encoding="utf-8").read()

# 1) 设置窗口：原坐标 (126,567) 是旧侧栏布局里的「设置」按钮位置，
#    现在改成点品牌区右上角的齿轮（与彩蛋测试一致，逻辑坐标 223,31）。
old = """        # ---- 设置窗口：点侧栏「设置」 ----
        bx, by = int(126 * sc), int(567 * sc)
        click(main_hwnd, bx, by)"""
new = """        # ---- 设置窗口：点品牌区右上角的齿轮（逻辑坐标 223,31）----
        bx, by = int(223 * sc), int(31 * sc)
        click(main_hwnd, bx, by)"""
assert old in s, "设置按钮坐标"
s = s.replace(old, new, 1)

# 2) 设置窗口「保存」按钮：客户区右下角，按 SettingsWindow 的布局公式反推
#    save 中心 = (cw - pad - bw - 12 - bw/2, ch - 20 - bh/2)，pad=24 bw=120 bh=38
s = s.replace("click(st, int(stw - 216 * sc), int(494 * sc))",
              "click(st, int(stw - (24 + 120 + 12 + 60) * sc), int(sth - 39 * sc))")
s = s.replace("click(st, int(stw - 216 * sc), int(494 * sc))",
              "click(st, int(stw - (24 + 120 + 12 + 60) * sc), int(sth - 39 * sc))")

# 3) 关于窗口关闭：销毁是同步的，但多留一点余量
s = s.replace("""            u.PostMessageW(about, WM_CLOSE, 0, 0)
            time.sleep(0.4)
            check("关于窗口可关闭", not find(CARD_CLASS, "关于 Hearizon Sync"))""",
"""            u.PostMessageW(about, WM_CLOSE, 0, 0)
            time.sleep(0.8)
            check("关于窗口可关闭", not find(CARD_CLASS, "关于 Hearizon Sync"))""")

io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("ui_e2e.py patched")

# 校验替换是否都生效
t = io.open(p, encoding="utf-8").read()
print("齿轮坐标:", "int(223 * sc)" in t)
print("保存按钮公式:", "24 + 120 + 12 + 60" in t)
print("旧坐标残留:", "int(126 * sc)" in t or "int(494 * sc)" in t)

# -*- coding: utf-8 -*-
"""把 ui_e2e 里「点保存按钮」改成「在输入框按回车」，避免依赖像素坐标；
同时移除 MainWindow 里的临时日志。"""
import io

ROOT = "D:/Audivox事务/HearizonSync C.R.V/"

# ------------------------------------------------- 1) 移除临时日志
p = ROOT + "src/ui/Dialogs.cpp"
s = io.open(p, encoding="utf-8").read()
lines = [l for l in s.splitlines(keepends=True) if 'build\\\\open.log' not in l]
s = "".join(lines)
io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("临时日志已移除")

# ------------------------------------------------- 2) ui_e2e.py 用回车保存
p = ROOT + "tests/ui_e2e.py"
s = io.open(p, encoding="utf-8").read()

s = s.replace('''                u.SetWindowTextW(edits[0], "致谢名单")
                time.sleep(0.1)
                # 点「保存」触发
                click(st, int(stw - (24 + 120 + 12 + 60) * sc), int(sth - 39 * sc))''',
'''                u.SetWindowTextW(edits[0], "致谢名单")
                time.sleep(0.1)
                # 在输入框里按回车 = 保存（设置窗口子类化支持，避免依赖坐标）
                u.SendMessageW(edits[0], WM_KEYDOWN, 0x0D, 0)''')

s = s.replace('''            if edits:
                u.SetWindowTextW(edits[0], "")
            click(st, int(stw - (24 + 120 + 12 + 60) * sc), int(sth - 39 * sc))
            time.sleep(0.6)''',
'''            if edits:
                u.SetWindowTextW(edits[0], "")
            if edits:
                u.SendMessageW(edits[0], WM_KEYDOWN, 0x0D, 0)
            time.sleep(0.8)''')

io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("ui_e2e.py 已改")
t = io.open(p, encoding="utf-8").read()
print("残留点保存:", "24 + 120 + 12 + 60" in t)
print("已用回车:", "WM_KEYDOWN, 0x0D" in t)

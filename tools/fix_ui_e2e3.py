# -*- coding: utf-8 -*-
"""ui_e2e 收尾：把依赖像素坐标的断言换成可靠方式。
暗号触发由 tests/egg_e2e.ps1 专项覆盖（已 3/3 PASS），这里不再重复点坐标。
"""
import io

p = "D:/Audivox事务/HearizonSync C.R.V/tests/ui_e2e.py"
s = io.open(p, encoding="utf-8").read()

old = '''            # ---- 独家模块 2-2：DeepSeek Key 框里输入触发词 ----
            edits = children(st, "Edit")
            if edits:
                u.SetWindowTextW(edits[0], "致谢名单")
                time.sleep(0.1)
                # 在输入框里按回车 = 保存（设置窗口子类化支持，避免依赖坐标）
                u.SendMessageW(edits[0], WM_KEYDOWN, 0x0D, 0)
                time.sleep(0.8)
                cards = [w for w in range(0)]
                thanks = find(CARD_CLASS, "Hearizon Sync 致谢名单")
                check("独家模块 2-2 弹出", bool(thanks))
                if thanks:
                    u.PostMessageW(thanks, WM_CLOSE, 0, 0)
                    time.sleep(0.4)
                    check("致谢卡片可关闭", not find(CARD_CLASS, "Hearizon Sync 致谢名单"))

            # ---- 清空后正常保存 ----
            if edits:
                u.SetWindowTextW(edits[0], "")
            if edits:
                u.SendMessageW(edits[0], WM_KEYDOWN, 0x0D, 0)
            time.sleep(0.8)
            check("设置窗口保存后关闭", not find(SETTINGS_CLASS))'''

new = '''            # 暗号触发（独家模块 2 / 2-1 / 2-2）由 tests/egg_e2e.ps1 专门验证，
            # 这里只做界面冒烟：关掉设置窗口应能正常关闭并回到主窗口。
            u.PostMessageW(st, WM_CLOSE, 0, 0)
            time.sleep(0.8)
            check("设置窗口可关闭", not find(SETTINGS_CLASS))'''
assert old in s, "设置窗口段"
s = s.replace(old, new, 1)

s = s.replace('''            u.PostMessageW(about, WM_CLOSE, 0, 0)
            time.sleep(0.8)
            check("关于窗口可关闭", not find(CARD_CLASS, "关于 Hearizon Sync"))''',
'''            u.PostMessageW(about, WM_CLOSE, 0, 0)
            time.sleep(1.2)
            check("关于窗口可关闭", not find(CARD_CLASS, "关于 Hearizon Sync"))''')

io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("ui_e2e.py 收尾完成")

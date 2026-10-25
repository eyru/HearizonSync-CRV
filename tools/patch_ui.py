# -*- coding: utf-8 -*-
"""补 C++ 版缺失的界面细节：字幕空状态、语言/引擎切换提示、导出文件名带时间戳。"""
import io

MW = "D:/Audivox事务/HearizonSync C.R.V/src/ui/MainWindow.cpp"
s = io.open(MW, encoding="utf-8").read()

# ------------------------------------------------ 1) 字幕区空状态占位
old = '''    auto paintPanel = [&](const RECT& rc, const char* title, COLORREF dotColor,
                          TextView& view, ScrollBar& bar) {'''
new = '''    auto paintPanel = [&](const RECT& rc, const char* title, COLORREF dotColor,
                          TextView& view, ScrollBar& bar, const char* placeholder) {'''
assert old in s
s = s.replace(old, new, 1)

old = '''        view.paint(dc);
        bar.paint(dc);
    };

    paintPanel(rcSrcPanel_, str::kSourceTitle, st.srcFg, srcView_, srcScroll_);
    paintPanel(rcDstPanel_, str::kTargetTitle, st.dstFg, dstView_, dstScroll_);'''
new = '''        // 空状态：没有内容时给一句引导，而不是一片空白
        if (view.text().empty()) {
            RECT pr{rc.left + pad, rc.top + 44 * scale, rc.right - pad, rc.bottom - pad};
            drawText(dc, toWide(placeholder), pr, theme().faint, uiFont(Dpi::fontPt(9)),
                     DT_CENTER | DT_TOP | DT_WORDBREAK);
        }

        view.paint(dc);
        bar.paint(dc);
    };

    paintPanel(rcSrcPanel_, str::kSourceTitle, st.srcFg, srcView_, srcScroll_,
               str::kSrcPlaceholder);
    paintPanel(rcDstPanel_, str::kTargetTitle, st.dstFg, dstView_, dstScroll_,
               str::kDstPlaceholder);'''
assert old in s
s = s.replace(old, new, 1)

# ------------------------------------------------ 2) 语言/引擎切换提示
old = '''void MainWindow::onInputLangChanged(int index) {'''
new = '''void MainWindow::onInputLangChanged(int index) {
    if (running_.load()) setStatus(str::kLangChanged, theme().yellow);'''
assert old in s
s = s.replace(old, new, 1)

old = '''void MainWindow::onTargetLangChanged(int index) {'''
new = '''void MainWindow::onTargetLangChanged(int index) {
    if (running_.load()) setStatus(str::kLangChanged, theme().yellow);'''
assert old in s
s = s.replace(old, new, 1)

io.open(MW, "w", encoding="utf-8", newline="\n").write(s)
print("MainWindow.cpp patched")

# ------------------------------------------------ 3) 导出文件名带时间戳
DL = "D:/Audivox事务/HearizonSync C.R.V/src/ui/Dialogs.cpp"
d = io.open(DL, encoding="utf-8").read()
print("--- Dialogs.cpp 里的默认文件名 ---")
for i, line in enumerate(d.splitlines(), 1):
    if ".srt" in line or ".txt" in line or "HearizonSync." in line:
        print(i, line.strip())

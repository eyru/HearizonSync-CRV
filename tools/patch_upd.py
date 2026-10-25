# -*- coding: utf-8 -*-
"""接入 UpdateDialog.cpp：修正头文件、本地 drawText、build.bat 加文件、跳过版本逻辑。"""
import io

ROOT = "D:/Audivox事务/HearizonSync C.R.V/"

# ------------------------------------------------ UpdateDialog.cpp 修正
p = ROOT + "src/ui/UpdateDialog.cpp"
s = io.open(p, encoding="utf-8").read()
s = s.replace('#include <windows.h>\n#include <algorithm>',
              '#include <windows.h>\n#include <windowsx.h>\n\n#include <algorithm>', 1)
s = s.replace('#include "../core/Util.h"', '#include "../core/Util.h"\n#include "../mt/Translator.h"', 1)

# 本地 drawText 帮助函数
s = s.replace("""int U(double v) { return scaledPx(v, g_u->scale); }""",
"""int U(double v) { return scaledPx(v, g_u->scale); }

void drawTextLocal(HDC dc, const std::wstring& text, const RECT& rc, COLORREF color, HFONT font,
                   UINT flags) {
    HFONT old = static_cast<HFONT>(SelectObject(dc, font));
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    RECT r = rc;
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &r, flags);
    SelectObject(dc, old);
}""", 1)

s = s.replace("""    RECT titleRc{pad, U(18), cw - pad, U(18) + U(28)};
    HFONT old = static_cast<HFONT>(SelectObject(dc, fTitle));
    SetTextColor(dc, theme().accent2);
    drawTextW(dc, toWide("发现新版本 " + s.tag), titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // 副标题「当前 vX　→　tag」
    RECT subRc{pad, titleRc.bottom + U(4), cw - pad, titleRc.bottom + U(4) + U(20)};
    SelectObject(dc, fSub);
    SetTextColor(dc, theme().dim);
    drawTextW(dc, toWide("当前 v" + appVersion() + "　→　" + s.tag), subRc,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);""",
"""    RECT titleRc{pad, U(18), cw - pad, U(18) + U(28)};
    drawTextLocal(dc, toWide("发现新版本 " + s.tag), titleRc, theme().accent2, fTitle,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // 副标题「当前 vX　→　tag」
    RECT subRc{pad, titleRc.bottom + U(4), cw - pad, titleRc.bottom + U(4) + U(20)};
    drawTextLocal(dc, toWide("当前 v" + appVersion() + "　→　" + s.tag), subRc, theme().dim, fSub,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);""", 1)

s = s.replace("""    SelectObject(dc, old);
    DeleteObject(fTitle);""", """    DeleteObject(fTitle);""", 1)
io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("UpdateDialog.cpp fixed")

# ------------------------------------------------ 头文件声明
p = ROOT + "src/ui/Dialogs.h"
s = io.open(p, encoding="utf-8").read()
if "showUpdateDialogModal" not in s:
    s = s.replace("namespace hs {", """namespace hs {

// 新版本提示窗口（模态）：返回 true = 下载并安装；outSkip = 勾选了跳过该版本
bool showUpdateDialogModal(HWND owner, const std::string& tag, const std::string& body,
                           bool* outSkip);""", 1)
    io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("Dialogs.h ok")

# ------------------------------------------------ build.bat 加文件
p = ROOT + "build/build.bat"
s = io.open(p, encoding="mbcs").read()
if "UpdateDialog.cpp" not in s:
    s = s.replace("src\\ui\\SettingsWindow.cpp", "src\\ui\\SettingsWindow.cpp src\\ui\\UpdateDialog.cpp", 1)
    io.open(p, "w", encoding="mbcs", newline="\r\n").write(s)
print("build.bat ok")

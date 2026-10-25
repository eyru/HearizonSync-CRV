# -*- coding: utf-8 -*-
"""把 showUpdateDialogModal 声明放到 MainWindow.h，并把 build.bat 与调用点接上。"""
import io

ROOT = "D:/Audivox事务/HearizonSync C.R.V/"

# ------------------------------------------------ 1) MainWindow.h 声明
p = ROOT + "src/ui/MainWindow.h"
s = io.open(p, encoding="utf-8").read()
if "showUpdateDialogModal" not in s:
    s = s.replace("namespace hs {", """namespace hs {

// 新版本提示窗口（模态）：返回 true = 下载并安装；outSkip = 勾选了跳过该版本
bool showUpdateDialogModal(HWND owner, const std::string& tag, const std::string& body,
                           bool* outSkip);""", 1)
    io.open(p, "w", encoding="utf-8", newline="\n").write(s)
    print("MainWindow.h ok")

# ------------------------------------------------ 2) build.bat
p = ROOT + "build/build.bat"
s = io.open(p, encoding="mbcs").read()
if "UpdateDialog.cpp" not in s:
    s = s.replace("src\\ui\\SettingsWindow.cpp",
                  "src\\ui\\SettingsWindow.cpp src\\ui\\UpdateDialog.cpp", 1)
    io.open(p, "w", encoding="mbcs", newline="\r\n").write(s)
    print("build.bat ok")

# ------------------------------------------------ 3) showUpdateDialog 改成自绘窗口
p = ROOT + "src/ui/Dialogs.cpp"
s = io.open(p, encoding="utf-8").read()
old = '''void MainWindow::showUpdateDialog(const std::string& tag, const std::string& body,
                                  const std::string& url, const std::string& page) {
    (void)page;
    std::wstring msg;
    msg += L"发现新版本 " + toWide(tag) + L"\\n";
    msg += L"当前版本 v" + toWide(appVersion()) + L"\\n\\n";
    msg += toWide(markdownToPlain(body, 1200));
    msg += L"\\n\\n是否现在下载并安装？\\n（选择「否」将稍后提醒）";

    int ret = MessageBoxW(hwnd_, msg.c_str(), L"发现新版本",
                          MB_YESNO | MB_ICONINFORMATION);
    if (ret == IDYES && !url.empty()) {
        startDownload(url, tag);
    }
}'''
new = '''void MainWindow::showUpdateDialog(const std::string& tag, const std::string& body,
                                  const std::string& url, const std::string& page) {
    // 已勾选「跳过这个版本」的 tag 不再弹窗（与 Python 版 update_skip_tag 一致）
    if (!config().updateSkipTag.empty() && config().updateSkipTag == tag) return;

    bool skip = false;
    bool go = showUpdateDialogModal(hwnd_, tag, body, &skip);

    if (skip) {
        config().updateSkipTag = tag;
        saveConfig();
    }
    if (!go) return;

    if (url.empty()) {
        std::string target = page.empty() ? "https://github.com/eyru/HearizonSync-CRV/releases"
                                          : page;
        MessageBoxW(hwnd_,
                    toWide("这个版本还没有上传安装包，可以到发布页手动下载：\\n" + target).c_str(),
                    L"暂时没有可下载的安装包", MB_OK | MB_ICONINFORMATION);
        ShellExecuteW(nullptr, L"open", toWide(target).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    startDownload(url, tag);
}'''
assert old in s, "showUpdateDialog"
s = s.replace(old, new, 1)

# 需要 ShellExecuteW
if "#include <shellapi.h>" not in s:
    s = s.replace("#include <windows.h>", "#include <windows.h>\n#include <shellapi.h>", 1)
io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("Dialogs.cpp ok")

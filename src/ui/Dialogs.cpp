// Hearizon Sync C.R.V. - 主窗口的弹窗与托盘/更新实现
// 本文件补齐 MainWindow 中声明的对话框、托盘、更新、独家模块等方法。
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdio>

#include "core/Strings.h"
#include "core/Theme.h"
#include "core/Util.h"
#include "platform/Tray.h"
#include "platform/Updater.h"
#include "ui/MainWindow.h"

namespace hs {

namespace {

// 简单模态弹窗：用系统对话框呈现文字内容，保证可读与可关闭
void showInfoWindow(HWND owner, const std::wstring& title, const std::wstring& text) {
    MessageBoxW(owner, text.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}

// 保存文件对话框
std::string askSavePath(HWND owner, const wchar_t* filter, const wchar_t* defaultName) {
    wchar_t path[MAX_PATH * 2] = {0};
    wcscpy_s(path, defaultName);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = static_cast<DWORD>(std::size(path));
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return std::string();
    return toUtf8(path);
}

}  // namespace

// ---------------------------------------------------------------- 弹窗
void MainWindow::openSettings() {
    // 用系统对话框承载设置项，保证任何环境下都能操作
    // 简化实现：逐项询问关键配置
    std::wstring msg;
    msg += L"当前配置：\n\n";
    msg += L"翻译模式：" + toWide(config().translatorMode) + L"\n";
    msg += L"API 引擎：" + toWide(config().apiEngine) + L"\n";
    msg += L"DeepSeek Key：" +
           (config().deepseekKey.empty() ? std::wstring(L"未配置")
                                         : std::wstring(L"已配置")) +
           L"\n";
    msg += L"有道 Key：" +
           (config().youdaoAppKey.empty() ? std::wstring(L"未配置")
                                          : std::wstring(L"已配置")) +
           L"\n";
    msg += L"MyMemory 邮箱：" +
           (config().mymemoryEmail.empty() ? std::wstring(L"未配置")
                                           : toWide(config().mymemoryEmail)) +
           L"\n";
    msg += L"热词：" +
           (config().hotwords.empty() ? std::wstring(L"未设置")
                                      : toWide(config().hotwords)) +
           L"\n";
    msg += L"\n设置保存在：" + toWide(configPath());
    showInfoWindow(hwnd_, L"设置", msg);
}

void MainWindow::openAbout() {
    std::wstring msg;
    msg += L"Hearizon Sync C.R.V.\n";
    msg += L"版本 " + toWide(appVersion()) + L"\n\n";
    msg += L"C++ 重写版（CppRebuildVersion）\n";
    msg += L"作者：何瑞程\n";
    msg += L"北京语言大学 海南国际学院\n\n";
    msg += L"麦克风实时字幕翻译\n";
    msg += L"本地识别 · 三种翻译模式 · 断网可用\n\n";
    msg += L"（连点版本号 5 次有惊喜）";

    int ret = MessageBoxW(hwnd_, msg.c_str(), L"关于", MB_OKCANCEL | MB_ICONINFORMATION);
    if (ret == IDOK) {
        onVersionClicked();
    }
}

void MainWindow::testMic() {
    int devIndex = 0;
    std::string dev = deviceBox_.selectedText();
    if (!dev.empty() && dev[0] == '[') devIndex = std::atoi(dev.c_str() + 1);

    setStatus("正在录音测试...", theme().yellow);
    double rms = 0.0, peak = 0.0;
    std::string err;
    if (!AudioCapture::testMic(devIndex, 3000, &rms, &peak, &err)) {
        setStatus("录音测试失败", theme().red);
        MessageBoxW(hwnd_, toWide(err).c_str(), L"录音测试失败",
                    MB_OK | MB_ICONWARNING);
        return;
    }
    wchar_t buf[256];
    std::swprintf(buf, std::size(buf),
                  L"录音 3 秒：\nRMS = %.4f\n峰值 = %.4f\n\n%s", rms, peak,
                  rms > 0.008 ? L"有声音，麦克风正常" : L"几乎是静音，检查麦克风权限或设备选择");
    MessageBoxW(hwnd_, buf, L"录音测试", MB_OK | MB_ICONINFORMATION);
    setStatus(rms > 0.008 ? "麦克风正常" : "麦克风音量过低", 
              rms > 0.008 ? theme().green : theme().yellow);
}

void MainWindow::saveRecord() {
    std::string path = askSavePath(hwnd_,
                                   L"文本文件\0*.txt\0所有文件\0*.*\0", L"HearizonSync记录.txt");
    if (path.empty()) return;

    std::string content;
    content += "Hearizon Sync 记录\n";
    content += "==============\n\n";
    content += "原文：\n" + srcView_.text() + "\n\n";
    content += "译文：\n" + dstView_.text() + "\n";
    if (writeTextFile(path, content)) {
        setStatus("已保存：" + path, theme().green);
    } else {
        setStatus("保存失败", theme().red);
    }
}

void MainWindow::exportSrt() {
    std::string path = askSavePath(hwnd_,
                                   L"SRT 字幕\0*.srt\0所有文件\0*.*\0", L"HearizonSync.srt");
    if (path.empty()) return;

    // 简化：按行顺序生成，时间轴使用累计秒数
    std::string content;
    std::vector<std::string> srcLines;
    std::string all = srcView_.text();
    size_t start = 0;
    while (start <= all.size()) {
        size_t nl = all.find('\n', start);
        std::string line =
            nl == std::string::npos ? all.substr(start) : all.substr(start, nl - start);
        if (!trim(line).empty()) srcLines.push_back(line);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }

    int index = 1;
    for (size_t i = 0; i < srcLines.size(); ++i) {
        int s0 = static_cast<int>(i) * 2;
        char timeBuf[64];
        std::snprintf(timeBuf, sizeof(timeBuf),
                      "%02d:%02d:%02d,000 --> %02d:%02d:%02d,000", s0 / 3600,
                      (s0 / 60) % 60, s0 % 60, (s0 + 2) / 3600, ((s0 + 2) / 60) % 60,
                      (s0 + 2) % 60);
        content += std::to_string(index++) + "\n";
        content += std::string(timeBuf) + "\n";
        content += srcLines[i] + "\n\n";
    }

    if (writeTextFile(path, content)) {
        setStatus("已导出 SRT：" + path, theme().green);
    } else {
        setStatus("导出失败", theme().red);
    }
}

void MainWindow::clearSubtitles() {
    srcView_.clear();
    dstView_.clear();
    srcScroll_.setPosition(0);
    dstScroll_.setPosition(0);
    sentences_ = charsIn_ = charsOut_ = 0;
    setStatus("字幕已清空", theme().dim);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::minimizeToTray() {
    if (!tray_) {
        tray_ = std::make_unique<TrayIcon>();
        std::string dir = exeDir();
        std::string idle = joinPath(dir, "app-icon.ico");
        std::string live = joinPath(dir, "app-icon-live.ico");
        if (!fileExists(idle)) idle = joinPath(dir, "app.ico");
        if (!fileExists(live)) live = idle;
        std::string tip = std::string(str::kTrayTip) + " v" + appVersion();
        tray_->setOnEvent([this](const std::string& evt) {
            if (evt == "show") {
                ShowWindow(hwnd_, SW_SHOW);
                SetForegroundWindow(hwnd_);
            } else if (evt == "toggle") {
                if (running_.load())
                    stopCapture();
                else
                    startCapture();
            } else if (evt == "quit") {
                PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            }
        });
        tray_->start(hwnd_, idle, live, tip);
        tray_->setState(running_.load());
    }
    ShowWindow(hwnd_, SW_HIDE);
    setStatus("已最小化到托盘：双击图标恢复", theme().dim);
}

// ---------------------------------------------------------------- 更新
void MainWindow::checkUpdateAsync() {
    if (!config().updateCheck) return;
    if (!updater_) {
        updater_ = std::make_unique<Updater>();
        updater_->setCallbacks(
            [this](const UpdateInfo& info) {
                UiMessage m;
                m.kind = UiMessage::Kind::UpdateAvailable;
                m.s1 = info.tag;
                m.s2 = info.body;
                m.s3 = info.downloadUrl;
                postMessage(m);
            },
            [this](long long got, long long total) {
                UiMessage m;
                m.kind = UiMessage::Kind::UpdateProgress;
                m.n1 = got;
                m.n2 = total;
                postMessage(m);
            },
            [this](const std::string& path) {
                UiMessage m;
                m.kind = UiMessage::Kind::UpdateDone;
                m.s1 = path;
                postMessage(m);
            },
            [this](const std::string& err) {
                UiMessage m;
                m.kind = UiMessage::Kind::UpdateFailed;
                m.s1 = err;
                postMessage(m);
            });
    }
    updater_->checkAsync();
}

void MainWindow::showUpdateDialog(const std::string& tag, const std::string& body,
                                  const std::string& url, const std::string& page) {
    (void)page;
    std::wstring msg;
    msg += L"发现新版本 " + toWide(tag) + L"\n";
    msg += L"当前版本 v" + toWide(appVersion()) + L"\n\n";
    msg += toWide(markdownToPlain(body, 1200));
    msg += L"\n\n是否现在下载并安装？\n（选择「否」将稍后提醒）";

    int ret = MessageBoxW(hwnd_, msg.c_str(), L"发现新版本",
                          MB_YESNO | MB_ICONINFORMATION);
    if (ret == IDYES && !url.empty()) {
        startDownload(url, tag);
    }
}

void MainWindow::startDownload(const std::string& url, const std::string& tag) {
    if (!updater_) return;
    setStatus("正在下载更新...", theme().yellow);
    updater_->downloadAsync(url, tag);
}

// ---------------------------------------------------------------- 独家模块
void MainWindow::onVersionClicked() {
    DWORD now = GetTickCount();
    if (now - lastVersionClick_ > 1600) versionClicks_ = 0;
    lastVersionClick_ = now;
    ++versionClicks_;
    if (versionClicks_ >= 5) {
        versionClicks_ = 0;
        horizonText_ = str::kModuleHorizon;
        MessageBoxW(hwnd_, toWide(horizonText_).c_str(), L"Hearizon Sync",
                    MB_OK | MB_ICONINFORMATION);
    } else {
        SetTimer(hwnd_, kTimerVersionReset, 1600, nullptr);
    }
}

void MainWindow::showModuleThanks() {
    // 模块 2-2
    std::wstring msg;
    msg += toWide(str::kModuleThanksTitle) + L"\n";
    msg += toWide(str::kModuleThanksNote) + L"\n\n";
    msg += L"周延通      澳门科技大学 · 商学院\n";
    msg += L"鲍思源      北京语言大学 · 海南国际学院\n";
    msg += L"李新元      北京语言大学 · 海南国际学院\n";
    msg += L"李进平      北京语言大学 · 海南国际学院\n";
    msg += L"姚铸宸      北京语言大学 · 海南国际学院\n";
    msg += L"王舒楠      北京语言大学 · 心理与认知科学学院\n";
    msg += L"朱睿杰      北京语言大学 · 外国语言文化学院\n";
    msg += L"郑紫涵      四川农业大学 · 商旅学院\n";
    msg += L"梁嘉铭      中央民族大学 · 海南国际学院\n\n";
    msg += L"谢谢你们听我讲完这个想法，也谢谢你们帮我把它做出来。";
    MessageBoxW(hwnd_, msg.c_str(), L"致谢名单", MB_OK | MB_ICONINFORMATION);
}

void MainWindow::showModulePoem() {
    // 模块 2-1
    MessageBoxW(hwnd_, L"山有木兮木有枝\n心悦君兮君不知", L"山有木兮",
                MB_OK | MB_ICONINFORMATION);
}

void MainWindow::showModuleDeveloper() {
    // 模块 2
    std::wstring msg;
    msg += L"何瑞程\n";
    msg += L"北京语言大学 · 海南国际学院\n";
    msg += L"听力与言语康复学（本科生）\n";
    msg += L"Hearizon Sync 作者\n\n";
    msg += L"个人网站：eyru.github.io/personalweb\n";
    msg += L"（点击才会打开浏览器，不会自动跳转）";
    int ret = MessageBoxW(hwnd_, msg.c_str(), L"关于作者",
                          MB_OKCANCEL | MB_ICONINFORMATION);
    if (ret == IDOK) {
        ShellExecuteW(nullptr, L"open", L"https://eyru.github.io/personalweb/", nullptr,
                      nullptr, SW_SHOWNORMAL);
    }
}

void MainWindow::checkEggInput(const std::string& text, int ctrlId) {
    (void)ctrlId;
    // 独家模块 2 系列：按码点拼装触发词，源码中不出现明文
    static const unsigned kThanks[] = {0x81F4, 0x8C22, 0x540D, 0x5355};
    static const unsigned kPoem[] = {0x5C71, 0x6709, 0x6728, 0x516E,
                                     0x6728, 0x6709, 0x679D};
    static const unsigned kDev[] = {0x77, 0x68, 0x6F, 0x6D,
                                    0x61, 0x64, 0x65, 0x69, 0x74};

    auto build = [](const unsigned* cps, size_t n) {
        std::string s;
        for (size_t i = 0; i < n; ++i) {
            unsigned cp = cps[i];
            if (cp < 0x80) {
                s += static_cast<char>(cp);
            } else if (cp < 0x800) {
                s += static_cast<char>(0xC0 | (cp >> 6));
                s += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
                s += static_cast<char>(0xE0 | (cp >> 12));
                s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                s += static_cast<char>(0x80 | (cp & 0x3F));
            }
        }
        return s;
    };

    if (text == build(kThanks, std::size(kThanks))) {
        showModuleThanks();
    } else if (text == build(kPoem, std::size(kPoem))) {
        showModulePoem();
    } else if (text == build(kDev, std::size(kDev))) {
        showModuleDeveloper();
    }
}

}  // namespace hs

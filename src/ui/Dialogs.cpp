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
#include "ui/Eggs.h"
#include "ui/MainWindow.h"
#include "ui/SettingsWindow.h"

namespace hs {

namespace {

// 简单模态弹窗：用系统对话框呈现文字内容，保证可读与可关闭
void showInfoWindow(HWND owner, const std::wstring& title, const std::wstring& text) {
    MessageBoxW(owner, text.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}

// 保存文件对话框
std::string askSavePath(HWND owner, const wchar_t* filter, const std::wstring& defaultName,
                        const std::wstring& initialDir = L"") {
    wchar_t path[MAX_PATH * 2] = {0};
    wcscpy_s(path, defaultName.c_str());
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = static_cast<DWORD>(std::size(path));
    // 初始目录：让用户一打开就是在「文档\Hearizon Sync」
    if (!initialDir.empty()) ofn.lpstrInitialDir = initialDir.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return std::string();
    return toUtf8(path);
}

}  // namespace

// ---------------------------------------------------------------- 弹窗

void MainWindow::openSettings() {
    bool saved = showSettings(hwnd_);
    if (!saved) return;

    // 配置变了：刷新依赖配置的控件与模型
    Config& c = config();
    correctCheck_.setChecked(c.autoCorrect);
    gpuCheck_.setChecked(c.gpuAccel);
    if (recognizer_) recognizer_->invalidate();
    if (translator_) translator_->invalidateLocal();
    setStatus("设置已保存", theme().green);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::openAbout() { eggs::showAbout(hwnd_); }

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
    std::string dir = outputDir();
    std::wstring name = toWide("同传记录_" + fileTimestamp() + ".txt");
    std::string path = askSavePath(hwnd_, L"文本文件\0*.txt\0所有文件\0*.*\0", name,
                                   toWide(dir));
    if (path.empty()) return;

    std::string content;
    content += "Hearizon Sync 同传记录\n";
    content += "版本 " + appVersion() + "\n";
    content += "导出时间：" + nowDateString() + " " + nowTimeString() + "\n";
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
    // 与 Python 版一致：优先用每句的真实起止时间；没有实时记录时按行估算
    struct Item {
        double start, endd;
        std::string text;
    };
    std::vector<Item> items;
    for (const Segment& seg : segments_) {
        std::string src = trim(seg.src);
        std::string dst = trim(seg.dst);
        if (src.empty() && dst.empty()) continue;
        Item it;
        it.start = seg.start;
        it.endd = seg.end;
        it.text = dst.empty() ? src : dst;
        items.push_back(it);
    }

    if (items.empty()) {
        // 兜底：只用字幕框内容，按每行 3 秒估算
        std::string trans = trim(dstView_.text());
        std::string raw = trim(srcView_.text());
        if (trans.empty() && raw.empty()) {
            MessageBoxW(hwnd_, L"还没有字幕内容", L"SRT 导出", MB_OK | MB_ICONINFORMATION);
            return;
        }
        const std::string& base = trans.empty() ? raw : trans;
        size_t pos = 0;
        int i = 0;
        while (pos <= base.size()) {
            size_t nl = base.find('\n', pos);
            std::string line = nl == std::string::npos ? base.substr(pos)
                                                       : base.substr(pos, nl - pos);
            line = trim(line);
            if (!line.empty()) {
                Item it;
                it.start = i * 3.0;
                it.endd = (i + 1) * 3.0;
                it.text = line;
                items.push_back(it);
                ++i;
            }
            if (nl == std::string::npos) break;
            pos = nl + 1;
        }
        if (items.empty()) {
            MessageBoxW(hwnd_, L"还没有字幕内容", L"SRT 导出", MB_OK | MB_ICONINFORMATION);
            return;
        }
    }

    // 时间轴换算成相对会话开始，并保证相邻句不重叠
    double t0 = items.front().start;
    std::string content;
    for (size_t i = 0; i < items.size(); ++i) {
        double st = items[i].start - t0;
        if (st < 0) st = 0;
        double en = items[i].endd - t0;
        if (en < st + 0.6) en = st + 0.6;
        if (i + 1 < items.size()) {
            double nxt = items[i + 1].start - t0;
            if (nxt < 0) nxt = 0;
            if (nxt > st + 0.2 && en > nxt - 0.02) en = nxt - 0.02;
        }
        if (en <= st) en = st + 0.6;

        std::string text = items[i].text;
        for (char& c : text) {
            if (c == '\n' || c == '\r') c = ' ';
        }
        content += std::to_string(i + 1) + "\n";
        content += fmtSrtTime(st) + " --> " + fmtSrtTime(en) + "\n";
        content += text + "\n\n";
    }

    std::string dir = outputDir();
    std::wstring name = toWide("同传字幕_" + fileTimestamp() + ".srt");
    std::string path = askSavePath(hwnd_, L"SRT 字幕\0*.srt\0所有文件\0*.*\0", name,
                                   toWide(dir));
    if (path.empty()) return;

    if (writeTextFile(path, content)) {
        MessageBoxW(hwnd_, toWide("SRT 已导出：\n" + path).c_str(), L"SRT 已导出",
                    MB_OK | MB_ICONINFORMATION);
        setStatus("已导出 SRT：" + path, theme().green);
    } else {
        MessageBoxW(hwnd_, L"导出失败：没有写入权限", L"导出失败", MB_OK | MB_ICONWARNING);
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
                    toWide("这个版本还没有上传安装包，可以到发布页手动下载：\n" + target).c_str(),
                    L"暂时没有可下载的安装包", MB_OK | MB_ICONINFORMATION);
        ShellExecuteW(nullptr, L"open", toWide(target).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    startDownload(url, tag);
}

void MainWindow::startDownload(const std::string& url, const std::string& tag) {
    if (!updater_) return;
    setStatus("正在下载更新...", theme().yellow);
    updater_->downloadAsync(url, tag);
}

// ---------------------------------------------------------------- 独家模块
}  // namespace hs

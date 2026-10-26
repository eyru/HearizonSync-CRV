// Hearizon Sync C.R.V. - 托盘图标（Shell_NotifyIcon）
#pragma once

#include <windows.h>
#include <shellapi.h>

#include <functional>
#include <string>

namespace hs {

class TrayIcon {
public:
    using EventCallback = std::function<void(const std::string&)>;

    TrayIcon();
    ~TrayIcon();

    // 创建托盘图标；idle/live 是 .ico 路径
    bool start(HWND owner, const std::string& idleIcon, const std::string& liveIcon,
               const std::string& tip);
    void stop();
    void setState(bool running);

    void setOnEvent(EventCallback cb) { onEvent_ = std::move(cb); }

    // 由主窗口转发 WM_TRAYICON
    static constexpr UINT kTrayMessage = WM_APP + 100;
    void handleMessage(WPARAM wp, LPARAM lp);

private:
    void showMenu();

    NOTIFYICONDATAW nid_{};
    HWND owner_ = nullptr;
    bool running_ = false;
    HICON idleIcon_ = nullptr;
    HICON liveIcon_ = nullptr;
    bool ownsIcons_ = false;   // 图标是本次加载的还是资源缓存里的
    EventCallback onEvent_;
    bool added_ = false;
};

}  // namespace hs

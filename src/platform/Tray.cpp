#include "Tray.h"

#include <shellapi.h>

#include "core/Strings.h"
#include "core/Util.h"

namespace hs {

namespace {
constexpr UINT kMenuShow = 1;
constexpr UINT kMenuToggle = 2;
constexpr UINT kMenuQuit = 3;
const wchar_t* kTrayWndClass = L"HearizonSyncCRVTray";
}  // namespace

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon() { stop(); }

bool TrayIcon::start(HWND owner, const std::string& idleIcon, const std::string& liveIcon,
                     const std::string& tip) {
    owner_ = owner;
    idleIcon_ = static_cast<HICON>(
        LoadImageW(nullptr, toWide(idleIcon).c_str(), IMAGE_ICON, 0, 0,
                   LR_LOADFROMFILE | LR_DEFAULTSIZE));
    if (!idleIcon_) idleIcon_ = LoadIconW(nullptr, IDI_APPLICATION);
    liveIcon_ = static_cast<HICON>(
        LoadImageW(nullptr, toWide(liveIcon).c_str(), IMAGE_ICON, 0, 0,
                   LR_LOADFROMFILE | LR_DEFAULTSIZE));
    if (!liveIcon_) liveIcon_ = idleIcon_;

    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = owner;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = kTrayMessage;
    nid_.hIcon = running_ ? liveIcon_ : idleIcon_;
    std::wstring tipW = toWide(tip);
    wcsncpy_s(nid_.szTip, std::size(nid_.szTip), tipW.c_str(), _TRUNCATE);

    added_ = Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
    return added_;
}

void TrayIcon::stop() {
    if (added_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        added_ = false;
    }
    if (idleIcon_ && idleIcon_ != LoadIconW(nullptr, IDI_APPLICATION)) {
        DestroyIcon(idleIcon_);
    }
    idleIcon_ = nullptr;
    liveIcon_ = nullptr;
}

void TrayIcon::setState(bool running) {
    running_ = running;
    if (!added_) return;
    nid_.hIcon = running ? liveIcon_ : idleIcon_;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::handleMessage(WPARAM wp, LPARAM lp) {
    (void)wp;
    UINT evt = static_cast<UINT>(LOWORD(lp));
    if (evt == WM_LBUTTONDBLCLK) {
        if (onEvent_) onEvent_("show");
    } else if (evt == WM_RBUTTONUP || evt == WM_CONTEXTMENU) {
        showMenu();
    }
}

void TrayIcon::showMenu() {
    POINT pt;
    GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kMenuShow, toWide(str::kTrayShow).c_str());
    AppendMenuW(menu, MF_STRING, kMenuToggle, toWide(str::kTrayToggle).c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuQuit, toWide(str::kTrayQuit).c_str());

    SetForegroundWindow(owner_);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0,
                              owner_, nullptr);
    DestroyMenu(menu);

    if (cmd == kMenuShow) {
        if (onEvent_) onEvent_("show");
    } else if (cmd == kMenuToggle) {
        if (onEvent_) onEvent_("toggle");
    } else if (cmd == kMenuQuit) {
        if (onEvent_) onEvent_("quit");
    }
}

}  // namespace hs

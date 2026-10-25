#include "SingleInstance.h"

#include "core/Util.h"

namespace hs {

HANDLE SingleInstance::mutex_ = nullptr;

bool SingleInstance::acquire(const wchar_t* name) {
    mutex_ = CreateMutexW(nullptr, TRUE, name);
    if (!mutex_) return true;   // 创建失败时放行，避免无法启动
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex_);
        mutex_ = nullptr;
        return false;
    }
    return true;
}

void SingleInstance::release() {
    if (mutex_) {
        ReleaseMutex(mutex_);
        CloseHandle(mutex_);
        mutex_ = nullptr;
    }
}

void SingleInstance::activateExisting(const wchar_t* windowClass) {
    HWND hwnd = FindWindowW(windowClass, nullptr);
    if (!hwnd) return;
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
}

}  // namespace hs

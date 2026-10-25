// Hearizon Sync C.R.V. - 程序入口
#include <windows.h>
#include <objbase.h>
#include <commctrl.h>

#include "core/Theme.h"
#include "core/Util.h"
#include "platform/SingleInstance.h"
#include "ui/MainWindow.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int show) {
    (void)hPrev;
    (void)cmdLine;
    (void)show;

    // 单实例：已有窗口就激活它
    if (!hs::SingleInstance::acquire(L"HearizonSyncCRV")) {
        hs::SingleInstance::activateExisting(L"HearizonSyncCRVMain");
        return 0;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    (void)hr;

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    hs::Dpi::enableAwareness();

    hs::MainWindow win;
    if (!win.create(hInst)) {
        MessageBoxW(nullptr, L"窗口创建失败", L"Hearizon Sync", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    int code = win.run();

    hs::SingleInstance::release();
    CoUninitialize();
    return code;
}

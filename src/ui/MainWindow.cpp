#include "MainWindow.h"

#include <commctrl.h>
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
#include "ui/SettingsWindow.h"

namespace hs {

MainWindow* g_mainWindow = nullptr;

namespace {
const wchar_t* kWindowClass = L"HearizonSyncCRVMain";
const int kSidebarWidth = 252;
const int kSidebarPad = 16;
const int kStatusHeight = 30;
const int kPanelGap = 8;
// 窗口最小客户区：再小侧栏就摆不下了（逻辑像素，实际按 DPI 缩放）
const int kMinClientW = 620;
const int kMinClientH = 640;
}  // namespace

MainWindow::MainWindow() {
    g_mainWindow = this;
}

MainWindow::~MainWindow() {
    stopCapture();
    if (tray_) tray_->stop();
    g_mainWindow = nullptr;
}

bool MainWindow::create(HINSTANCE hInst) {
    hInst_ = hInst;
    Dpi::enableAwareness();

    loadConfig();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &MainWindow::WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kWindowClass;
    // 大小图标分开取：LoadIcon 给的是系统尺寸，高 DPI 下会糊，改用 LoadImage 指定边长
    wc.hIcon = appIcon(GetSystemMetrics(SM_CXICON), false);
    wc.hIconSm = appIcon(GetSystemMetrics(SM_CXSMICON), false);
    if (!wc.hIcon) wc.hIcon = LoadIconW(hInst, L"APPICON");
    if (!wc.hIconSm) wc.hIconSm = wc.hIcon;
    if (!RegisterClassExW(&wc)) return false;

    Config& c = config();
    const double sys = Dpi::systemScale();
    auto logical = [sys](int v) { return static_cast<int>(v * sys + 0.5); };

    // 恢复尺寸：只认「普通状态」的矩形，最大化/最小化时的尺寸不能当窗口大小用
    const bool noSavedSize = (c.windowW <= 0 || c.windowH <= 0);
    int w = (c.windowW > 0) ? c.windowW : logical(1180);
    int h = (c.windowH > 0) ? c.windowH : logical(840);
    bool wantMax = c.windowMaximized;

    // 旧配置里存的是物理像素，高 DPI 下可能小到放不下侧栏，这里兜底放大
    if (w < logical(kMinClientW)) w = logical(kMinClientW);
    if (h < logical(kMinClientH)) h = logical(kMinClientH);

    // 外框不能超出工作区，否则底部按钮会跑到屏幕外
    RECT wa{};
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0)) {
        RECT box{0, 0, w, h};
        AdjustWindowRectEx(&box, WS_OVERLAPPEDWINDOW, FALSE, 0);
        int outerH = box.bottom - box.top;
        int outerW = box.right - box.left;
        int maxH = (wa.bottom - wa.top) - logical(8);
        int maxW = (wa.right - wa.left) - logical(8);
        if (outerH > maxH) h -= (outerH - maxH);
        if (outerW > maxW) w -= (outerW - maxW);
    }

    // 位置：有记录就沿用，但要确保窗口标题栏还在屏幕内
    int x = (c.windowX >= 0) ? c.windowX : CW_USEDEFAULT;
    int y = (c.windowY >= 0) ? c.windowY : CW_USEDEFAULT;
    if (c.windowX >= 0 && wa.right > wa.left) {
        const int margin = logical(80);
        if (x < wa.left - margin || x > wa.right - margin) x = CW_USEDEFAULT;
        if (y < wa.top - margin || y > wa.bottom - margin) y = CW_USEDEFAULT;
    }

    std::wstring title = toWide(std::string(str::kAppTitle) + " v" + appVersion());
    hwnd_ = CreateWindowExW(0, kWindowClass, title.c_str(),
                            WS_OVERLAPPEDWINDOW, x, y, w, h, nullptr, nullptr, hInst,
                            nullptr);
    if (!hwnd_) return false;

    Dpi::update(hwnd_);

    // 首次运行没有保存过尺寸：按真实 DPI 重算一遍默认尺寸。
    // 建窗口时用的 systemScale 在部分环境下读到的是 96，会把界面压小一圈。
    if (noSavedSize) {
        const double ui = Dpi::uiScale();
        int dw = static_cast<int>(1180 * ui + 0.5);
        int dh = static_cast<int>(840 * ui + 0.5);
        HMONITOR mon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(mon, &mi)) {
            RECT box{0, 0, dw, dh};
            AdjustWindowRectEx(&box, WS_OVERLAPPEDWINDOW, FALSE, 0);
            int outerW = box.right - box.left;
            int outerH = box.bottom - box.top;
            int availW = (mi.rcWork.right - mi.rcWork.left) - static_cast<int>(8 * ui);
            int availH = (mi.rcWork.bottom - mi.rcWork.top) - static_cast<int>(8 * ui);
            if (outerW > availW) dw -= (outerW - availW);
            if (outerH > availH) dh -= (outerH - availH);
        }
        SetWindowPos(hwnd_, nullptr, 0, 0, dw, dh, SWP_NOZORDER | SWP_NOMOVE);
    }

    if (wantMax) ShowWindow(hwnd_, SW_MAXIMIZE);

    // 高 DPI 下按实际 DPI 重新取图标，任务栏和 Alt+Tab 才不会糊
    {
        const int big = GetSystemMetrics(SM_CXICON);
        const int small = GetSystemMetrics(SM_CXSMICON);
        HICON hi = appIcon(big, false);
        HICON lo = appIcon(small, false);
        if (hi) SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hi));
        if (lo) SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(lo));
    }

    // 业务对象
    recognizer_ = std::make_unique<Recognizer>();
    recognizer_->setStatusCallback([this](const std::string& s) { setStatusThreadSafe(s); });
    translator_ = std::make_unique<Translator>(
        [this](const std::string& s) { setStatusThreadSafe(s); });
    capture_ = std::make_unique<AudioCapture>();
    capture_->vad().setCallbacks(
        [this](const std::vector<float>& a) { onSentence(a); },
        [this](double rms) { onLevel(rms); },
        [this](const std::vector<float>& a) { onPartial(a); });

    initWidgets();
    layout();
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    SetTimer(hwnd_, kTimerPump, 60, nullptr);
    lastSpeechTick_ = GetTickCount();

    // 启动后 4 秒检查更新
    SetTimer(hwnd_, kTimerUpdate, 4000, nullptr);

    // 首次运行：半秒后弹一次引导（窗口先画出来，不让用户看着空白）
    {
        std::string flag = joinPath(userHome(), ".hearizon_sync_seen");
        if (!fileExists(flag)) {
            writeTextFile(flag, "seen");
            SetTimer(hwnd_, kTimerWelcome, 500, nullptr);
        }
    }

    return true;
}

int MainWindow::run() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<MainWindow*>(cs->lpCreateParams);
        if (!self && g_mainWindow) self = g_mainWindow;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(self ? self : g_mainWindow));
        if (self) self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self) self = g_mainWindow;
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
    if (msg == WM_GETMINMAXINFO) Dpi::update(hwnd);
    return self->handleMessage(msg, wp, lp);
}

LRESULT MainWindow::handleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_GETMINMAXINFO: {
            // 窗口再小侧栏就会挤在一起，这里划死下限
            auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            const UiScale scale{Dpi::uiScale()};
            RECT box{0, 0, kMinClientW * scale, kMinClientH * scale};
            AdjustWindowRectEx(&box, WS_OVERLAPPEDWINDOW, FALSE, 0);
            mmi->ptMinTrackSize.x = box.right - box.left;
            mmi->ptMinTrackSize.y = box.bottom - box.top;
            return 0;
        }

        case WM_CREATE:
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd_, &ps);
            // 双缓冲
            HDC mem = CreateCompatibleDC(dc);
            RECT rc;
            GetClientRect(hwnd_, &rc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(mem, bmp));
            paint(mem);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, oldBmp);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hwnd_, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_SIZE:
            layout();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            onMouseMove(x, y);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            onMouseDown(x, y);
            return 0;
        }

        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            onMouseUp(x, y);
            return 0;
        }

        case WM_MOUSEWHEEL: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd_, &pt);
            onMouseWheel(GET_WHEEL_DELTA_WPARAM(wp), pt.x, pt.y);
            return 0;
        }

        case WM_KEYDOWN:
            onKeyDown(wp);
            return 0;

        case WM_TIMER:
            onTimer(wp);
            return 0;

        case WM_CLOSE:
            DestroyWindow(hwnd_);
            return 0;

        case WM_DESTROY: {
            // 用 WINDOWPLACEMENT 保存：最大化时记的是还原后的尺寸，
            // 否则下次会把「最大化尺寸」当普通尺寸恢复，越开越大
            Config& c = config();
            WINDOWPLACEMENT wp{};
            wp.length = sizeof(wp);
            if (GetWindowPlacement(hwnd_, &wp)) {
                c.windowMaximized = (wp.showCmd == SW_SHOWMAXIMIZED);
                const RECT& r = wp.rcNormalPosition;
                c.windowX = r.left;
                c.windowY = r.top;
                c.windowW = r.right - r.left;
                c.windowH = r.bottom - r.top;
                saveConfig();
            }
            KillTimer(hwnd_, kTimerPump);
            KillTimer(hwnd_, kTimerUpdate);
            KillTimer(hwnd_, kTimerSilence);
            PostQuitMessage(0);
            return 0;
        }

        case WM_APP + 2:   // GPU 探测完成
            onGpuProbeDone();
            return 0;

        case WM_APP + 1:   // 来自其他线程的刷新请求
            pumpUiMessages();
            InvalidateRect(hwnd_, nullptr, FALSE);
            return 0;

        case TrayIcon::kTrayMessage:   // 托盘图标事件
            if (tray_) tray_->handleMessage(wp, lp);
            return 0;

        case WM_CTLCOLORSTATIC:
            return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));

        default:
            break;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

// ---------------------------------------------------------------- 控件初始化
void MainWindow::initWidgets() {
    Config& c = config();
    const UiScale scale{Dpi::uiScale()};

    HFONT ui9 = uiFont(Dpi::fontPt(9));
    HFONT ui8 = uiFont(Dpi::fontPt(8));
    HFONT ui10 = uiFont(Dpi::fontPt(10), true);

    // 输入语言
    inLangBox_.setFont(ui9);
    inLangBox_.setItems(INPUT_LANG_NAMES);
    inLangBox_.setOnChange([this](int i) { onInputLangChanged(i); });

    // 目标语言
    targetLangBox_.setFont(ui9);
    targetLangBox_.setItems(OUTPUT_LANG_NAMES);
    targetLangBox_.setSelected(0);
    targetLangBox_.setOnChange([this](int i) { onTargetLangChanged(i); });

    // 翻译模式
    modeBox_.setFont(ui9);
    modeBox_.setItems({TR_LOCAL, TR_FREE, TR_API});
    {
        int idx = 1;
        if (c.translatorMode == TR_LOCAL) idx = 0;
        else if (c.translatorMode == TR_API) idx = 2;
        modeBox_.setSelected(idx);
    }
    modeBox_.setOnChange([this](int i) { onModeChanged(i); });

    // API 引擎
    apiEngineBox_.setFont(ui9);
    apiEngineBox_.setItems({API_ENGINE_DEEPSEEK, API_ENGINE_YOUDAO, API_ENGINE_AUTO});
    {
        int idx = 2;
        if (c.apiEngine == API_ENGINE_DEEPSEEK) idx = 0;
        else if (c.apiEngine == API_ENGINE_YOUDAO) idx = 1;
        apiEngineBox_.setSelected(idx);
    }
    apiEngineBox_.setOnChange([this](int i) { onApiEngineChanged(i); });

    // 识别引擎（显示用）
    engineBox_.setFont(ui9);
    engineBox_.setItems({"sensevoice", "small", "base"});
    engineBox_.setSelected(0);
    engineBox_.setOnChange([this](int i) { onEngineChanged(i); });

    // GPU / 纠错
    gpuCheck_.setText(str::kGpuAccel);
    gpuCheck_.setChecked(c.gpuAccel);
    gpuCheck_.setOnChange([this](bool on) { onGpuToggled(on); });

    correctCheck_.setText(str::kAutoCorrect);
    correctCheck_.setChecked(c.autoCorrect);
    correctCheck_.setOnChange([this](bool on) { onCorrectToggled(on); });

    // 按钮
    btnStart_.setText(str::kStart);
    btnStart_.setColors(theme().accent, theme().accent2, RGB(0x03, 0x21, 0x1A));
    btnStart_.setFont(ui10);
    btnStart_.setRadius(10 * scale);
    btnStart_.setOnClick([this]() {
        if (running_.load())
            stopCapture();
        else
            startCapture();
    });

    struct BtnDef {
        Button* b;
        const char* text;
    };
    BtnDef defs[] = {
        {&btnTestMic_, str::kTestMic},     {&btnSave_, str::kSaveRecord},
        {&btnExport_, str::kExportSrt},    {&btnClear_, str::kClearSubtitle},
        {&btnSettings_, str::kSettings},   {&btnTray_, str::kMinimizeToTray},
        {&btnAbout_, str::kAbout},         {&btnRefresh_, str::kRefreshDevice},
        {&btnPosBottom_, str::kPositionBottom},
        {&btnPosTop_, str::kPositionTop},
    };
    for (auto& d : defs) {
        d.b->setText(d.text);
        d.b->setFont(ui8);
        d.b->setColors(theme().card, theme().cardHover, theme().text2);
        d.b->setRadius(7 * scale);
        d.b->setAnchorLeft(true);
    }

    // 品牌区设置齿轮（圆角小方块，只放一个图标字符）
    // 矢量绘制齿轮：字体里的 U+2699 在小尺寸下糊成一团
    btnGear_.setText("");
    btnGear_.setDrawGear(true);
    btnGear_.setColors(theme().sidebar, theme().cardHover, theme().dim);
    btnGear_.setRadius(8 * scale);
    btnGear_.setAnchorLeft(false);
    btnGear_.setOnClick([this]() { openSettings(); });
    // 动作按钮两列排布，文字居中更整齐
    for (Button* b : {&btnTestMic_, &btnSave_, &btnExport_, &btnClear_, &btnSettings_,
                      &btnTray_}) {
        b->setAnchorLeft(false);
    }
    btnAbout_.setColors(theme().sidebar, theme().card, theme().dim);
    btnAbout_.setAnchorLeft(false);
    btnRefresh_.setAnchorLeft(false);

    btnTestMic_.setOnClick([this]() { testMic(); });
    btnSave_.setOnClick([this]() { saveRecord(); });
    btnExport_.setOnClick([this]() { exportSrt(); });
    btnClear_.setOnClick([this]() { clearSubtitles(); });
    btnTray_.setOnClick([this]() { minimizeToTray(); });
    btnAbout_.setOnClick([this]() { openAbout(); });
    btnRefresh_.setOnClick([this]() { refreshDevices(); });
    btnSettings_.setOnClick([this]() { openSettings(); });
    btnPosBottom_.setOnClick([this]() { onPositionChanged(false); });
    btnPosTop_.setOnClick([this]() { onPositionChanged(true); });
    updatePosButtons();

    // 配色
    themeBox_.setFont(ui9);
    std::vector<std::string> themeNames;
    for (int i = 0; i < subThemeCount(); ++i) themeNames.push_back(subThemeByIndex(i).name);
    themeBox_.setItems(themeNames);
    themeIndex_ = subThemeIndex(c.subTheme);
    themeBox_.setSelected(themeIndex_);
    themeBox_.setOnChange([this](int i) { onThemeChanged(i); });

    // 字幕视图
    const SubTheme& st = subThemeByIndex(themeIndex_);
    srcView_.setFont(uiFont(Dpi::fontPt(subSizeSrc_)));
    srcView_.setColors(st.srcFg, st.bg);
    srcView_.setLineHeight(Dpi::sp(24));
    dstView_.setFont(uiFont(Dpi::fontPt(subSizeDst_)));
    dstView_.setColors(st.dstFg, st.bg);
    dstView_.setLineHeight(Dpi::sp(28));

    subTop_ = (c.subPos == "top");

    // 设备
    refreshDevices();

    // 应用模式语言限制
    applyModeLanguages();

    // 托盘
    tray_ = std::make_unique<TrayIcon>();
    {
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
        if (!tray_->start(hwnd_, idle, live, tip)) {
            // 与 Python 版一致：「托盘不可用：{原因}」
            setStatus(std::string(str::kTrayUnavailable) + "系统通知区域不可用", theme().yellow);
        }
    }
}

// ---------------------------------------------------------------- 布局
void MainWindow::layout() {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    const UiScale scale{Dpi::uiScale()};

    int sidebarW = kSidebarWidth * scale;
    rcSidebar_ = {0, 0, sidebarW, rc.bottom};
    rcStatus_ = {0, rc.bottom - kStatusHeight * scale, rc.right, rc.bottom};

    // 品牌区设置齿轮：贴侧栏右上角
    {
        const int gs = 26 * scale;
        const int gx = sidebarW - kSidebarPad * scale - gs;
        const int gy = 18 * scale;
        btnGear_.setRect({gx, gy, gx + gs, gy + gs});
    }

    int contentLeft = sidebarW;
    int contentW = rc.right - contentLeft;
    int panelTop = kPanelGap * scale;
    int panelH = (rcStatus_.top - panelTop * 2) / 2;

    rcSrcPanel_ = {contentLeft + kPanelGap * scale, panelTop,
                   rc.right - kPanelGap * scale, panelTop + panelH};
    rcDstPanel_ = {contentLeft + kPanelGap * scale, rcSrcPanel_.bottom + kPanelGap * scale,
                   rc.right - kPanelGap * scale, rcSrcPanel_.bottom + kPanelGap * scale + panelH};

    // 字幕视图内边距
    int pad = 12 * scale;
    int headH = 26 * scale;
    srcView_.setRect({rcSrcPanel_.left + pad, rcSrcPanel_.top + headH,
                      rcSrcPanel_.right - pad - 10 * scale, rcSrcPanel_.bottom - pad});
    dstView_.setRect({rcDstPanel_.left + pad, rcDstPanel_.top + headH,
                      rcDstPanel_.right - pad - 10 * scale, rcDstPanel_.bottom - pad});
    srcScroll_.setRect({rcSrcPanel_.right - pad - 8 * scale, rcSrcPanel_.top + headH,
                        rcSrcPanel_.right - pad, rcSrcPanel_.bottom - pad});
    dstScroll_.setRect({rcDstPanel_.right - pad - 8 * scale, rcDstPanel_.top + headH,
                        rcDstPanel_.right - pad, rcDstPanel_.bottom - pad});

    // 侧栏：先按自然尺寸算高度；放不下就逐档压缩，压到极限仍放不下则改为滚动
    int left = kSidebarPad * scale;
    int w = sidebarW - left * 2;
    sideTop_ = 112 * scale;

    struct Metrics {
        int labelH, labelGap, comboH, itemGap, gpuRow, btnH, btnGap, posRow, themeH;
    };
    auto metricsFor = [](double k) {
        auto q = [k](int v, int lo) {
            int r = static_cast<int>(v * k + 0.5);
            return r < lo ? lo : r;
        };
        Metrics m;
        m.labelH = q(16, 12);
        m.labelGap = q(4, 3);
        m.comboH = q(30, 22);
        m.itemGap = q(7, 3);
        m.gpuRow = q(22, 16);
        m.btnH = q(27, 20);
        m.btnGap = q(6, 5);
        m.posRow = q(24, 18);
        m.themeH = q(28, 22);
        return m;
    };

    int startH = 44 * scale;
    int startTop = rcStatus_.top - startH - 12 * scale;
    int bottomLimit = startTop - 8 * scale;
    sideBottom_ = bottomLimit;

    // 摆一遍（apply=false 只量高度），off 为滚动偏移，返回内容结束位置
    auto runSidebar = [&](const Metrics& m, bool apply, int off) {
        int y = sideTop_ + off;
        auto comboAt = [&](ComboBox& b) {
            int t = y + m.labelH * scale + m.labelGap * scale;
            if (apply) b.setRect({left, t, left + w, t + m.comboH * scale});
            y = t + m.comboH * scale + m.itemGap * scale;
        };
        comboAt(inLangBox_);
        comboAt(deviceBox_);
        comboAt(targetLangBox_);
        comboAt(modeBox_);
        comboAt(apiEngineBox_);
        comboAt(engineBox_);

        y += 2 * scale;
        if (apply) {
            gpuCheck_.setRect({left, y, left + 110 * scale, y + m.gpuRow * scale});
            btnRefresh_.setRect({left + w - 66 * scale, y, left + w, y + m.gpuRow * scale});
        }
        y += m.gpuRow * scale + m.itemGap * scale;

        if (apply) correctCheck_.setRect({left, y, left + w, y + m.gpuRow * scale});
        y += m.gpuRow * scale + m.itemGap * scale + 6 * scale;

        // 动作按钮：两列三行，比单列省一半高度
        {
            const int colGap = 6, gap = 0;
            int colGapPx = colGap * scale;
            int colW = (w - colGapPx) / 2;
            Button* cells[6] = {&btnTestMic_, &btnSave_, &btnExport_, &btnClear_,
                                &btnSettings_, &btnTray_};
            for (int i = 0; i < 6; ++i) {
                int col = i % 2, row = i / 2;
                int bx = left + col * (colW + colGapPx);
                int by = y + row * (m.btnH + m.btnGap) * scale;
                if (apply) {
                    cells[i]->setRect({bx, by, bx + colW, by + m.btnH * scale});
                }
            }
            (void)gap;
            y += 3 * (m.btnH + m.btnGap) * scale;
        }
        // 分隔线在「字幕位置」标签之上，且留足标签高度，避免压字
        y += 5 * scale;
        if (apply) dividerY_ = y;
        y += 10 * scale;
        y += (m.labelH + m.labelGap) * scale;

        int halfW = (w - 6 * scale) / 2;
        if (apply) {
            btnPosBottom_.setRect({left, y, left + halfW, y + m.posRow * scale});
            btnPosTop_.setRect({left + halfW + 6 * scale, y, left + w, y + m.posRow * scale});
        }
        y += m.posRow * scale + (m.labelH + m.labelGap * 2) * scale;

        if (apply) themeBox_.setRect({left, y, left + w, y + m.themeH * scale});
        y += m.themeH * scale;
        return y;
    };

    Metrics m = metricsFor(1.0);
    int end = runSidebar(m, false, 0);
    for (double k = 0.95; end > bottomLimit && k >= 0.75 - 1e-9; k -= 0.05) {
        m = metricsFor(k);
        end = runSidebar(m, false, 0);
    }
    labelH_ = m.labelH * scale;
    labelGap_ = m.labelGap * scale;

    // 压到极限仍放不下：改成滚动，不重叠
    int avail = bottomLimit - sideTop_;
    int full = end - sideTop_;
    sideMax_ = full > avail ? full - avail : 0;
    bool changed = false;
    if (sideScroll_ > sideMax_) { sideScroll_ = sideMax_; changed = true; }
    if (sideScroll_ < 0) { sideScroll_ = 0; changed = true; }
    (void)changed;
    runSidebar(m, true, -sideScroll_);

    // 开始按钮（底部固定）
    btnStart_.setRect({left - 4 * scale, startTop, left + w + 4 * scale,
                       rcStatus_.top - 12 * scale});

    // 关于按钮（状态栏右侧）
    btnAbout_.setRect({rc.right - 70 * scale, rcStatus_.top + 4 * scale,
                       rc.right - 8 * scale, rcStatus_.bottom - 4 * scale});
    // 版本号（连点彩蛋）
    rcVersion_ = {rc.right - 70 * scale - 96 * scale, rcStatus_.top,
                  rc.right - 70 * scale - 4 * scale, rcStatus_.bottom};
}

// ---------------------------------------------------------------- 绘制
void MainWindow::paint(HDC dc) {
    RECT rc;
    GetClientRect(hwnd_, &rc);

    // 背景
    HBRUSH bg = CreateSolidBrush(theme().bg);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);

    paintSidebar(dc);
    paintPanels(dc);
    paintStatusBar(dc);

    // 弹层最后画
    if (openPopup_) openPopup_->paintPopup(dc);
}

void MainWindow::paintSidebar(HDC dc) {
    const UiScale scale{Dpi::uiScale()};

    HBRUSH sb = CreateSolidBrush(theme().sidebar);
    FillRect(dc, &rcSidebar_, sb);
    DeleteObject(sb);

    int left = kSidebarPad * scale;
    int y = 16 * scale;

    // 品牌区：logo + 标题 + 副标题（与 Python 版一致）
    {
        const int logoPx = 32 * scale;
        HICON logo = appIcon(logoPx, false);
        if (logo) {
            DrawIconEx(dc, left, y + 2 * scale, logo, logoPx, logoPx, 0, nullptr,
                       DI_NORMAL);
        } else {
            // 资源缺失时退化成圆点，不塌成空白
            HBRUSH fb = CreateSolidBrush(theme().accent);
            HPEN fp = CreatePen(PS_SOLID, 1, theme().accent);
            HBRUSH ob = static_cast<HBRUSH>(SelectObject(dc, fb));
            HPEN op = static_cast<HPEN>(SelectObject(dc, fp));
            Ellipse(dc, left + 2 * scale, y + 4 * scale, left + logoPx - 2 * scale,
                    y + logoPx - 2 * scale);
            SelectObject(dc, ob);
            SelectObject(dc, op);
            DeleteObject(fb);
            DeleteObject(fp);
        }

        const int textX = left + logoPx + 9 * scale;
        RECT nameRc{textX, y, rcSidebar_.right - left, y + 20 * scale};
        drawText(dc, toWide(str::kAppTitle), nameRc, theme().text,
                 uiFont(Dpi::fontPt(11), true), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT tagRc{textX, y + 18 * scale, rcSidebar_.right - left, y + 34 * scale};
        drawText(dc, toWide(str::kSidebarTagline), tagRc, theme().faint,
                 uiFont(Dpi::fontPt(8)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // 设置齿轮：品牌区右上角，与 Python 版一致（这里能直接进填 API Key 的设置页）
    btnGear_.paint(dc, rcSidebar_);
    y += 44 * scale;

    // 状态点 + 文字
    RECT dotRc{left, y + 6 * scale, left + 7 * scale, y + 13 * scale};
    HBRUSH dot = CreateSolidBrush(statusColor_);
    HPEN dotPen = CreatePen(PS_SOLID, 1, statusColor_);
    HBRUSH oldB = static_cast<HBRUSH>(SelectObject(dc, dot));
    HPEN oldP = static_cast<HPEN>(SelectObject(dc, dotPen));
    Ellipse(dc, dotRc.left, dotRc.top, dotRc.right, dotRc.bottom);
    SelectObject(dc, oldB);
    SelectObject(dc, oldP);
    DeleteObject(dot);
    DeleteObject(dotPen);

    RECT statusRc{left + 12 * scale, y, rcSidebar_.right - left, y + 20 * scale};
    drawText(dc, toWide(statusText_), statusRc, theme().dim,
             uiFont(Dpi::fontPt(9)), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    y += 26 * scale;

    // 音量条
    RECT levelRc{left, y, rcSidebar_.right - left, y + 6 * scale};
    HBRUSH track = CreateSolidBrush(theme().border);
    FillRect(dc, &levelRc, track);
    DeleteObject(track);
    // 由 onLevel 更新（这里画固定最小宽度）
    y += 16 * scale;

    // 分隔线
    HPEN line = CreatePen(PS_SOLID, 1, theme().border);
    HPEN oldLine = static_cast<HPEN>(SelectObject(dc, line));
    MoveToEx(dc, left, y, nullptr);
    LineTo(dc, rcSidebar_.right - left, y);
    SelectObject(dc, oldLine);
    DeleteObject(line);

    // 可滚动区域：标题与状态不随滚动移动，避免与内容重叠
    int savedDc = SaveDC(dc);
    IntersectClipRect(dc, 0, sideTop_, rcSidebar_.right, rcStatus_.top);

    // 各字段标签（位置从控件矩形推导，改布局也不会错位）
    auto labelAbove = [&](const char* text, const RECT& box) {
        RECT lr{box.left, box.top - labelGap_ - labelH_, rcSidebar_.right - box.left,
                box.top - labelGap_};
        drawText(dc, toWide(text), lr, theme().dim, uiFont(Dpi::fontPt(8)),
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    };
    labelAbove(str::kInputLang, inLangBox_.rect());
    labelAbove(str::kAudioDevice, deviceBox_.rect());
    labelAbove(str::kTargetLang, targetLangBox_.rect());
    labelAbove(str::kTranslateMode, modeBox_.rect());
    labelAbove(str::kApiEngine, apiEngineBox_.rect());
    labelAbove(str::kAsrEngine, engineBox_.rect());

    // 控件
    inLangBox_.paint(dc);
    deviceBox_.paint(dc);
    targetLangBox_.paint(dc);
    modeBox_.paint(dc);
    apiEngineBox_.paint(dc);
    engineBox_.paint(dc);
    gpuCheck_.paint(dc);
    correctCheck_.paint(dc);

    for (Button* b : {&btnRefresh_, &btnTestMic_, &btnSave_, &btnExport_, &btnClear_,
                      &btnSettings_, &btnTray_, &btnPosBottom_, &btnPosTop_}) {
        b->paint(dc, rcSidebar_);
    }

    // 字幕分区：分隔线 + 标签，标签位置全部由控件矩形推导
    if (dividerY_ > 0) {
        HPEN dl = CreatePen(PS_SOLID, 1, theme().border);
        HPEN oldDl = static_cast<HPEN>(SelectObject(dc, dl));
        MoveToEx(dc, left, dividerY_, nullptr);
        LineTo(dc, rcSidebar_.right - left, dividerY_);
        SelectObject(dc, oldDl);
        DeleteObject(dl);
    }
    labelAbove(str::kPosition, btnPosBottom_.rect());
    labelAbove(str::kTheme, themeBox_.rect());
    themeBox_.paint(dc);

    RestoreDC(dc, savedDc);

    // 侧栏滚动条（仅内容超高时出现）
    if (sideMax_ > 0) {
        int sw = 4 * scale;
        if (sw < 3) sw = 3;
        int sx = rcSidebar_.right - sw - 2 * scale;
        int bandH = sideBottom_ - sideTop_;
        HBRUSH track = CreateSolidBrush(theme().border);
        RECT tr{sx, sideTop_, sx + sw, sideBottom_};
        FillRect(dc, &tr, track);
        DeleteObject(track);

        int full = bandH + sideMax_;
        int thumbH = bandH * bandH / (full > 0 ? full : 1);
        if (thumbH < 24 * scale) thumbH = 24 * scale;
        int travel = bandH - thumbH;
        int ty = sideTop_;
        if (sideMax_ > 0) ty += travel * sideScroll_ / sideMax_;
        HBRUSH thumb = CreateSolidBrush(theme().border2);
        RECT hr{sx, ty, sx + sw, ty + thumbH};
        FillRect(dc, &hr, thumb);
        DeleteObject(thumb);
    }

    // 开始按钮
    btnStart_.paint(dc, rcSidebar_);
}

void MainWindow::paintPanels(HDC dc) {
    const UiScale scale{Dpi::uiScale()};
    const SubTheme& st = subThemeByIndex(themeIndex_);

    auto paintPanel = [&](const RECT& rc, const char* title, COLORREF dotColor,
                          TextView& view, ScrollBar& bar, const char* placeholder) {
        HBRUSH bg = CreateSolidBrush(theme().panel);
        HPEN pen = CreatePen(PS_SOLID, 1, theme().border);
        HBRUSH oldB = static_cast<HBRUSH>(SelectObject(dc, bg));
        HPEN oldP = static_cast<HPEN>(SelectObject(dc, pen));
        roundRect(dc, rc, 10 * scale);
        SelectObject(dc, oldB);
        SelectObject(dc, oldP);
        DeleteObject(bg);
        DeleteObject(pen);

        // 标题
        int pad = 12 * scale;
        RECT dotRc{rc.left + pad, rc.top + 9 * scale, rc.left + pad + 7 * scale,
                   rc.top + 16 * scale};
        HBRUSH db = CreateSolidBrush(dotColor);
        HBRUSH ob = static_cast<HBRUSH>(SelectObject(dc, db));
        HPEN dp = CreatePen(PS_SOLID, 1, dotColor);
        HPEN op = static_cast<HPEN>(SelectObject(dc, dp));
        Ellipse(dc, dotRc.left, dotRc.top, dotRc.right, dotRc.bottom);
        SelectObject(dc, ob);
        SelectObject(dc, op);
        DeleteObject(db);
        DeleteObject(dp);

        RECT titleRc{rc.left + pad + 12 * scale, rc.top + 2 * scale,
                     rc.right - pad, rc.top + 22 * scale};
        drawText(dc, toWide(title), titleRc, theme().dim, uiFont(Dpi::fontPt(9)),
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // 空状态：没有内容时给一句引导，而不是一片空白
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
               str::kDstPlaceholder);
}

void MainWindow::paintStatusBar(HDC dc) {
    const UiScale scale{Dpi::uiScale()};

    HBRUSH sb = CreateSolidBrush(theme().sidebar);
    FillRect(dc, &rcStatus_, sb);
    DeleteObject(sb);

    HPEN line = CreatePen(PS_SOLID, 1, theme().border);
    HPEN oldLine = static_cast<HPEN>(SelectObject(dc, line));
    MoveToEx(dc, rcStatus_.left, rcStatus_.top, nullptr);
    LineTo(dc, rcStatus_.right, rcStatus_.top);
    SelectObject(dc, oldLine);
    DeleteObject(line);

    // 统计
    char buf[256];
    long long dur = startTick_ ? (GetTickCount() - startTick_) / 1000 : 0;
    std::snprintf(buf, sizeof(buf), "句数 %lld   字数 %lld   时长 %02lld:%02lld",
                  sentences_, charsIn_, dur / 60, dur % 60);
    RECT statRc{rcStatus_.left + 14 * scale, rcStatus_.top, rcVersion_.left - 6 * scale,
                rcStatus_.bottom};
    drawText(dc, toWide(buf), statRc, theme().faint, uiFont(Dpi::fontPt(8)),
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    drawText(dc, toWide(std::string("v") + appVersion()), rcVersion_, theme().faint,
             uiFont(Dpi::fontPt(8)), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    btnAbout_.paint(dc, rcStatus_);
}

// ---------------------------------------------------------------- 交互
void MainWindow::onMouseMove(int x, int y) {
    if (dragging_) {
        POINT pt{x, y};
        ClientToScreen(hwnd_, &pt);
        SetWindowPos(hwnd_, nullptr, dragWindowRect_.left + (pt.x - dragStart_.x),
                     dragWindowRect_.top + (pt.y - dragStart_.y), 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER);
        return;
    }

    inLangBox_.onMouseMove(x, y);
    deviceBox_.onMouseMove(x, y);
    targetLangBox_.onMouseMove(x, y);
    modeBox_.onMouseMove(x, y);
    apiEngineBox_.onMouseMove(x, y);
    engineBox_.onMouseMove(x, y);
    themeBox_.onMouseMove(x, y);
    gpuCheck_.onMouseMove(x, y);
    correctCheck_.onMouseMove(x, y);
    for (Button* b : {&btnStart_, &btnGear_, &btnTestMic_, &btnSave_, &btnExport_,
                      &btnClear_, &btnSettings_, &btnTray_, &btnAbout_, &btnRefresh_,
                      &btnPosBottom_, &btnPosTop_}) {
        b->onMouseMove(x, y);
    }
    srcScroll_.onMouseMove(x, y);
    dstScroll_.onMouseMove(x, y);
    if (srcScroll_.dragging() || dstScroll_.dragging()) {
        srcView_.setScroll(srcScroll_.position());
        dstView_.setScroll(dstScroll_.position());
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::onMouseDown(int x, int y) {
    // 弹层优先
    if (openPopup_) {
        if (openPopup_->popupHitTest(x, y)) {
            openPopup_ = nullptr;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
        openPopup_ = nullptr;
    }

    if (srcScroll_.onMouseDown(x, y) || dstScroll_.onMouseDown(x, y)) {
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

    // 品牌区按钮的命中优先级高于标题栏拖动，否则齿轮/关于被拖动区吞掉
    if (btnGear_.onMouseDown(x, y)) return;
    if (btnAbout_.onMouseDown(x, y)) return;
    if (btnStart_.onMouseDown(x, y)) return;

    // 标题栏拖动（顶部 34 逻辑像素空白区）
    if (y < 34 * Dpi::uiScale()) {
        dragging_ = true;
        dragStart_ = {x, y};
        RECT wr;
        GetWindowRect(hwnd_, &wr);
        dragWindowRect_ = wr;
        return;
    }


    // 侧栏控件只在其可视带内响应（滚动后超出的部分不可点）
    if (y < sideTop_ || y >= sideBottom_) return;

    gpuCheck_.onMouseDown(x, y);
    correctCheck_.onMouseDown(x, y);
    for (Button* b : {&btnTestMic_, &btnSave_, &btnExport_, &btnClear_,
                      &btnSettings_, &btnTray_, &btnRefresh_,
                      &btnPosBottom_, &btnPosTop_}) {
        b->onMouseDown(x, y);
    }
}

void MainWindow::onMouseUp(int x, int y) {
    if (dragging_) {
        dragging_ = false;
        return;
    }

    // 注意：Button::onMouseUp 命中时会自行执行 onClick_ 回调，
    // 这里只做分发与提前返回，不能再重复调用业务函数。
    if (btnStart_.onMouseUp(x, y)) return;
    if (btnAbout_.onMouseUp(x, y)) return;
    if (btnGear_.onMouseUp(x, y)) return;
    if (y < sideTop_ || y >= sideBottom_) return;
    if (btnTestMic_.onMouseUp(x, y)) return;
    if (btnSave_.onMouseUp(x, y)) return;
    if (btnExport_.onMouseUp(x, y)) return;
    if (btnClear_.onMouseUp(x, y)) return;
    if (btnTray_.onMouseUp(x, y)) return;
    if (btnRefresh_.onMouseUp(x, y)) return;
    if (btnSettings_.onMouseUp(x, y)) return;
    if (btnPosBottom_.onMouseUp(x, y)) return;
    if (btnPosTop_.onMouseUp(x, y)) return;

    if (gpuCheck_.onMouseUp(x, y)) return;
    if (correctCheck_.onMouseUp(x, y)) return;

    // 下拉
    auto tryBox = [&](ComboBox& box) -> bool {
        if (box.onMouseUp(x, y)) {
            openPopup_ = &box;
            InvalidateRect(hwnd_, nullptr, FALSE);
            return true;
        }
        return false;
    };
    if (tryBox(inLangBox_)) return;
    if (tryBox(deviceBox_)) return;
    if (tryBox(targetLangBox_)) return;
    if (tryBox(modeBox_)) return;
    if (tryBox(apiEngineBox_)) return;
    if (tryBox(engineBox_)) return;
    if (tryBox(themeBox_)) return;
}

void MainWindow::onMouseWheel(int delta, int x, int y) {
    int step = delta > 0 ? -3 : 3;
    if (x < rcSidebar_.right && y >= sideTop_ && y < rcStatus_.top && sideMax_ > 0) {
        sideScroll_ += step * 10;
        if (sideScroll_ > sideMax_) sideScroll_ = sideMax_;
        if (sideScroll_ < 0) sideScroll_ = 0;
        layout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    if (x >= rcSrcPanel_.left && x < rcSrcPanel_.right && y >= rcSrcPanel_.top &&
        y < rcSrcPanel_.bottom) {
        srcScroll_.setPosition(srcScroll_.position() + step * 22);
        srcView_.setScroll(srcScroll_.position());
    } else {
        dstScroll_.setPosition(dstScroll_.position() + step * 22);
        dstView_.setScroll(dstScroll_.position());
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

// 字号热调：译文 +-1，原文跟着减 4，范围 14..32（与 Python 版一致）
void MainWindow::applyFontStep(int delta) {
    int dst = subSizeDst_ + delta;
    if (dst < 14) dst = 14;
    if (dst > 32) dst = 32;
    int src = dst - 4;
    if (src < 10) src = 10;
    if (src > 32) src = 32;
    subSizeDst_ = dst;
    subSizeSrc_ = src;
    config().subSizeDst = dst;
    config().subSizeSrc = src;
    saveConfig();
    layout();
    InvalidateRect(hwnd_, nullptr, FALSE);
    setStatus("字幕字号：译文 " + std::to_string(dst) + " / 原文 " + std::to_string(src),
              theme().text2);
}

void MainWindow::onKeyDown(WPARAM key) {    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (ctrl && key == VK_RETURN) {
        if (running_.load())
            stopCapture();
        else
            startCapture();
    } else if (ctrl && key == 'L') {
        clearSubtitles();
    } else if (ctrl && key == 'S') {
        if (shift)
            exportSrt();
        else
            saveRecord();
    } else if (ctrl && (key == VK_ADD || key == VK_OEM_PLUS)) {
        applyFontStep(1);
    } else if (ctrl && (key == VK_SUBTRACT || key == VK_OEM_MINUS)) {
        applyFontStep(-1);
    } else if (ctrl && key == '0') {
        subSizeDst_ = 18;
        subSizeSrc_ = 14;
        config().subSizeDst = subSizeDst_;
        config().subSizeSrc = subSizeSrc_;
        saveConfig();
        layout();
        InvalidateRect(hwnd_, nullptr, FALSE);
        setStatus("字幕字号已恢复默认", theme().text2);
    } else if (ctrl && key == VK_OEM_COMMA) {
        openSettings();
    } else if (key == VK_F1) {
        openAbout();
    }
}

void MainWindow::onTimer(UINT_PTR id) {
    if (id == kTimerPump) {
        pumpUiMessages();
        // 无声提醒
        if (running_.load() && !silenceWarned_) {
            if (GetTickCount() - lastSpeechTick_ > 10000) {
                silenceWarned_ = true;
                setStatus("已有 10 秒没有听到声音，检查麦克风", theme().yellow);
            }
        }
    } else if (id == kTimerUpdate) {
        KillTimer(hwnd_, kTimerUpdate);
        checkUpdateAsync();
    } else if (id == kTimerWelcome) {
        KillTimer(hwnd_, kTimerWelcome);
        eggs::showWelcome(hwnd_);
    }
}

// ---------------------------------------------------------------- 业务
void MainWindow::startCapture() {
    if (running_.load()) return;
    running_.store(true);
    stopping_.store(false);
    startTick_ = GetTickCount();
    sentences_ = charsIn_ = charsOut_ = 0;
    segments_.clear();
    sessionStart_ = nowSeconds();
    lastSpeechTick_ = GetTickCount();
    silenceWarned_ = false;

    btnStart_.setText(str::kStop);
    btnStart_.setColors(theme().red, RGB(0xEF, 0x44, 0x44), RGB(0xFF, 0xFF, 0xFF));
    setStatus(str::kStarting, theme().yellow);

    int devIndex = 0;
    std::string dev = deviceBox_.selectedText();
    if (!dev.empty() && dev[0] == '[') {
        devIndex = std::atoi(dev.c_str() + 1);
    }

    capture_->vad().reset();
    std::string err = capture_->start(devIndex);
    if (!err.empty()) {
        running_.store(false);
        setStatus(str::kMicError, theme().red);
        MessageBoxW(hwnd_, toWide(err).c_str(), L"麦克风错误", MB_OK | MB_ICONWARNING);
        btnStart_.setText(str::kStart);
        btnStart_.setColors(theme().accent, theme().accent2, RGB(0x03, 0x21, 0x1A));
        return;
    }
    setStatus(str::kListening, theme().listen);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::stopCapture() {
    if (!running_.load()) return;
    stopping_.store(true);
    if (capture_) capture_->stop();
    running_.store(false);
    stopping_.store(false);
    btnStart_.setText(str::kStart);
    btnStart_.setColors(theme().accent, theme().accent2, RGB(0x03, 0x21, 0x1A));
    setStatus(str::kStopped, theme().dim);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::onLevel(double rms) {
    (void)rms;
    // 电平条绘制由 paint 处理；此处只在变化明显时刷新
}

void MainWindow::onPartial(const std::vector<float>& audio) {
    if (!running_.load() || !recognizer_) return;
    std::string err;
    AsrResult r = recognizer_->transcribe(audio, &err);
    if (r.empty()) return;
    UiMessage msg;
    msg.kind = UiMessage::Kind::PartialSrc;
    msg.s1 = r.text;
    msg.s2 = r.lang;
    postMessage(msg);
}

void MainWindow::onSentence(const std::vector<float>& audio) {
    if (!running_.load()) return;
    UiMessage st;
    st.kind = UiMessage::Kind::Status;
    st.s1 = str::kRecognizing;
    postMessage(st);

    std::string err;
    AsrResult r = recognizer_->transcribe(audio, &err);
    if (r.text.empty()) {
        UiMessage m;
        m.kind = UiMessage::Kind::Status;
        m.s1 = str::kNoSpeech;
        postMessage(m);
        return;
    }
    if (!isMeaningful(r.text)) {
        UiMessage m;
        m.kind = UiMessage::Kind::Status;
        m.s1 = str::kPunctOnly;
        postMessage(m);
        return;
    }

    // 自动纠错
    std::string text = r.text;
    if (correctCheck_.checked() && translator_) {
        text = translator_->autoCorrect(text, r.lang);
    }

    std::string ts = nowTimeString();

    // 真实时间轴：音频片段时长 ≈ 说话时长，宁可早一点也不要晚
    Segment seg;
    seg.start = nowSeconds() - static_cast<double>(audio.size()) / kSampleRate;
    seg.end = nowSeconds();
    seg.src = text;
    segments_.push_back(seg);

    UiMessage srcMsg;
    srcMsg.kind = UiMessage::Kind::Src;
    srcMsg.s1 = ts;
    srcMsg.s2 = text;
    srcMsg.s3 = langDisplayName(r.lang);
    postMessage(srcMsg);

    UiMessage tr;
    tr.kind = UiMessage::Kind::Status;
    tr.s1 = str::kTranslating;
    postMessage(tr);

    UiMessage begin;
    begin.kind = UiMessage::Kind::DstBegin;
    begin.s1 = ts;
    postMessage(begin);

    std::string target = targetLangBox_.selectedText();
    MtResult mt = translator_->translate(
        text, target, r.lang, [this](const std::string& chunk) {
            UiMessage c;
            c.kind = UiMessage::Kind::DstChunk;
            c.s1 = chunk;
            postMessage(c);
        });

    UiMessage end;
    end.kind = UiMessage::Kind::DstEnd;
    end.s1 = mt.text;
    end.s2 = mt.provider;
    postMessage(end);
}

void MainWindow::postMessage(UiMessage msg) {
    {
        std::lock_guard<std::mutex> guard(queueLock_);
        queue_.push(std::move(msg));
    }
    if (hwnd_) PostMessageW(hwnd_, WM_APP + 1, 0, 0);
}

void MainWindow::pumpUiMessages() {
    std::queue<UiMessage> local;
    {
        std::lock_guard<std::mutex> guard(queueLock_);
        std::swap(local, queue_);
    }
    while (!local.empty()) {
        applyMessage(local.front());
        local.pop();
    }
    updateScrollbars();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::updateScrollbars() {
    int viewH = srcView_.rect().bottom - srcView_.rect().top;
    srcScroll_.setRange(srcView_.contentHeight(), viewH);
    dstScroll_.setRange(dstView_.contentHeight(), viewH);
    srcView_.setScroll(srcScroll_.position());
    dstView_.setScroll(dstScroll_.position());
}

void MainWindow::applyMessage(const UiMessage& msg) {
    switch (msg.kind) {
        case UiMessage::Kind::Status:
            setStatus(msg.s1, theme().accent);
            break;

        case UiMessage::Kind::PartialSrc:
            // 临时结果单独占一行，下一句直接替换，不会堆积
            srcView_.setPartial("… " + msg.s1);
            break;

        case UiMessage::Kind::Src: {
            srcView_.clearPartial();
            std::string line = "[" + msg.s1 + "] (" + msg.s3 + ") " + msg.s2;
            srcView_.appendLine(line);
            ++sentences_;
            charsIn_ += static_cast<long long>(msg.s2.size());
            lastSpeechTick_ = GetTickCount();
            silenceWarned_ = false;
            break;
        }

        case UiMessage::Kind::DstBegin:
            dstView_.append("[" + msg.s1 + "] ");
            break;

        case UiMessage::Kind::DstChunk:
            dstView_.append(msg.s1);
            break;

        case UiMessage::Kind::DstEnd: {
            std::string line = msg.s1;
            if (!msg.s2.empty()) line += "    — " + msg.s2;
            dstView_.appendLine(line);
            if (!segments_.empty() && segments_.back().dst.empty()) {
                segments_.back().dst = msg.s1;
                segments_.back().end = nowSeconds();
            }
            charsOut_ += static_cast<long long>(msg.s1.size());
            break;
        }

        case UiMessage::Kind::UpdateAvailable:
            showUpdateDialog(msg.s1, msg.s2, msg.s3, Updater::pageUrl());
            break;

        case UiMessage::Kind::UpdateProgress: {
            if (msg.n2 > 0) {
                char buf[128];
                std::snprintf(buf, sizeof(buf), "正在下载更新 %lld / %lld KB",
                              msg.n1 / 1024, msg.n2 / 1024);
                setStatus(buf, theme().yellow);
            }
            break;
        }

        case UiMessage::Kind::UpdateDone: {
            setStatus("更新已下载完成", theme().green);
            int ret = MessageBoxW(
                hwnd_,
                L"新版本安装包已下载完成。\n\n点击「确定」将关闭本程序并启动安装程序。\n"
                L"覆盖安装即可，你的设置、API Key 都会保留。",
                L"更新已下载", MB_OKCANCEL | MB_ICONINFORMATION);
            if (ret == IDOK) {
                ShellExecuteW(nullptr, L"open", toWide(msg.s1).c_str(), nullptr, nullptr,
                              SW_SHOWNORMAL);
                PostMessageW(hwnd_, WM_CLOSE, 0, 0);
            }
            break;
        }

        case UiMessage::Kind::UpdateFailed:
            setStatus("更新下载失败", theme().red);
            MessageBoxW(hwnd_, toWide("更新包下载失败：" + msg.s1).c_str(), L"下载失败",
                        MB_OK | MB_ICONWARNING);
            break;

        default:
            break;
    }
}

void MainWindow::setStatus(const std::string& text, COLORREF color) {
    statusText_ = text;
    statusColor_ = color;
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::setStatusThreadSafe(const std::string& text) {
    UiMessage m;
    m.kind = UiMessage::Kind::Status;
    m.s1 = text;
    postMessage(m);
}

// ---------------------------------------------------------------- 侧栏动作
void MainWindow::onModeChanged(int index) {
    (void)index;
    std::string mode = modeBox_.selectedText();
    config().translatorMode = mode;
    saveConfig();
    applyModeLanguages();
    if (mode == TR_API) {
        setStatus(std::string(str::kApiModeStarted) + config().apiEngine, theme().yellow);
        if (!correctCheck_.checked()) {
            correctCheck_.setChecked(true);
            config().autoCorrect = true;
            saveConfig();
        }
    } else if (mode == TR_FREE) {
        setStatus("在线翻译（免费）：不用填 Key、不花钱、国内直连", theme().accent);
    } else {
        setStatus("本地翻译（离线）：仅支持 英语→中文", theme().accent);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::onApiEngineChanged(int index) {
    (void)index;
    config().apiEngine = apiEngineBox_.selectedText();
    saveConfig();
    if (config().translatorMode == TR_API) {
        setStatus(std::string(str::kApiModeStarted) + config().apiEngine, theme().yellow);
    }
}

void MainWindow::onInputLangChanged(int index) {
    if (running_.load()) setStatus(str::kLangChanged, theme().yellow);
    (void)index;
    std::string lang = inLangBox_.selectedText();
    if (recognizer_) {
        recognizer_->setLanguage(lang);
        if (recognizer_->needsWhisper()) {
            setStatus("「" + lang + "」SenseVoice 不支持，已自动切到 Whisper",
                      theme().yellow);
        } else {
            setStatus("输入语言：" + lang, theme().accent);
        }
    }
}

void MainWindow::onTargetLangChanged(int index) {
    if (running_.load()) setStatus(str::kLangChanged, theme().yellow);
    (void)index;
}

void MainWindow::onEngineChanged(int index) {
    (void)index;
}

void MainWindow::onGpuToggled(bool on) {
    config().gpuAccel = on;
    saveConfig();
    if (recognizer_) recognizer_->invalidate();
    if (translator_) translator_->invalidateLocal();
    if (!on) {
        setStatus(str::kGpuOff, theme().dim);
        return;
    }

    // 开启前真实探测，避免「勾了却没生效」的假象
    setStatus(str::kGpuProbing, theme().dim);
    gpuProbePending_ = true;
    if (gpuThread_.joinable()) gpuThread_.join();
    HWND hwnd = hwnd_;
    gpuThread_ = std::thread([this, hwnd] {
        gpu::ProbeResult r = gpu::probe(opusModelDir());
        {
            std::lock_guard<std::mutex> lk(gpuMtx_);
            gpuResult_ = r;
        }
        if (hwnd) PostMessageW(hwnd, WM_APP + 2, 0, 0);
    });
}

void MainWindow::onGpuProbeDone() {
    gpu::ProbeResult r;
    {
        std::lock_guard<std::mutex> lk(gpuMtx_);
        r = gpuResult_;
    }
    gpuProbePending_ = false;

    if (r.usable) {
        config().gpuAccel = true;
        saveConfig();
        if (recognizer_) recognizer_->invalidate();
        if (translator_) translator_->invalidateLocal();
        setStatus(str::kGpuOnOk, theme().green);
        return;
    }

    // 没成功：把开关退回关闭态，说清为什么
    config().gpuAccel = false;
    saveConfig();
    gpuCheck_.setChecked(false);
    if (recognizer_) recognizer_->invalidate();
    if (translator_) translator_->invalidateLocal();
    setStatus(std::string(str::kGpuFailPrefix) + r.msg, theme().yellow);

    std::string body = r.msg + "\n\n";
    if (r.hasCard) {
        body += str::kGpuWarnBodyHasCard;
        MessageBoxW(hwnd_, toWide(body).c_str(), toWide(str::kGpuWarnTitle).c_str(),
                    MB_OK | MB_ICONWARNING);
    } else {
        body += str::kGpuInfoBodyNoCard;
        MessageBoxW(hwnd_, toWide(body).c_str(), toWide(str::kGpuInfoTitle).c_str(),
                    MB_OK | MB_ICONINFORMATION);
    }
}

void MainWindow::onCorrectToggled(bool on) {
    std::string mode = modeBox_.selectedText();
    if (on && mode != TR_API && !config().correctWarnSkip) {
        // TaskDialog 带「不再提醒」勾选框（与 Python 版一致）
        TASKDIALOGCONFIG cfg{};
        cfg.cbSize = sizeof(cfg);
        cfg.hwndParent = hwnd_;
        cfg.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
        cfg.pszWindowTitle = L"Hearizon Sync";
        cfg.pszMainIcon = TD_WARNING_ICON;
        cfg.pszMainInstruction = toWide(str::kCorrectConsumeTitle).c_str();
        cfg.pszContent = toWide(str::kCorrectConsumeBody).c_str();
        cfg.pszVerificationText = toWide(str::kDoNotRemind).c_str();
        cfg.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;

        BOOL verified = FALSE;
        int pressed = 0;
        if (FAILED(TaskDialogIndirect(&cfg, &pressed, nullptr, &verified))) {
            // 极旧系统退回 MessageBox
            int ret = MessageBoxW(hwnd_, toWide(str::kCorrectConsumeBody).c_str(),
                                  toWide(str::kCorrectConsumeTitle).c_str(),
                                  MB_YESNO | MB_ICONWARNING);
            pressed = (ret == IDYES) ? IDYES : IDNO;
        }
        if (verified) {
            config().correctWarnSkip = true;
            saveConfig();
        }
        if (pressed != IDYES) {
            correctCheck_.setChecked(false);
            InvalidateRect(hwnd_, nullptr, FALSE);
            return;
        }
    }
    config().autoCorrect = on;
    saveConfig();
    setStatus(on ? "自动纠错已开启（会消耗 API 余额）" : "自动纠错已关闭",
              on ? theme().yellow : theme().dim);
}

void MainWindow::onThemeChanged(int index) {
    themeIndex_ = index;
    config().subTheme = subThemeByIndex(index).key;
    saveConfig();
    const SubTheme& st = subThemeByIndex(index);
    srcView_.setColors(st.srcFg, st.bg);
    dstView_.setColors(st.dstFg, st.bg);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::updatePosButtons() {
    const Theme& t = theme();
    auto paint = [&](Button& b, bool on) {
        if (on)
            b.setColors(t.accent, t.accent2, RGB(0x03, 0x21, 0x1A));
        else
            b.setColors(t.card, t.cardHover, t.text2);
    };
    paint(btnPosBottom_, !subTop_);
    paint(btnPosTop_, subTop_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void MainWindow::onPositionChanged(bool top) {
    subTop_ = top;
    config().subPos = top ? "top" : "bottom";
    saveConfig();
    updatePosButtons();
}

void MainWindow::refreshDevices() {
    auto devices = enumerateInputDevices();
    std::vector<std::string> names;
    for (const auto& d : devices) {
        std::string n = "[" + std::to_string(d.index) + "] " + d.name;
        if (d.isLoopbackLike) n += "  【系统声音】";
        names.push_back(n);
    }
    deviceBox_.setItems(names);
    if (!names.empty()) deviceBox_.setSelected(0);
    setStatus("已刷新设备：" + std::to_string(names.size()) + " 个", theme().accent);
}

void MainWindow::applyModeLanguages() {
    std::string mode = modeBox_.selectedText();
    std::vector<std::string> inOk = modeInputLangs(mode);
    std::vector<std::string> outOk = modeOutputLangs(mode);

    std::vector<std::string> inDisabled, outDisabled;
    for (const auto& l : INPUT_LANG_NAMES) {
        if (std::find(inOk.begin(), inOk.end(), l) == inOk.end()) inDisabled.push_back(l);
    }
    for (const auto& l : OUTPUT_LANG_NAMES) {
        if (std::find(outOk.begin(), outOk.end(), l) == outOk.end()) outDisabled.push_back(l);
    }
    inLangBox_.setDisabledItems(inDisabled);
    targetLangBox_.setDisabledItems(outDisabled);

    // API 引擎下拉只在 API 模式下可用
    apiEngineBox_.setEnabled(mode == TR_API);

    // 当前选择不可用时自动纠正
    if (std::find(inDisabled.begin(), inDisabled.end(), inLangBox_.selectedText()) !=
        inDisabled.end()) {
        inLangBox_.setSelected(0);
    }
    if (std::find(outDisabled.begin(), outDisabled.end(), targetLangBox_.selectedText()) !=
        outDisabled.end()) {
        targetLangBox_.setSelected(0);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

}  // namespace hs

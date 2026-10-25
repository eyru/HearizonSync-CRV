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

namespace hs {

MainWindow* g_mainWindow = nullptr;

namespace {
const wchar_t* kWindowClass = L"HearizonSyncCRVMain";
const int kSidebarWidth = 252;
const int kSidebarPad = 16;
const int kStatusHeight = 30;
const int kPanelGap = 8;
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
    wc.hIcon = LoadIconW(hInst, L"APPICON");
    wc.hIconSm = wc.hIcon;
    if (!RegisterClassExW(&wc)) return false;

    Config& c = config();
    int w = c.windowW > 0 ? c.windowW : 1180;
    int h = c.windowH > 0 ? c.windowH : 780;
    int x = c.windowX >= 0 ? c.windowX : CW_USEDEFAULT;
    int y = c.windowY >= 0 ? c.windowY : CW_USEDEFAULT;

    std::wstring title = toWide(std::string(str::kAppTitle) + " v" + appVersion());
    hwnd_ = CreateWindowExW(0, kWindowClass, title.c_str(),
                            WS_OVERLAPPEDWINDOW, x, y, w, h, nullptr, nullptr, hInst,
                            nullptr);
    if (!hwnd_) return false;

    Dpi::update(hwnd_);

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
    return self->handleMessage(msg, wp, lp);
}

LRESULT MainWindow::handleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
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
            Config& c = config();
            RECT rc;
            if (GetWindowRect(hwnd_, &rc)) {
                c.windowX = rc.left;
                c.windowY = rc.top;
                c.windowW = rc.right - rc.left;
                c.windowH = rc.bottom - rc.top;
                saveConfig();
            }
            KillTimer(hwnd_, kTimerPump);
            KillTimer(hwnd_, kTimerUpdate);
            KillTimer(hwnd_, kTimerSilence);
            PostQuitMessage(0);
            return 0;
        }

        case WM_APP + 1:   // 来自其他线程的刷新请求
            pumpUiMessages();
            InvalidateRect(hwnd_, nullptr, FALSE);
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
    int scale = static_cast<int>(Dpi::scale() + 0.5);
    if (scale < 1) scale = 1;

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
        {&btnTestMic_, str::kTestMic},   {&btnSave_, str::kSaveRecord},
        {&btnExport_, str::kExportSrt},  {&btnClear_, str::kClearSubtitle},
        {&btnTray_, str::kMinimizeToTray}, {&btnAbout_, str::kAbout},
        {&btnRefresh_, str::kRefreshDevice},
        {&btnPosBottom_, str::kPositionBottom},
        {&btnPosTop_, str::kPositionTop},
    };
    for (auto& d : defs) {
        d.b->setText(d.text);
        d.b->setFont(ui8);
        d.b->setColors(theme().sidebar, theme().card, theme().text2);
        d.b->setRadius(7 * scale);
        d.b->setAnchorLeft(true);
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
    btnPosBottom_.setOnClick([this]() { onPositionChanged(false); });
    btnPosTop_.setOnClick([this]() { onPositionChanged(true); });

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
        tray_->start(hwnd_, idle, live, tip);
    }
}

// ---------------------------------------------------------------- 布局
void MainWindow::layout() {
    RECT rc;
    GetClientRect(hwnd_, &rc);
    int scale = static_cast<int>(Dpi::scale() + 0.5);
    if (scale < 1) scale = 1;

    int sidebarW = kSidebarWidth * scale;
    rcSidebar_ = {0, 0, sidebarW, rc.bottom};
    rcStatus_ = {0, rc.bottom - kStatusHeight * scale, rc.right, rc.bottom};

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

    // 侧栏控件
    int y = 118 * scale;
    int left = kSidebarPad * scale;
    int w = sidebarW - left * 2;
    int h = 34 * scale;
    int gap = 8 * scale;

    auto place = [&](ComboBox& box, int height) {
        box.setRect({left, y, left + w, y + height});
        y += height + gap;
    };

    inLangBox_.setRect({left, y, left + w, y + h});
    y += h + gap;
    deviceBox_.setRect({left, y, left + w, y + h});
    y += h + gap;
    targetLangBox_.setRect({left, y, left + w, y + h});
    y += h + gap;
    modeBox_.setRect({left, y, left + w, y + h});
    y += h + gap;
    apiEngineBox_.setRect({left, y, left + w, y + h});
    y += h + gap;
    engineBox_.setRect({left, y, left + w, y + h});
    y += h + gap + 4 * scale;

    // GPU + 刷新设备
    gpuCheck_.setRect({left, y, left + 100 * scale, y + 24 * scale});
    btnRefresh_.setRect({left + w - 64 * scale, y, left + w, y + 24 * scale});
    y += 24 * scale + gap;

    // 自动纠错
    correctCheck_.setRect({left, y, left + w, y + 24 * scale});
    y += 24 * scale + gap + 6 * scale;

    // 操作按钮
    int btnH = 30 * scale;
    for (Button* b : {&btnTestMic_, &btnSave_, &btnExport_, &btnClear_, &btnTray_}) {
        b->setRect({left - 4 * scale, y, left + w + 4 * scale, y + btnH});
        y += btnH + 2 * scale;
    }
    y += 8 * scale;

    // 字幕样式
    int halfW = (w - 6 * scale) / 2;
    btnPosBottom_.setRect({left, y, left + halfW, y + 24 * scale});
    btnPosTop_.setRect({left + halfW + 6 * scale, y, left + w, y + 24 * scale});
    y += 24 * scale + gap;
    themeBox_.setRect({left, y, left + w, y + 28 * scale});
    y += 28 * scale + gap;

    // 开始按钮（底部固定）
    int startH = 44 * scale;
    btnStart_.setRect({left - 4 * scale, rcStatus_.top - startH - 12 * scale,
                       left + w + 4 * scale, rcStatus_.top - 12 * scale});

    // 关于按钮（状态栏右侧）
    btnAbout_.setRect({rc.right - 70 * scale, rcStatus_.top + 4 * scale,
                       rc.right - 8 * scale, rcStatus_.bottom - 4 * scale});
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
    int scale = static_cast<int>(Dpi::scale() + 0.5);
    if (scale < 1) scale = 1;

    HBRUSH sb = CreateSolidBrush(theme().sidebar);
    FillRect(dc, &rcSidebar_, sb);
    DeleteObject(sb);

    int left = kSidebarPad * scale;
    int y = 18 * scale;

    // 标题
    RECT titleRc{left, y, rcSidebar_.right - left, y + 30 * scale};
    drawText(dc, toWide("Hearizon Sync"), titleRc, theme().text,
             uiFont(Dpi::fontPt(15), true), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += 34 * scale;

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

    // 各字段标签
    auto label = [&](const char* text, int ly) {
        RECT lr{left, ly, rcSidebar_.right - left, ly + 14 * scale};
        drawText(dc, toWide(text), lr, theme().dim, uiFont(Dpi::fontPt(8)),
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    };

    int ly = 96 * scale;
    label(str::kInputLang, ly);
    label(str::kAudioDevice, ly + 42 * scale);
    label(str::kTargetLang, ly + 84 * scale);
    label(str::kTranslateMode, ly + 126 * scale);
    label(str::kApiEngine, ly + 168 * scale);
    label(str::kAsrEngine, ly + 210 * scale);

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
                      &btnTray_, &btnPosBottom_, &btnPosTop_}) {
        b->paint(dc, rcSidebar_);
    }

    // 字幕样式标题
    RECT styleRc{left, themeBox_.rect().top - 26 * scale, rcSidebar_.right - left,
                 themeBox_.rect().top - 10 * scale};
    drawText(dc, toWide(str::kSubtitleStyle), styleRc, theme().dim,
             uiFont(Dpi::fontPt(8)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    themeBox_.paint(dc);

    // 开始按钮
    btnStart_.paint(dc, rcSidebar_);
}

void MainWindow::paintPanels(HDC dc) {
    int scale = static_cast<int>(Dpi::scale() + 0.5);
    if (scale < 1) scale = 1;
    const SubTheme& st = subThemeByIndex(themeIndex_);

    auto paintPanel = [&](const RECT& rc, const char* title, COLORREF dotColor,
                          TextView& view, ScrollBar& bar) {
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

        view.paint(dc);
        bar.paint(dc);
    };

    paintPanel(rcSrcPanel_, str::kSourceTitle, st.srcFg, srcView_, srcScroll_);
    paintPanel(rcDstPanel_, str::kTargetTitle, st.dstFg, dstView_, dstScroll_);
}

void MainWindow::paintStatusBar(HDC dc) {
    int scale = static_cast<int>(Dpi::scale() + 0.5);
    if (scale < 1) scale = 1;

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
    RECT statRc{rcStatus_.left + 14 * scale, rcStatus_.top, rcStatus_.right - 80 * scale,
                rcStatus_.bottom};
    drawText(dc, toWide(buf), statRc, theme().faint, uiFont(Dpi::fontPt(8)),
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);

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
    for (Button* b : {&btnStart_, &btnTestMic_, &btnSave_, &btnExport_, &btnClear_,
                      &btnTray_, &btnAbout_, &btnRefresh_, &btnPosBottom_,
                      &btnPosTop_}) {
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

    // 标题栏拖动（顶部 34px 空白区）
    if (y < 34 * static_cast<int>(Dpi::scale() + 0.5)) {
        dragging_ = true;
        dragStart_ = {x, y};
        RECT wr;
        GetWindowRect(hwnd_, &wr);
        dragWindowRect_ = wr;
        return;
    }

    gpuCheck_.onMouseDown(x, y);
    correctCheck_.onMouseDown(x, y);
    for (Button* b : {&btnStart_, &btnTestMic_, &btnSave_, &btnExport_, &btnClear_,
                      &btnTray_, &btnAbout_, &btnRefresh_, &btnPosBottom_,
                      &btnPosTop_}) {
        b->onMouseDown(x, y);
    }
}

void MainWindow::onMouseUp(int x, int y) {
    if (dragging_) {
        dragging_ = false;
        return;
    }

    if (btnStart_.onMouseUp(x, y)) return;
    if (btnTestMic_.onMouseUp(x, y)) {
        testMic();
        return;
    }
    if (btnSave_.onMouseUp(x, y)) {
        saveRecord();
        return;
    }
    if (btnExport_.onMouseUp(x, y)) {
        exportSrt();
        return;
    }
    if (btnClear_.onMouseUp(x, y)) {
        clearSubtitles();
        return;
    }
    if (btnTray_.onMouseUp(x, y)) {
        minimizeToTray();
        return;
    }
    if (btnAbout_.onMouseUp(x, y)) {
        openAbout();
        return;
    }
    if (btnRefresh_.onMouseUp(x, y)) {
        refreshDevices();
        return;
    }
    if (btnPosBottom_.onMouseUp(x, y)) {
        subTop_ = false;
        config().subPos = "bottom";
        saveConfig();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    if (btnPosTop_.onMouseUp(x, y)) {
        subTop_ = true;
        config().subPos = "top";
        saveConfig();
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }

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

void MainWindow::onKeyDown(WPARAM key) {
    bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
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
        subSizeDst_ = std::min(32, subSizeDst_ + 1);
        InvalidateRect(hwnd_, nullptr, FALSE);
    } else if (ctrl && (key == VK_SUBTRACT || key == VK_OEM_MINUS)) {
        subSizeDst_ = std::max(14, subSizeDst_ - 1);
        InvalidateRect(hwnd_, nullptr, FALSE);
    } else if (ctrl && key == '0') {
        subSizeDst_ = 18;
        InvalidateRect(hwnd_, nullptr, FALSE);
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
    } else if (id == kTimerVersionReset) {
        KillTimer(hwnd_, kTimerVersionReset);
        versionClicks_ = 0;
    }
}

// ---------------------------------------------------------------- 业务
void MainWindow::startCapture() {
    if (running_.load()) return;
    running_.store(true);
    stopping_.store(false);
    startTick_ = GetTickCount();
    sentences_ = charsIn_ = charsOut_ = 0;
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
            // 简化：临时结果直接追加到原文（正式结果会覆盖）
            srcView_.append(msg.s1);
            break;

        case UiMessage::Kind::Src: {
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
    if (on) {
        setStatus("已开启 GPU（下次识别重新加载模型）", theme().accent);
    } else {
        setStatus("已关闭 GPU，全部走 CPU", theme().dim);
    }
}

void MainWindow::onCorrectToggled(bool on) {
    std::string mode = modeBox_.selectedText();
    if (on && mode != TR_API && !config().correctWarnSkip) {
        // 弹出提醒
        int ret = MessageBoxW(hwnd_,
                              toWide(str::kCorrectConsumeBody).c_str(),
                              toWide(str::kCorrectConsumeTitle).c_str(),
                              MB_YESNO | MB_ICONWARNING);
        if (ret != IDYES) {
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

void MainWindow::onPositionChanged(bool top) {
    subTop_ = top;
    config().subPos = top ? "top" : "bottom";
    saveConfig();
    InvalidateRect(hwnd_, nullptr, FALSE);
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

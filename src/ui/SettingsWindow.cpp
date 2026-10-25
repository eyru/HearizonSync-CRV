#include "SettingsWindow.h"

#include <windowsx.h>

#include <string>

#include "core/Config.h"
#include "core/Strings.h"
#include "core/Theme.h"
#include "core/Util.h"
#include "audio/AudioCapture.h"
#include "core/Gpu.h"
#include "ui/Eggs.h"
#include "ui/Widgets.h"

namespace hs {

namespace {

const wchar_t* kSettingsClass = L"HearizonSyncCRVSettings";

struct State {
    HWND hwnd = nullptr;
    HWND owner = nullptr;
    HWND edDeepseek = nullptr;
    HWND edMymemory = nullptr;
    HWND edYoudaoKey = nullptr;
    HWND edYoudaoSecret = nullptr;
    HWND edHotwords = nullptr;
    bool updateCheck = true;
    bool saved = false;
    bool running = true;
    double scale = 1.0;

    HBRUSH bgBrush = nullptr;
    HBRUSH cardBrush = nullptr;

    RECT rcSave{};
    RECT rcClose{};
    RECT rcCheck{};
    int hover = 0;   // 1 保存 2 关闭 3 复选
    int pressed = 0;
};

State* g_state = nullptr;

int S(int v) { return scaledPx(v, g_state ? g_state->scale : 1.0); }

void drawLabel(HDC dc, const std::string& text, int x, int y, int w,
               COLORREF color, int pt, bool bold = false) {
    RECT rc{x, y, x + w, y + S(16)};
    drawText(dc, toWide(text), rc, color, uiFont(Dpi::fontPt(pt), bold),
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void drawHint(HDC dc, const std::string& text, int x, int y, int w) {
    RECT rc{x, y, x + w, y + S(30)};
    drawText(dc, toWide(text), rc, theme().faint, uiFont(Dpi::fontPt(8)),
             DT_LEFT | DT_TOP | DT_WORDBREAK);
}

void drawButton(HDC dc, const RECT& rc, const std::string& text, bool hot,
                bool primary) {
    COLORREF bg = primary ? (hot ? theme().accent2 : theme().accent)
                          : (hot ? theme().cardHover : theme().card);
    COLORREF fg = primary ? RGB(0x03, 0x21, 0x1A) : theme().text2;
    HBRUSH brush = CreateSolidBrush(bg);
    HPEN pen = CreatePen(PS_SOLID, 1, primary ? bg : theme().border);
    HBRUSH ob = static_cast<HBRUSH>(SelectObject(dc, brush));
    HPEN op = static_cast<HPEN>(SelectObject(dc, pen));
    roundRect(dc, rc, S(9));
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(brush);
    DeleteObject(pen);
    RECT tr = rc;
    drawText(dc, toWide(text), tr, fg, uiFont(Dpi::fontPt(9), primary),
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void drawCheck(HDC dc, const RECT& rc, const std::string& text, bool checked,
               bool hot) {
    int box = S(15);
    RECT boxRc{rc.left, rc.top + (rc.bottom - rc.top - box) / 2, rc.left + box,
               rc.top + (rc.bottom - rc.top - box) / 2 + box};
    HBRUSH brush = CreateSolidBrush(checked ? theme().accent : theme().panel);
    HPEN pen = CreatePen(PS_SOLID, 1,
                         checked ? theme().accent : (hot ? theme().border2 : theme().border));
    HBRUSH ob = static_cast<HBRUSH>(SelectObject(dc, brush));
    HPEN op = static_cast<HPEN>(SelectObject(dc, pen));
    roundRect(dc, boxRc, S(4));
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(brush);
    DeleteObject(pen);

    if (checked) {
        HPEN cp = CreatePen(PS_SOLID, S(2), RGB(0x03, 0x21, 0x1A));
        HPEN old = static_cast<HPEN>(SelectObject(dc, cp));
        int cx = boxRc.left + box / 2;
        int cy = boxRc.top + box / 2;
        MoveToEx(dc, cx - box / 5, cy, nullptr);
        LineTo(dc, cx - box / 12, cy + box / 5);
        LineTo(dc, cx + box / 4, cy - box / 5);
        SelectObject(dc, old);
        DeleteObject(cp);
    }

    RECT tr{boxRc.right + S(9), rc.top, rc.right, rc.bottom};
    drawText(dc, toWide(text), tr, theme().text, uiFont(Dpi::fontPt(9)),
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

std::string fieldText(HWND h) {
    if (!h) return std::string();
    int len = GetWindowTextLengthW(h);
    if (len <= 0) return std::string();
    std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(h, &buf[0], len + 1);
    buf.resize(static_cast<size_t>(len));
    return trim(toUtf8(buf));
}

void setFieldText(HWND h, const std::string& text) {
    if (!h) return;
    SetWindowTextW(h, toWide(text).c_str());
}

struct FieldRects {
    RECT label{};
    RECT card{};    // 输入框的卡片底
    RECT edit{};    // 真正的子窗口 EDIT
    RECT hint{};
    bool hasHint = false;
};

struct Layout {
    RECT title{}, version{}, modeTitle{}, modeDesc{}, modeUsage{}, groupTitle{};
    FieldRects f[5];
    RECT cableTitle{}, cableHint{}, cableState{};
    RECT gpuTitle{}, gpuState{}, gpuHint{};
    RECT check{}, save{}, close{};
    int bottom = 0;
};

// 说明文字按宽度自动折行后的高度
int measureText(HDC dc, const std::string& text, int w, int pt) {
    HFONT f = uiFont(Dpi::fontPt(pt));
    HGDIOBJ old = SelectObject(dc, f);
    RECT rc{0, 0, w, 0};
    DrawTextW(dc, toWide(text).c_str(), -1, &rc,
              DT_LEFT | DT_TOP | DT_WORDBREAK | DT_CALCRECT);
    SelectObject(dc, old);
    DeleteObject(f);
    int h = rc.bottom - rc.top;
    return h > 0 ? h : S(16);
}

std::string currentModeDesc() {
    std::string mode = config().translatorMode;
    if (mode == TR_LOCAL) {
        return "本地翻译（离线）：只用本机模型，不联网、不花钱；目前支持 英语 → 中文。";
    }
    if (mode == TR_API) {
        return "API 模式（付费）：走 DeepSeek / 有道，语言最全，按量计费；需要在下面填 Key。";
    }
    return "在线翻译（免费）：国内直连 MyMemory，不用 Key、不花钱；额度用尽自动兜底到本地模型。";
}


// 本机检测到的虚拟声卡（VB-Cable 之类）数量
int cableCount() {
    int n = 0;
    for (const auto& d : enumerateInputDevices()) {
        std::string up = d.name;
        for (auto& c : up) c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
        if (up.find("CABLE") != std::string::npos) ++n;
    }
    return n;
}

std::string cableStateText() {
    int n = cableCount();
    if (n > 0) {
        return "已检测到虚拟声卡：" + std::to_string(n) + " 个，直接在「音频输入」里选即可";
    }
    return "（当前未检测到虚拟声卡，装好后点「刷新设备」）";
}

// 唯一的布局来源：paint 与 layoutChildren 都用它，标签/输入框/说明永不重叠
Layout computeLayout(HDC dc, int cw) {
    Layout L;
    const int pad = S(24);
    const int w = cw - pad * 2;
    int y = S(16);

    L.title = {pad, y, cw - pad - S(110), y + S(30)};
    L.version = {cw - pad - S(100), y + S(4), cw - pad, y + S(26)};
    y += S(40);

    L.modeTitle = {pad, y, cw - pad, y + S(20)};
    y += S(24);
    std::string desc = currentModeDesc();
    int dh = measureText(dc, desc, w, 8);
    L.modeDesc = {pad, y, cw - pad, y + dh};
    y += dh + S(4);
    std::string usage = "翻译模式在左侧竖栏里切换。下面填的 Key 只在对应模式起作用，填了也不影响其它模式。";
    int uh = measureText(dc, usage, w, 8);
    L.modeUsage = {pad, y, cw - pad, y + uh};
    y += uh + S(18);

    L.groupTitle = {pad, y, cw - pad, y + S(20)};
    y += S(26);

    const char* labels[5] = {str::kDeepseekKey, str::kMymemoryEmail, str::kAppKey,
                             str::kAppSecret, str::kHotwords};
    const char* hints[5] = {str::kDeepseekHint, str::kMymemoryHint, nullptr,
                            str::kYoudaoHint, str::kHotwordsHint};
    for (int i = 0; i < 5; ++i) {
        L.f[i].label = {pad, y, cw - pad, y + S(16)};
        y += S(20);
        int cardH = (i == 4) ? S(108) : S(42);
        L.f[i].card = {pad, y, cw - pad, y + cardH};
        L.f[i].edit = {pad + S(10), y + S(6), cw - pad - S(10), y + cardH - S(6)};
        y += cardH;
        if (hints[i]) {
            int hh = measureText(dc, hints[i], w, 8);
            y += S(6);
            L.f[i].hint = {pad, y, cw - pad, y + hh};
            L.f[i].hasHint = true;
            y += hh;
        }
        y += S(14);
    }

    // ---- 虚拟声卡（网课 / 系统声音）----
    L.cableTitle = {pad, y, cw - pad, y + S(20)};
    y += S(26);
    int ch = measureText(dc, str::kVbCableHint, w, 8);
    L.cableHint = {pad, y, cw - pad, y + ch};
    y += ch + S(5);
    int csh = measureText(dc, cableStateText(), w, 8);
    L.cableState = {pad, y, cw - pad, y + csh};
    y += csh + S(20);

    // ---- GPU 加速说明（先报本机真实检测结果，不再一律说「有显卡」）----
    L.gpuTitle = {pad, y, cw - pad, y + S(20)};
    y += S(26);
    int gsh = measureText(dc, gpu::quickSummary(), w, 8);
    L.gpuState = {pad, y, cw - pad, y + gsh};
    y += gsh + S(5);
    int ghh = measureText(dc, str::kGpuHint, w, 8);
    L.gpuHint = {pad, y, cw - pad, y + ghh};
    y += ghh + S(20);

    L.check = {pad, y, cw - pad, y + S(22)};
    y += S(22) + S(18);

    const int bw = S(120), bh = S(38);
    L.close = {cw - pad - bw, y, cw - pad, y + bh};
    L.save = {L.close.left - bw - S(12), y, L.close.left - S(12), y + bh};
    y += bh;
    L.bottom = y + S(20);
    return L;
}

void layoutChildren() {
    State* s = g_state;
    if (!s || !s->hwnd) return;
    RECT rc;
    GetClientRect(s->hwnd, &rc);
    HDC dc = GetDC(s->hwnd);
    Layout L = computeLayout(dc, rc.right);
    ReleaseDC(s->hwnd, dc);

    HWND eds[5] = {s->edDeepseek, s->edMymemory, s->edYoudaoKey, s->edYoudaoSecret,
                   s->edHotwords};
    for (int i = 0; i < 5; ++i) {
        const RECT& e = L.f[i].edit;
        MoveWindow(eds[i], e.left, e.top, e.right - e.left, e.bottom - e.top, TRUE);
    }
    s->rcCheck = L.check;
    s->rcSave = L.save;
    s->rcClose = L.close;
}

void doSave() {
    State* s = g_state;
    if (!s) return;

    std::string key = fieldText(s->edDeepseek);

    // 独家模块：触发词只在这个输入框里生效
    if (eggs::handleInput(s->hwnd, key)) {
        setFieldText(s->edDeepseek, "");
        SetFocus(s->edDeepseek);
        return;
    }

    Config& c = config();
    c.deepseekKey = key;
    c.mymemoryEmail = fieldText(s->edMymemory);
    c.youdaoAppKey = fieldText(s->edYoudaoKey);
    c.youdaoAppSecret = fieldText(s->edYoudaoSecret);
    c.hotwords = fieldText(s->edHotwords);
    c.updateCheck = s->updateCheck;
    saveConfig();

    s->saved = true;
    s->running = false;
}

// 单行输入框子类化：回车 = 先看是不是独家模块暗号，不是就保存并关闭。
// 不子类化的话 EDIT 会自己吃掉回车，父窗口收不到任何按键。
WNDPROC g_editOld = nullptr;

LRESULT CALLBACK EditProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
        doSave();
        State* s = g_state;
        if (s && !s->running && IsWindow(h)) DestroyWindow(s->hwnd);
        return 0;
    }
    if (msg == WM_CHAR && wp == VK_RETURN) return 0;
    return CallWindowProcW(g_editOld, h, msg, wp, lp);
}

void onMouseUp(int x, int y) {    State* s = g_state;
    if (!s) return;

    auto inside = [&](const RECT& r) {
        return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
    };

    if (inside(s->rcSave)) {
        doSave();
        if (!s->running) DestroyWindow(s->hwnd);
        return;
    }
    if (inside(s->rcClose)) {
        s->running = false;
        DestroyWindow(s->hwnd);
        return;
    }
    if (inside(s->rcCheck)) {
        s->updateCheck = !s->updateCheck;
        InvalidateRect(s->hwnd, nullptr, FALSE);
        return;
    }
}

void paint(HDC dc) {
    State* s = g_state;
    if (!s) return;
    RECT rc;
    GetClientRect(s->hwnd, &rc);

    FillRect(dc, &rc, s->bgBrush);

    Layout L = computeLayout(dc, rc.right);

    drawText(dc, toWide(str::kSettings), L.title, theme().text,
             uiFont(Dpi::fontPt(14), true), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    drawText(dc, toWide(std::string("v") + appVersion()), L.version, theme().faint,
             uiFont(Dpi::fontPt(8)), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    drawText(dc, toWide(str::kTranslateModeDesc), L.modeTitle, theme().accent2,
             uiFont(Dpi::fontPt(9), true), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    drawText(dc, toWide(currentModeDesc()), L.modeDesc, theme().dim,
             uiFont(Dpi::fontPt(8)), DT_LEFT | DT_TOP | DT_WORDBREAK);
    drawText(dc, toWide("翻译模式在左侧竖栏里切换。下面填的 Key 只在对应模式起作用，"
                        "填了也不影响其它模式。"),
             L.modeUsage, theme().faint, uiFont(Dpi::fontPt(8)),
             DT_LEFT | DT_TOP | DT_WORDBREAK);

    drawText(dc, toWide("API Key 与免费额度"), L.groupTitle, theme().text2,
             uiFont(Dpi::fontPt(9), true), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    const char* labels[5] = {str::kDeepseekKey, str::kMymemoryEmail, str::kAppKey,
                             str::kAppSecret, str::kHotwords};
    const char* hints[5] = {str::kDeepseekHint, str::kMymemoryHint, nullptr,
                            str::kYoudaoHint, str::kHotwordsHint};
    int hintPt = 8;
    for (int i = 0; i < 5; ++i) {
        drawText(dc, toWide(labels[i]), L.f[i].label, theme().dim,
                 uiFont(Dpi::fontPt(8)), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // 输入框底：圆角卡片
        HBRUSH bg = CreateSolidBrush(theme().card);
        HPEN pen = CreatePen(PS_SOLID, 1, theme().border);
        HBRUSH ob = static_cast<HBRUSH>(SelectObject(dc, bg));
        HPEN op = static_cast<HPEN>(SelectObject(dc, pen));
        roundRect(dc, L.f[i].card, S(9));
        SelectObject(dc, ob);
        SelectObject(dc, op);
        DeleteObject(bg);
        DeleteObject(pen);

        if (hints[i]) {
            drawText(dc, toWide(hints[i]), L.f[i].hint, theme().faint,
                     uiFont(Dpi::fontPt(hintPt)), DT_LEFT | DT_TOP | DT_WORDBREAK);
        }
    }

    // ---- 虚拟声卡 ----
    drawText(dc, toWide(str::kVbCableTitle), L.cableTitle, theme().accent2,
             uiFont(Dpi::fontPt(9), true), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    drawText(dc, toWide(str::kVbCableHint), L.cableHint, theme().faint,
             uiFont(Dpi::fontPt(8)), DT_LEFT | DT_TOP | DT_WORDBREAK);
    bool hasCable = cableCount() > 0;
    drawText(dc, toWide(cableStateText()), L.cableState,
             hasCable ? theme().accent : theme().faint, uiFont(Dpi::fontPt(8)),
             DT_LEFT | DT_TOP | DT_WORDBREAK);

    // ---- GPU ----
    drawText(dc, toWide(str::kGpuTitle), L.gpuTitle, theme().accent2,
             uiFont(Dpi::fontPt(9), true), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    std::string gs = gpu::quickSummary();
    bool gpuOk = gs.find("未检测到") == std::string::npos;
    drawText(dc, toWide(gs), L.gpuState, gpuOk ? theme().accent : theme().faint,
             uiFont(Dpi::fontPt(8)), DT_LEFT | DT_TOP | DT_WORDBREAK);
    drawText(dc, toWide(str::kGpuHint), L.gpuHint, theme().faint,
             uiFont(Dpi::fontPt(8)), DT_LEFT | DT_TOP | DT_WORDBREAK);

    drawCheck(dc, s->rcCheck, str::kUpdateCheck, s->updateCheck, s->hover == 3);
    drawButton(dc, s->rcSave, str::kSaveSettings, s->hover == 1, true);
    drawButton(dc, s->rcClose, str::kClose, s->hover == 2, false);
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    State* s = g_state;
    switch (msg) {
        case WM_CREATE: {
            if (!s) return -1;
            s->hwnd = hwnd;
            Dpi::update(hwnd);

            // 深色 EDIT
            auto mkEdit = [&](DWORD extra) {
                return CreateWindowExW(
                    0, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | extra,
                    0, 0, 10, 10, hwnd, nullptr, nullptr, nullptr);
            };
            s->edDeepseek = mkEdit(0);
            s->edMymemory = mkEdit(0);
            s->edYoudaoKey = mkEdit(0);
            s->edYoudaoSecret = mkEdit(0);
            s->edHotwords = CreateWindowExW(
                0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL |
                    WS_VSCROLL | ES_WANTRETURN,
                0, 0, 10, 10, hwnd, nullptr, nullptr, nullptr);

            HFONT f = uiFont(Dpi::fontPt(9));
            for (HWND h : {s->edDeepseek, s->edMymemory, s->edYoudaoKey,
                           s->edYoudaoSecret, s->edHotwords}) {
                SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE);
            }

            // 单行框接管回车（热词框是多行，保留自己的换行行为）
            if (!g_editOld) {
                g_editOld = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
                    s->edDeepseek, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditProc)));
            }
            for (HWND h : {s->edMymemory, s->edYoudaoKey, s->edYoudaoSecret}) {
                SetWindowLongPtrW(h, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditProc));
            }

            Config& c = config();
            setFieldText(s->edDeepseek, c.deepseekKey);
            setFieldText(s->edMymemory, c.mymemoryEmail);
            setFieldText(s->edYoudaoKey, c.youdaoAppKey);
            setFieldText(s->edYoudaoSecret, c.youdaoAppSecret);
            setFieldText(s->edHotwords, c.hotwords);
            s->updateCheck = c.updateCheck;

            layoutChildren();
            SetFocus(s->edDeepseek);
            return 0;
        }

        case WM_SIZE:
            layoutChildren();
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            HDC mem = CreateCompatibleDC(dc);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HBITMAP old = static_cast<HBITMAP>(SelectObject(mem, bmp));
            paint(mem);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC dc = reinterpret_cast<HDC>(wp);
            SetTextColor(dc, theme().text);
            SetBkColor(dc, theme().card);
            return reinterpret_cast<LRESULT>(s && s->cardBrush ? s->cardBrush
                                                               : GetStockObject(NULL_BRUSH));
        }

        case WM_MOUSEMOVE: {
            if (!s) return 0;
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            auto inside = [&](const RECT& r) {
                return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
            };
            int h = 0;
            if (inside(s->rcSave)) h = 1;
            else if (inside(s->rcClose)) h = 2;
            else if (inside(s->rcCheck)) h = 3;
            if (h != s->hover) {
                s->hover = h;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP:
            onMouseUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;

        case WM_KEYDOWN: {
            if (!s) return 0;
            if (wp == VK_ESCAPE) {
                s->running = false;
                DestroyWindow(hwnd);
                return 0;
            }
            if (wp == VK_RETURN && GetKeyState(VK_CONTROL) < 0) {
                doSave();
                if (!s->running) DestroyWindow(hwnd);
                return 0;
            }
            return 0;
        }

        case WM_CLOSE:
            if (s) {
                s->running = false;
                DestroyWindow(hwnd);
            }
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool showSettings(HWND owner) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kSettingsClass;
    wc.hIcon = appIcon(GetSystemMetrics(SM_CXICON), false);
    wc.hIconSm = appIcon(GetSystemMetrics(SM_CXSMICON), false);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    State st;
    g_state = &st;
    st.owner = owner;
    st.scale = Dpi::uiScale();
    st.bgBrush = CreateSolidBrush(theme().bg);
    st.cardBrush = CreateSolidBrush(theme().card);

    int cw = S(700);
    // 高度按内容算，字段增减都不会截断或重叠
    HDC probe = GetDC(nullptr);
    Layout probeLayout = computeLayout(probe, cw);
    ReleaseDC(nullptr, probe);
    int ch = probeLayout.bottom;

    // 客户区固定，外框由系统样式反推
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    DWORD exStyle = WS_EX_DLGMODALFRAME;
    RECT box{0, 0, cw, ch};
    AdjustWindowRectEx(&box, style, FALSE, exStyle);
    int ow = box.right - box.left;
    int oh = box.bottom - box.top;

    int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
    RECT orc{};
    if (owner && GetWindowRect(owner, &orc)) {
        x = orc.left + ((orc.right - orc.left) - ow) / 2;
        y = orc.top + ((orc.bottom - orc.top) - oh) / 2;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
    }
    st.hwnd = CreateWindowExW(exStyle, kSettingsClass, toWide(str::kSettings).c_str(),
                              style, x, y, ow, oh, owner, nullptr,
                              GetModuleHandleW(nullptr), nullptr);

    if (!st.hwnd) {
        DeleteObject(st.bgBrush);
        DeleteObject(st.cardBrush);
        g_state = nullptr;
        return false;
    }

    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(st.hwnd, SW_SHOW);
    UpdateWindow(st.hwnd);

    MSG msg{};
    while (st.running && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(st.hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }

    if (IsWindow(st.hwnd)) DestroyWindow(st.hwnd);
    DeleteObject(st.bgBrush);
    DeleteObject(st.cardBrush);
    g_state = nullptr;
    return st.saved;
}

}  // namespace hs

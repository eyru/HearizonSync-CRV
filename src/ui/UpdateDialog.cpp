// 新版本提示窗口：介绍更新内容，可「稍后提醒」，也可勾选「跳过这个版本」。
// 对应 Python 版 _show_update_dialog：580x580 自绘窗口，不是系统 MessageBox。
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <string>

#include "../core/Config.h"
#include "../core/Strings.h"
#include "../core/Theme.h"
#include "../core/Util.h"
#include "../mt/Translator.h"
#include "Widgets.h"

namespace hs {
namespace {

const wchar_t* kUpdateClass = L"HSCrvUpdateDialog";

struct UpdateState {
    HWND hwnd = nullptr;
    HWND owner = nullptr;
    double scale = 1.0;
    bool running = true;

    std::string tag;
    std::string body;
    std::string url;

    bool download = false;   // 点了「下载并安装」
    bool skip = false;       // 勾了「跳过这个版本」

    Button btnLater;
    Button btnGo;
    CheckBox skipCheck;
    TextView bodyView;

    HBRUSH bgBrush = nullptr;
    HBRUSH cardBrush = nullptr;
};

UpdateState* g_u = nullptr;

int U(double v) { return scaledPx(v, g_u->scale); }

void drawTextLocal(HDC dc, const std::wstring& text, const RECT& rc, COLORREF color, HFONT font,
                   UINT flags) {
    HFONT old = static_cast<HFONT>(SelectObject(dc, font));
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    RECT r = rc;
    DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &r, flags);
    SelectObject(dc, old);
}

void layoutAll(UpdateState& s, int cw, int ch) {
    const int pad = U(24);
    const int btnH = U(34);
    const int btnW = U(104);
    const int skipH = U(24);

    int btnRowY = ch - pad - btnH;
    s.btnGo.setRect(RECT{cw - pad - btnW, btnRowY, cw - pad, btnRowY + btnH});
    s.btnLater.setRect(
        RECT{cw - pad - btnW - U(8) - btnW, btnRowY, cw - pad - U(8) - btnW, btnRowY + btnH});

    int skipY = btnRowY - U(10) - skipH;
    s.skipCheck.setRect(RECT{pad, skipY, cw - pad, skipY + skipH});

    int bodyTop = U(18) + U(28) + U(4) + U(20) + U(12);
    int bodyBottom = skipY - U(10);
    if (bodyBottom < bodyTop + U(80)) bodyBottom = bodyTop + U(80);
    s.bodyView.setRect(RECT{pad, bodyTop, cw - pad, bodyBottom});
}

void paintAll(UpdateState& s, HDC dc, const RECT& rc) {
    const int cw = rc.right - rc.left;
    const int pad = U(24);

    // 背景
    FillRect(dc, &rc, s.bgBrush);

    // 标题「发现新版本 {tag}」
    SetBkMode(dc, TRANSPARENT);
    HFONT fTitle = uiFont(Dpi::fontPt(15), true);
    HFONT fSub = uiFont(Dpi::fontPt(9));
    HFONT fBody = uiFont(Dpi::fontPt(9));

    RECT titleRc{pad, U(18), cw - pad, U(18) + U(28)};
    drawTextLocal(dc, toWide("发现新版本 " + s.tag), titleRc, theme().accent2, fTitle,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // 副标题「当前 vX　→　tag」
    RECT subRc{pad, titleRc.bottom + U(4), cw - pad, titleRc.bottom + U(4) + U(20)};
    drawTextLocal(dc, toWide("当前 v" + appVersion() + "　→　" + s.tag), subRc, theme().dim, fSub,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // 正文卡片
    RECT card = s.bodyView.rect();
    InflateRect(&card, U(1), U(1));
    HBRUSH bg = static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    HPEN pen = CreatePen(PS_SOLID, 1, theme().border);
    HGDIOBJ ob = SelectObject(dc, s.cardBrush);
    HGDIOBJ op = SelectObject(dc, pen);
    RoundRect(dc, card.left, card.top, card.right, card.bottom, U(10), U(10));
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
    (void)bg;

    // 正文内容
    s.bodyView.setFont(uiFont(Dpi::fontPt(9)));
    s.bodyView.setColors(theme().text2, theme().card);
    s.bodyView.setLineHeight(U(22));
    s.bodyView.paint(dc);

    // 勾选框与按钮
    s.skipCheck.paint(dc);
    s.btnLater.paint(dc, rc);
    s.btnGo.paint(dc, rc);

    DeleteObject(fTitle);
    DeleteObject(fSub);
    DeleteObject(fBody);
}

bool hit(const RECT& r, int x, int y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

LRESULT CALLBACK UpdateProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    UpdateState* s = g_u;
    if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
        case WM_CREATE: {
            s->hwnd = hwnd;
            s->bgBrush = CreateSolidBrush(theme().bg);
            s->cardBrush = CreateSolidBrush(theme().card);
            s->btnLater.create(hwnd, 1001, "稍后提醒", 0, 0, 100, 30);
            s->btnLater.setColors(theme().card, theme().cardHover, theme().text2);
            s->btnGo.create(hwnd, 1002, "下载并安装", 0, 0, 100, 30);
            s->btnGo.setColors(theme().accent, theme().accent2, RGB(0x03, 0x21, 0x1A));
            s->skipCheck.create(hwnd, "跳过这个版本（下次不再提示）", 0, 0, 100, 24);
            s->bodyView.create(hwnd, 0, 0, 0, 0);

            s->btnLater.setOnClick([s]() {
                s->skip = s->skipCheck.checked();
                s->download = false;
                s->running = false;
                PostMessageW(s->hwnd, WM_CLOSE, 0, 0);
            });
            s->btnGo.setOnClick([s]() {
                s->skip = s->skipCheck.checked();
                s->download = true;
                s->running = false;
                PostMessageW(s->hwnd, WM_CLOSE, 0, 0);
            });

            // 正文：Markdown 转纯文本，最多 1600 字
            std::string plain = markdownToPlain(s->body, 1600);
            if (plain.empty()) plain = "（这个版本没有填写更新说明）";
            s->bodyView.append(plain);

            RECT rc;
            GetClientRect(hwnd, &rc);
            layoutAll(*s, rc.right - rc.left, rc.bottom - rc.top);
            return 0;
        }
        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            layoutAll(*s, rc.right - rc.left, rc.bottom - rc.top);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            paintAll(*s, dc, rc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            s->btnLater.onMouseMove(x, y);
            s->btnGo.onMouseMove(x, y);
            s->skipCheck.onMouseMove(x, y);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            SetCapture(hwnd);
            s->btnLater.onMouseDown(x, y);
            s->btnGo.onMouseDown(x, y);
            s->skipCheck.onMouseDown(x, y);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: {
            ReleaseCapture();
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            s->btnLater.onMouseUp(x, y);
            s->btnGo.onMouseUp(x, y);
            s->skipCheck.onMouseUp(x, y);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            POINT pt{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            ScreenToClient(hwnd, &pt);
            if (hit(s->bodyView.rect(), pt.x, pt.y)) {
                int delta = GET_WHEEL_DELTA_WPARAM(wp);
                int step = delta > 0 ? -3 : 3;
                RECT br = s->bodyView.rect();
                int maxScroll =
                    std::max(0, s->bodyView.contentHeight() - static_cast<int>(br.bottom - br.top));
                int ns = s->bodyView.scroll() + step * U(10);
                ns = std::max(0, std::min(maxScroll, ns));
                s->bodyView.setScroll(ns);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                s->download = false;
                s->skip = false;
                s->running = false;
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
            }
            return 0;
        case WM_CLOSE:
            s->running = false;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            s->running = false;
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

// 返回 true 表示用户选择「下载并安装」；outSkip 表示是否勾了跳过这个版本
bool showUpdateDialogModal(HWND owner, const std::string& tag, const std::string& body,
                           bool* outSkip) {
    if (outSkip) *outSkip = false;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = UpdateProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kUpdateClass;
    wc.hIcon = appIcon(GetSystemMetrics(SM_CXICON), false);
    wc.hIconSm = appIcon(GetSystemMetrics(SM_CXSMICON), false);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    UpdateState st;
    g_u = &st;
    st.owner = owner;
    st.scale = Dpi::uiScale();
    st.tag = tag;
    st.body = body;

    int cw = U(580);
    int ch = U(580);
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

    HWND hwnd = CreateWindowExW(exStyle, kUpdateClass, L"发现新版本", style, x, y, ow, oh, owner,
                                nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) {
        g_u = nullptr;
        return false;
    }

    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    UpdateWindow(hwnd);

    MSG msg{};
    while (st.running && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE &&
            msg.hwnd != st.hwnd) {
            PostMessageW(st.hwnd, WM_KEYDOWN, VK_ESCAPE, 0);
            continue;
        }
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    if (IsWindow(hwnd)) DestroyWindow(hwnd);

    bool ok = st.download;
    if (outSkip) *outSkip = st.skip;
    if (st.bgBrush) DeleteObject(st.bgBrush);
    if (st.cardBrush) DeleteObject(st.cardBrush);
    g_u = nullptr;
    return ok;
}

}  // namespace hs

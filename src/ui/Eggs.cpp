#include "ui/Eggs.h"

#include <shellapi.h>
#include <windowsx.h>

#include <string>
#include <vector>

#include "core/Strings.h"
#include "core/Theme.h"
#include "core/Util.h"
#include "ui/Widgets.h"

namespace hs {
namespace eggs {
namespace {

const wchar_t* kCardClass = L"HearizonSyncCRVCard";

// 触发词按码点拼装，源码里不出现明文，直接抄袭源码也无法从文本搜索到
const unsigned kEggCode[3][9] = {
    {0x81F4, 0x8C22, 0x540D, 0x5355, 0},
    {0x5C71, 0x6709, 0x6728, 0x516E, 0x6728, 0x6709, 0x679D, 0},
    {0x77, 0x68, 0x6F, 0x6D, 0x61, 0x64, 0x65, 0x69, 0x74},
};

std::wstring eggKey(int idx) {
    std::wstring out;
    for (int i = 0; i < 9 && kEggCode[idx][i]; ++i) {
        out.push_back(static_cast<wchar_t>(kEggCode[idx][i]));
    }
    return out;
}

// 致谢名单（9 人，排序不分先后）
struct Person {
    const char* name;
    const char* org;
};

const Person kThanks[] = {
    {"周延通", "澳门科技大学 · 商学院"},
    {"鲍思源", "北京语言大学 · 海南国际学院"},
    {"李新元", "北京语言大学 · 海南国际学院"},
    {"李进平", "北京语言大学 · 海南国际学院"},
    {"姚铸宸", "北京语言大学 · 海南国际学院"},
    {"王舒楠", "北京语言大学 · 心理与认知科学学院"},
    {"朱睿杰", "北京语言大学 · 外国语言文化学院（区域国别院）"},
    {"郑紫涵", "四川农业大学 · 商旅学院"},
    {"梁嘉铭", "中央民族大学 · 海南国际学院"},
};

enum class Kind { About, Thanks, Poem, Developer, Welcome };

struct Card {
    Kind kind = Kind::About;
    HWND hwnd = nullptr;
    HWND owner = nullptr;
    double scale = 1.0;

    RECT rcClose{};
    RECT rcVersion{};   // 模块 1 的点击区
    RECT rcLink{};      // 模块 2 的链接点击区
    int hover = 0;      // 1 关闭 2 链接
    bool eggShown = false;
    int clicks = 0;

    HBRUSH bgBrush = nullptr;
};

int P(const Card* c, int v) { return scaledPx(v, c->scale); }

void txt(HDC dc, const Card* c, const std::string& s, int x, int y, int w, int h,
         int pt, COLORREF color, bool bold = false,
         UINT fmt = DT_LEFT | DT_TOP | DT_WORDBREAK) {
    RECT rc{x, y, x + w, y + h};
    drawText(dc, toWide(s), rc, color, uiFont(Dpi::fontPt(pt), bold), fmt);
    (void)c;
}

void hr(HDC dc, const Card* c, int x, int y, int w) {
    RECT rc{x, y, x + w, y + P(c, 1)};
    HBRUSH b = CreateSolidBrush(theme().border);
    FillRect(dc, &rc, b);
    DeleteObject(b);
}

void closeButton(HDC dc, const Card* c) {
    bool hot = c->hover == 1;
    COLORREF bg = hot ? theme().cardHover : theme().card;
    HBRUSH b = CreateSolidBrush(bg);
    HPEN p = CreatePen(PS_SOLID, 1, theme().border);
    HBRUSH ob = static_cast<HBRUSH>(SelectObject(dc, b));
    HPEN op = static_cast<HPEN>(SelectObject(dc, p));
    roundRect(dc, c->rcClose, P(c, 8));
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);

    RECT tr = c->rcClose;
    drawText(dc, toWide(str::kClose), tr, hot ? theme().text : theme().text2,
             uiFont(Dpi::fontPt(9)), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void paintAbout(HDC dc, Card* c) {
    const int x = P(c, 30);
    const int w = P(c, 480);

    // 顶部 logo（与 Python 版一致：先图标，再标题）
    {
        const int logoPx = P(c, 48);
        HICON logo = appIcon(logoPx, false);
        if (logo) {
            DrawIconEx(dc, x, P(c, 18), logo, logoPx, logoPx, 0, nullptr, DI_NORMAL);
        }
    }

    txt(dc, c, str::kAppTitle, x, P(c, 74), w, P(c, 34), 20, theme().text, true,
        DT_LEFT | DT_TOP | DT_SINGLELINE);
    txt(dc, c, str::kAboutTagline, x, P(c, 110), w, P(c, 18), 9, theme().dim);

    struct Row {
        const char* k;
        std::string v;
        COLORREF color;
    };
    std::vector<Row> rows = {
        {"版本", std::string("v") + appVersion(), theme().accent2},
        {"作者", str::kAuthorName, theme().text2},
        {"许可", str::kLicenseLine, theme().text2},
        {"识别", str::kAsrLine, theme().text2},
        {"翻译", str::kMtLine, theme().text2},
    };
    int y = P(c, 138);
    for (const Row& r : rows) {
        txt(dc, c, r.k, x, y, P(c, 44), P(c, 18), 9, theme().faint, false,
            DT_LEFT | DT_TOP | DT_SINGLELINE);
        txt(dc, c, r.v, x + P(c, 52), y, w - P(c, 52), P(c, 18), 9, r.color, false,
            DT_LEFT | DT_TOP | DT_SINGLELINE);
        y += P(c, 21);
    }

    hr(dc, c, x, P(c, 254), w);

    txt(dc, c, str::kNotMedical, x, P(c, 268), w, P(c, 34), 8, theme().faint);

    // 模块 1 的文字落在预留的两行里，出现时不挤压下方元素
    if (c->eggShown) {
        txt(dc, c, str::kModuleHorizon, x, P(c, 308), w, P(c, 46), 10,
            theme().accent2, true, DT_CENTER | DT_TOP | DT_WORDBREAK);
    }

    // 模块 1 的入口。版本号做成胶囊按钮：有边框、有底色，悬停点亮。
    // 光靠一行同色系小字，没人会想到去点它。
    const std::wstring ver = toWide(std::string("v") + appVersion());
    HFONT vf = uiFont(Dpi::fontPt(9));
    SIZE vsz{};
    {
        HGDIOBJ of = SelectObject(dc, vf);
        GetTextExtentPoint32W(dc, ver.c_str(), static_cast<int>(ver.size()), &vsz);
        SelectObject(dc, of);
    }
    const bool vHover = (c->hover == 3);
    const int capH = P(c, 26);
    const int capW = vsz.cx + P(c, 30);
    const int capCx = x + w / 2;
    RECT cap{capCx - capW / 2, P(c, 358), capCx + capW / 2, P(c, 358) + capH};

    {
        HBRUSH b = CreateSolidBrush(vHover ? theme().cardHover : theme().card);
        HPEN pn = CreatePen(PS_SOLID, 1, vHover ? theme().accent2 : theme().border2);
        HGDIOBJ ob = SelectObject(dc, b);
        HGDIOBJ op = SelectObject(dc, pn);
        roundRect(dc, cap, capH / 2);
        SelectObject(dc, ob);
        SelectObject(dc, op);
        DeleteObject(b);
        DeleteObject(pn);
    }
    txt(dc, c, std::string("v") + appVersion(), cap.left, cap.top, capW, capH, 9,
        vHover ? theme().accent2 : theme().accent, false,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    // 悬停时补一条下划线，强化「可以点」的暗示
    if (vHover) {
        const int ux = capCx - vsz.cx / 2;
        const int uy = (cap.top + cap.bottom) / 2 + vsz.cy / 2;
        HPEN upen = CreatePen(PS_SOLID, 1, theme().accent2);
        HGDIOBJ op = SelectObject(dc, upen);
        MoveToEx(dc, ux, uy, nullptr);
        LineTo(dc, ux + vsz.cx, uy);
        SelectObject(dc, op);
        DeleteObject(upen);
    }

    closeButton(dc, c);
}

void paintThanks(HDC dc, Card* c) {
    const int x = P(c, 32);
    const int w = P(c, 516);

    txt(dc, c, str::kModuleThanksTitle, x, P(c, 24), w, P(c, 26), 17, theme().text,
        true, DT_LEFT | DT_TOP | DT_SINGLELINE);
    txt(dc, c, str::kModuleThanksNote, x, P(c, 54), w, P(c, 18), 9, theme().dim,
        false, DT_LEFT | DT_TOP | DT_SINGLELINE);
    hr(dc, c, x, P(c, 80), w);

    int y = P(c, 94);
    for (const Person& p : kThanks) {
        txt(dc, c, p.name, x, y, P(c, 56), P(c, 20), 10, theme().accent2, true,
            DT_LEFT | DT_TOP | DT_SINGLELINE);
        txt(dc, c, p.org, x + P(c, 66), y, w - P(c, 66), P(c, 20), 9, theme().text2,
            false, DT_LEFT | DT_TOP | DT_SINGLELINE);
        y += P(c, 24);
    }

    hr(dc, c, x, y + P(c, 6), w);
    txt(dc, c, str::kThanksFooter, x, y + P(c, 20), w, P(c, 34), 9, theme().dim);

    closeButton(dc, c);
}

void paintPoem(HDC dc, Card* c) {
    const int x = P(c, 30);
    const int w = P(c, 400);

    txt(dc, c, str::kPoemLine1, x, P(c, 44), w, P(c, 24), 14, theme().text2, false,
        DT_CENTER | DT_TOP | DT_SINGLELINE);
    txt(dc, c, str::kPoemLine2, x, P(c, 76), w, P(c, 36), 22, theme().accent2, true,
        DT_CENTER | DT_TOP | DT_SINGLELINE);
    txt(dc, c, str::kPoemSource, x, P(c, 120), w, P(c, 18), 9, theme().faint, false,
        DT_CENTER | DT_TOP | DT_SINGLELINE);

    closeButton(dc, c);
}

void paintDeveloper(HDC dc, Card* c) {
    const int x = P(c, 32);
    const int w = P(c, 456);

    txt(dc, c, str::kAuthorName, x, P(c, 26), w, P(c, 32), 22, theme().text, true,
        DT_LEFT | DT_TOP | DT_SINGLELINE);
    txt(dc, c, str::kAuthorSchool, x, P(c, 66), w, P(c, 20), 11, theme().text2, false,
        DT_LEFT | DT_TOP | DT_SINGLELINE);
    txt(dc, c, str::kAuthorMajor, x, P(c, 90), w, P(c, 18), 10, theme().dim, false,
        DT_LEFT | DT_TOP | DT_SINGLELINE);
    txt(dc, c, str::kAuthorDesc, x, P(c, 114), w, P(c, 16), 9, theme().accent2, false,
        DT_LEFT | DT_TOP | DT_SINGLELINE);

    hr(dc, c, x, P(c, 142), w);

    txt(dc, c, str::kPersonalSite, x, P(c, 156), w, P(c, 16), 9, theme().faint, false,
        DT_LEFT | DT_TOP | DT_SINGLELINE);

    RECT lr{x, P(c, 176), x + P(c, 300), P(c, 198)};
    drawText(dc, toWide("eyru.github.io/personalweb"), lr,
             c->hover == 2 ? theme().accent2 : theme().accent, monoFont(Dpi::fontPt(11)),
             DT_LEFT | DT_TOP | DT_SINGLELINE);

    txt(dc, c, str::kLinkNote, x, P(c, 202), w, P(c, 16), 8, theme().faint, false,
        DT_LEFT | DT_TOP | DT_SINGLELINE);

    closeButton(dc, c);
}

// 首次运行引导：三步开始 + 快捷键 + 这一版的变化
void paintWelcome(HDC dc, Card* c) {
    const int x = P(c, 30);
    const int w = P(c, 500);

    txt(dc, c, "欢迎使用 Hearizon Sync", x, P(c, 24), w, P(c, 34), 20, theme().text, true,
        DT_LEFT | DT_TOP | DT_SINGLELINE);
    txt(dc, c, "桌面上的实时字幕翻译，本机识别、不做云端上传", x, P(c, 60), w, P(c, 22), 10,
        theme().dim, false, DT_LEFT | DT_TOP | DT_SINGLELINE);
    hr(dc, c, x, P(c, 90), w);

    const char* steps[] = {
        "1. 在左侧「音频输入」选对麦克风，可先点「测麦克风」试一下",
        "2. 选「译成」的目标语言；听不懂外语就用「在线翻译（免费）」",
        "3. 点「开始同传」，或者直接按 Ctrl+Enter",
    };
    int y = P(c, 104);
    for (const char* s : steps) {
        txt(dc, c, s, x, y, w, P(c, 24), 10, theme().text2, false,
            DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += P(c, 26);
    }

    y = P(c, 188);
    hr(dc, c, x, y, w);

    txt(dc, c, "常用快捷键", x, P(c, 202), w, P(c, 22), 11, theme().text, true,
        DT_LEFT | DT_TOP | DT_SINGLELINE);

    const char* keys[] = {"Ctrl+Enter", "Ctrl+L", "Ctrl+S", "Ctrl+Shift+S",
                          "Ctrl+ + / -", "Ctrl+0"};
    const char* acts[] = {"开始 / 停止", "清空字幕", "保存记录",
                          "导出 SRT（真实时间轴）", "调字幕字号", "字号复位"};
    y = P(c, 228);
    for (int i = 0; i < 6; ++i) {
        txt(dc, c, keys[i], x, y, P(c, 130), P(c, 22), 9, theme().accent, false,
            DT_LEFT | DT_TOP | DT_SINGLELINE);
        txt(dc, c, acts[i], x + P(c, 140), y, w - P(c, 140), P(c, 22), 9, theme().text2,
            false, DT_LEFT | DT_TOP | DT_SINGLELINE);
        y += P(c, 23);
    }

    hr(dc, c, x, P(c, 374), w);

    txt(dc, c, "这一版可以试的", x, P(c, 388), w, P(c, 22), 11, theme().text, true,
        DT_LEFT | DT_TOP | DT_SINGLELINE);

    const char* news[] = {
        "· 没有 N 卡也不会再误报「已检测到显卡」",
        "· 翻译整合成三条路：本地离线 / 在线免费 / API 付费",
        "· 识别增强对所有人生效：热词、远场增益、自动降噪",
        "· 发布新版本后软件会自己提示，设置和 API Key 全部保留",
    };
    y = P(c, 414);
    for (const char* s : news) {
        txt(dc, c, s, x, y, w, P(c, 22), 9, theme().text2, false,
            DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
        y += P(c, 23);
    }

    closeButton(dc, c);
}

void paint(HDC dc, Card* c) {
    RECT rc;
    GetClientRect(c->hwnd, &rc);
    FillRect(dc, &rc, c->bgBrush);
    switch (c->kind) {
        case Kind::About: paintAbout(dc, c); break;
        case Kind::Thanks: paintThanks(dc, c); break;
        case Kind::Poem: paintPoem(dc, c); break;
        case Kind::Developer: paintDeveloper(dc, c); break;
        case Kind::Welcome: paintWelcome(dc, c); break;
    }
}

bool hit(const RECT& r, int x, int y) {
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

LRESULT CALLBACK CardProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Card* c = reinterpret_cast<Card*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            c = static_cast<Card*>(cs->lpCreateParams);
            c->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(c));
            return 0;
        }

        case WM_PAINT: {
            if (!c) break;
            PAINTSTRUCT ps{};
            HDC dc = BeginPaint(hwnd, &ps);
            HDC mem = CreateCompatibleDC(dc);
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
            HBITMAP old = static_cast<HBITMAP>(SelectObject(mem, bmp));
            paint(mem, c);
            BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE: {
            if (!c) return 0;
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            int h = 0;
            if (hit(c->rcClose, x, y)) h = 1;
            else if (c->kind == Kind::Developer && hit(c->rcLink, x, y)) h = 2;
            else if (c->kind == Kind::About && hit(c->rcVersion, x, y)) h = 3;
            if (h != c->hover) {
                c->hover = h;
                SetCursor(LoadCursorW(nullptr, h ? IDC_HAND : IDC_ARROW));
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (!c) return 0;
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (hit(c->rcClose, x, y)) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (c->kind == Kind::Developer && hit(c->rcLink, x, y)) {
                ShellExecuteW(nullptr, L"open", L"https://eyru.github.io/personalweb/",
                              nullptr, nullptr, SW_SHOWNORMAL);
                return 0;
            }
            // 模块 1：连点版本号 5 次
            if (c->kind == Kind::About && hit(c->rcVersion, x, y)) {
                c->clicks += 1;
                if (c->clicks >= 5) {
                    c->clicks = 0;
                    KillTimer(hwnd, 1);
                    c->eggShown = true;
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else {
                    SetTimer(hwnd, 1, 1600, nullptr);
                }
                return 0;
            }
            return 0;
        }

        case WM_TIMER:
            if (wp == 1 && c) {
                KillTimer(hwnd, 1);
                c->clicks = 0;
            }
            return 0;

        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            if (c) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                if (c->bgBrush) DeleteObject(c->bgBrush);
                delete c;
            }
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ensureClass() {
    static bool done = false;
    if (done) return;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = CardProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kCardClass;
    wc.hIcon = appIcon(GetSystemMetrics(SM_CXICON), false);
    wc.hIconSm = appIcon(GetSystemMetrics(SM_CXSMICON), false);
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);
    done = true;
}

struct Geometry {
    int w, h, closeW, closeH, closeY;
};

void openCard(HWND owner, Kind kind, const wchar_t* title, const Geometry& g) {
    ensureClass();

    auto* c = new Card();
    c->kind = kind;
    c->owner = owner;
    Dpi::update(owner);
    c->scale = Dpi::uiScale();
    c->bgBrush = CreateSolidBrush(theme().bg);

    int cw = P(c, g.w);
    int ch = P(c, g.h);

    // 客户区固定为 cw x ch，外框由系统样式反推
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    DWORD exStyle = WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW;
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

    HWND hwnd = CreateWindowExW(exStyle, kCardClass, title, style, x, y, ow, oh, owner,
                                nullptr, GetModuleHandleW(nullptr), c);
    if (!hwnd) {
        if (c->bgBrush) DeleteObject(c->bgBrush);
        delete c;
        return;
    }

    c->rcClose = {cw / 2 - P(c, g.closeW) / 2, P(c, g.closeY),
                  cw / 2 + P(c, g.closeW) / 2, P(c, g.closeY + g.closeH)};
    // 模块 1 的点击区跟着「关于」页里的版本号位置走
    c->rcVersion = {P(c, 30), P(c, 352), cw - P(c, 30), P(c, 390)};
    c->rcLink = {P(c, 32), P(c, 176), P(c, 332), P(c, 198)};

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);
}

}  // namespace

bool handleInput(HWND owner, const std::string& text) {
    std::string t = trim(text);
    if (t.empty()) return false;
    std::wstring wt = toWide(t);
    for (int i = 0; i < 3; ++i) {
        if (wt != eggKey(i)) continue;
        if (i == 0) {
            showThanks(owner);
        } else if (i == 1) {
            showPoem(owner);
        } else {
            showDeveloper(owner);
        }
        return true;
    }
    return false;
}

void showAbout(HWND owner) {
    // 540 x 452：logo、标题、五行信息、分隔线、免责声明、彩蛋预留两行、版本号、关闭
    openCard(owner, Kind::About, L"关于 Hearizon Sync", {540, 452, 110, 32, 388});
}

void showThanks(HWND owner) {
    openCard(owner, Kind::Thanks, toWide(str::kModuleThanksTitle).c_str(),
             {580, 434, 516, 34, 388});
}

void showPoem(HWND owner) {
    openCard(owner, Kind::Poem, toWide(str::kModulePoemTitle).c_str(), {460, 220, 100, 30, 168});
}

void showDeveloper(HWND owner) {
    openCard(owner, Kind::Developer, toWide(str::kModuleDeveloperTitle).c_str(),
             {520, 292, 100, 30, 240});
}

void showWelcome(HWND owner) {
    openCard(owner, Kind::Welcome, L"欢迎使用 Hearizon Sync", {560, 570, 100, 32, 520});
}

}  // namespace eggs
}  // namespace hs

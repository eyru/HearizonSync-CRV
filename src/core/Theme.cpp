#include "Theme.h"

#include <map>
#include <string>

#include "Util.h"

namespace hs {

const Theme& theme() {
    static Theme t;
    return t;
}

namespace {
const SubTheme kSubThemes[] = {
    {"green", "经典绿", RGB(0x8F, 0xA8, 0xA0), RGB(0x4A, 0xDE, 0x9B),
     RGB(0x11, 0x1B, 0x17), RGB(0x5E, 0x7A, 0x72), RGB(0x2A, 0x7A, 0x5E)},
    {"contrast", "高对比", RGB(0xFF, 0xFF, 0xFF), RGB(0xFF, 0xFF, 0xFF),
     RGB(0x00, 0x00, 0x00), RGB(0xBD, 0xBD, 0xBD), RGB(0xD6, 0xD6, 0xD6)},
    {"warm", "暖纸张", RGB(0xE6, 0xD7, 0xBE), RGB(0xFF, 0xC6, 0x6D),
     RGB(0x1A, 0x16, 0x11), RGB(0xA8, 0x94, 0x78), RGB(0xC7, 0x9A, 0x4E)},
};
const int kSubThemeCount = static_cast<int>(sizeof(kSubThemes) / sizeof(kSubThemes[0]));
}  // namespace

const SubTheme& subTheme(const std::string& key) {
    for (int i = 0; i < kSubThemeCount; ++i) {
        if (key == kSubThemes[i].key) return kSubThemes[i];
    }
    return kSubThemes[0];
}

const SubTheme& subThemeByIndex(int index) {
    if (index < 0 || index >= kSubThemeCount) return kSubThemes[0];
    return kSubThemes[index];
}

int subThemeIndex(const std::string& key) {
    for (int i = 0; i < kSubThemeCount; ++i) {
        if (key == kSubThemes[i].key) return i;
    }
    return 0;
}

int subThemeCount() { return kSubThemeCount; }

// ---------------------------------------------------------------- DPI
namespace {
double g_scale = 1.0;
bool g_aware = false;
}  // namespace

void Dpi::enableAwareness() {
    if (g_aware) return;
    // 优先 per-monitor v2，失败退回 system aware
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        typedef BOOL(WINAPI * SetCtxFn)(void*);
        auto setCtx = reinterpret_cast<SetCtxFn>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
        if (setCtx) {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = -4
            if (setCtx(reinterpret_cast<void*>(-4))) {
                g_aware = true;
                return;
            }
        }
        typedef BOOL(WINAPI * SetAwareFn)(void);
        auto setAware =
            reinterpret_cast<SetAwareFn>(GetProcAddress(user32, "SetProcessDPIAware"));
        if (setAware) {
            setAware();
            g_aware = true;
            return;
        }
    }
    // 最后尝试 shcore
    HMODULE shcore = LoadLibraryW(L"shcore.dll");
    if (shcore) {
        typedef HRESULT(WINAPI * SetDpiFn)(int);
        auto fn = reinterpret_cast<SetDpiFn>(GetProcAddress(shcore, "SetProcessDpiAwareness"));
        if (fn) fn(1);
        g_aware = true;
    }
}

void Dpi::update(HWND hwnd) {
    if (!hwnd) return;
    HDC dc = GetDC(hwnd);
    if (!dc) return;
    int dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(hwnd, dc);
    if (dpi <= 0) dpi = 96;
    g_scale = static_cast<double>(dpi) / 96.0;
    if (g_scale < 1.0) g_scale = 1.0;
    if (g_scale > 4.0) g_scale = 4.0;
}

double Dpi::scale() { return g_scale; }

int Dpi::sp(int px) {
    int v = static_cast<int>(px * g_scale + 0.5);
    return v < 1 ? 1 : v;
}

int Dpi::fontPt(int pt) {
    int v = static_cast<int>(pt * g_scale + 0.5);
    return v < 1 ? 1 : v;
}

// ---------------------------------------------------------------- 字体
namespace {
std::map<std::string, HFONT> g_fonts;

HFONT makeFont(const wchar_t* face, int pt, bool bold) {
    LOGFONTW lf{};
    lf.lfHeight = -MulDiv(pt, 96, 72);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcsncpy_s(lf.lfFaceName, face, _TRUNCATE);
    return CreateFontIndirectW(&lf);
}
}  // namespace

HFONT uiFont(int pt, bool bold) {
    std::string key = "ui:" + std::to_string(pt) + (bold ? ":b" : "");
    auto it = g_fonts.find(key);
    if (it != g_fonts.end()) return it->second;
    HFONT f = makeFont(L"Microsoft YaHei UI", pt, bold);
    g_fonts[key] = f;
    return f;
}

HFONT monoFont(int pt) {
    std::string key = "mono:" + std::to_string(pt);
    auto it = g_fonts.find(key);
    if (it != g_fonts.end()) return it->second;
    HFONT f = makeFont(L"Consolas", pt, false);
    g_fonts[key] = f;
    return f;
}

void clearFontCache() {
    for (auto& kv : g_fonts) DeleteObject(kv.second);
    g_fonts.clear();
}

}  // namespace hs

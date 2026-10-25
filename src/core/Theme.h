// Hearizon Sync C.R.V. - 主题配色与 DPI
#pragma once

#include <windows.h>

#include <string>

namespace hs {

// 配色（与 Python 版一致）
struct Theme {
    COLORREF bg = RGB(0x0A, 0x0F, 0x0D);
    COLORREF sidebar = RGB(0x0D, 0x15, 0x12);
    COLORREF panel = RGB(0x11, 0x1B, 0x17);
    COLORREF card = RGB(0x16, 0x24, 0x1F);
    COLORREF cardHover = RGB(0x1C, 0x2E, 0x27);
    COLORREF border = RGB(0x1F, 0x33, 0x2C);
    COLORREF border2 = RGB(0x2A, 0x44, 0x3B);

    COLORREF text = RGB(0xE8, 0xF0, 0xEC);
    COLORREF text2 = RGB(0xB8, 0xC9, 0xC2);
    COLORREF dim = RGB(0x6E, 0x88, 0x80);
    COLORREF faint = RGB(0x44, 0x59, 0x53);

    COLORREF accent = RGB(0x10, 0xA3, 0x7F);
    COLORREF accent2 = RGB(0x4A, 0xDE, 0x9B);
    COLORREF green = RGB(0x22, 0xC5, 0x5E);
    COLORREF yellow = RGB(0xEA, 0xB3, 0x08);
    COLORREF red = RGB(0xDC, 0x26, 0x26);
    COLORREF listen = RGB(0x2D, 0xD4, 0xBF);
    COLORREF srcPartial = RGB(0x5E, 0x7A, 0x72);
    COLORREF dstPartial = RGB(0x2A, 0x7A, 0x5E);
};

const Theme& theme();

// 字幕配色方案
struct SubTheme {
    std::string key;
    std::string name;
    COLORREF srcFg;
    COLORREF dstFg;
    COLORREF bg;
    COLORREF partialSrc;
    COLORREF partialDst;
};

const SubTheme& subTheme(const std::string& key);
const SubTheme& subThemeByIndex(int index);
int subThemeIndex(const std::string& key);
int subThemeCount();

// DPI
class Dpi {
public:
    static void enableAwareness();      // 进程启动时调用一次
    static void update(HWND hwnd);      // 拿到窗口后计算缩放
    static double scale();              // 1.0 = 96 DPI
    static int sp(int px);              // 按缩放换算像素
    static int fontPt(int pt);          // 字号换算
};

// 字体
HFONT uiFont(int pt, bool bold = false);
HFONT monoFont(int pt);
void clearFontCache();

}  // namespace hs

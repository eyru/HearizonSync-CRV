# -*- coding: utf-8 -*-
"""关于页的版本号原来只是一行淡色小字，虽然加了 accent 色和下划线，
但用户还是找不到能点的东西。改成明显的胶囊按钮：
有边框、有底色，悬停时边框和文字一起点亮。
彩蛋本身仍然不写文案，只是把「这里可以点」这个视觉信号给足。
"""
import io

ROOT = r"D:\Audivox事务\HearizonSync C.R.V"
P = ROOT + r"\src\ui\Eggs.cpp"
s = io.open(P, encoding="utf-8").read()

OLD = '''    // 模块 1 的入口。这里用可点链接的视觉语言（强调色 + 下划线 + 悬停手型），
    // 否则版本号只是一行淡字，没人会想到去点它。
    RECT vr{x, P(c, 362), x + w, P(c, 378)};
    const std::wstring ver = toWide(std::string("v") + appVersion());
    const COLORREF vc = theme().accent;
    HFONT vf = uiFont(Dpi::fontPt(8));
    drawText(dc, ver, vr, vc, vf, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SIZE vsz{};
    {
        HGDIOBJ of = SelectObject(dc, vf);
        GetTextExtentPoint32W(dc, ver.c_str(), static_cast<int>(ver.size()), &vsz);
        SelectObject(dc, of);
    }
    const int ux = (vr.left + vr.right - vsz.cx) / 2;
    const int uy = (vr.top + vr.bottom) / 2 + vsz.cy / 2 - P(c, 1);
    HPEN upen = CreatePen(PS_SOLID, (std::max)(1, P(c, 1) / 2), vc);
    {
        HGDIOBJ op = SelectObject(dc, upen);
        MoveToEx(dc, ux, uy, nullptr);
        LineTo(dc, ux + vsz.cx, uy);
        SelectObject(dc, op);
    }
    DeleteObject(upen);
'''

NEW = '''    // 模块 1 的入口。版本号做成胶囊按钮：有边框、有底色，悬停点亮。
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
'''

assert OLD in s, "版本号绘制段不匹配"
s = s.replace(OLD, NEW, 1)

# 点击区跟着胶囊一起放大
OLD_RC = "    c->rcVersion = {P(c, 30), P(c, 358), cw - P(c, 30), P(c, 382)};"
NEW_RC = "    c->rcVersion = {P(c, 30), P(c, 352), cw - P(c, 30), P(c, 390)};"
if OLD_RC in s:
    s = s.replace(OLD_RC, NEW_RC, 1)
    print("rcVersion ok")
else:
    print("rcVersion 未找到，保持原样")

io.open(P, "w", encoding="utf-8", newline="").write(s)
print("Eggs.cpp ok")

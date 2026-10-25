# -*- coding: utf-8 -*-
"""给设置页补上「虚拟声卡」与「GPU 加速说明」两个区块（Python 版有，C++ 版漏了）。"""
import io
import os

P = "D:/Audivox事务/HearizonSync C.R.V/src/ui/SettingsWindow.cpp"
s = io.open(P, encoding="utf-8").read()

# ---------------------------------------------------------------- 1) include
if '#include "core/Gpu.h"' not in s:
    s = s.replace('#include "ui/Eggs.h"',
                  '#include "audio/AudioCapture.h"\n#include "core/Gpu.h"\n#include "ui/Eggs.h"', 1)

# ---------------------------------------------------------------- 2) helper
helper = '''
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

// 唯一的布局来源'''
s = s.replace("// 唯一的布局来源", helper, 1)

# ---------------------------------------------------------------- 3) Layout 字段
s = s.replace(
    "    FieldRects f[5];\n    RECT check{}, save{}, close{};",
    "    FieldRects f[5];\n"
    "    RECT cableTitle{}, cableHint{}, cableState{};\n"
    "    RECT gpuTitle{}, gpuState{}, gpuHint{};\n"
    "    RECT check{}, save{}, close{};", 1)

# ---------------------------------------------------------------- 4) computeLayout
anchor = "    L.check = {pad, y, cw - pad, y + S(22)};"
extra = '''    // ---- 虚拟声卡（网课 / 系统声音）----
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

'''
assert anchor in s
s = s.replace(anchor, extra + anchor, 1)

# ---------------------------------------------------------------- 5) paint
anchor2 = "    drawCheck(dc, s->rcCheck, str::kUpdateCheck, s->updateCheck, s->hover == 3);"
paint_extra = '''    // ---- 虚拟声卡 ----
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

'''
assert anchor2 in s
s = s.replace(anchor2, paint_extra + anchor2, 1)

io.open(P, "w", encoding="utf-8", newline="\n").write(s)
print("SettingsWindow.cpp patched")

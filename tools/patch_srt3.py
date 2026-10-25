# -*- coding: utf-8 -*-
"""重写 saveRecord / exportSrt，改为真实时间轴 + 时间戳文件名 + 可写目录。"""
import io

ROOT = "D:/Audivox事务/HearizonSync C.R.V/"

# ------------------------------------------------ 修 include 顺序
p = ROOT + "src/core/Util.cpp"
s = io.open(p, encoding="utf-8").read()
s = s.replace('#include "Util.h"\n\n#include <shlobj.h>\n#include <vector>',
              '#include "Util.h"', 1)
s = s.replace("#include <windows.h>", "#include <windows.h>\n#include <shlobj.h>", 1)
io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("include order fixed")

# ------------------------------------------------ Dialogs.cpp
p = ROOT + "src/ui/Dialogs.cpp"
s = io.open(p, encoding="utf-8").read()

# 1) askSavePath 支持初始目录
old = """std::string askSavePath(HWND owner, const wchar_t* filter, const wchar_t* defaultName) {
    wchar_t path[MAX_PATH * 2] = {0};
    wcscpy_s(path, defaultName);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = static_cast<DWORD>(std::size(path));
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return std::string();
    return toUtf8(path);
}"""
new = """std::string askSavePath(HWND owner, const wchar_t* filter, const std::wstring& defaultName,
                        const std::wstring& initialDir = L"") {
    wchar_t path[MAX_PATH * 2] = {0};
    wcscpy_s(path, defaultName.c_str());
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = static_cast<DWORD>(std::size(path));
    // 初始目录：让用户一打开就是在「文档\\Hearizon Sync」
    if (!initialDir.empty()) ofn.lpstrInitialDir = initialDir.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return std::string();
    return toUtf8(path);
}"""
assert old in s, "askSavePath"
s = s.replace(old, new, 1)

# 2) saveRecord
old_start = s.index("void MainWindow::saveRecord() {")
old_end = s.index("void MainWindow::exportSrt() {")
new_record = '''void MainWindow::saveRecord() {
    std::string dir = outputDir();
    std::wstring name = toWide("同传记录_" + fileTimestamp() + ".txt");
    std::string path = askSavePath(hwnd_, L"文本文件\\0*.txt\\0所有文件\\0*.*\\0", name,
                                   toWide(dir));
    if (path.empty()) return;

    std::string content;
    content += "Hearizon Sync 同传记录\\n";
    content += "版本 " + appVersion() + "\\n";
    content += "导出时间：" + nowDateString() + " " + nowTimeString() + "\\n";
    content += "==============\\n\\n";
    content += "原文：\\n" + srcView_.text() + "\\n\\n";
    content += "译文：\\n" + dstView_.text() + "\\n";
    if (writeTextFile(path, content)) {
        setStatus("已保存：" + path, theme().green);
    } else {
        setStatus("保存失败", theme().red);
    }
}

'''
s = s[:old_start] + new_record + s[old_end:]

# 3) exportSrt —— 真实时间轴
old_start = s.index("void MainWindow::exportSrt() {")
old_end = s.index("void MainWindow::clearSubtitles() {")
new_srt = '''void MainWindow::exportSrt() {
    // 与 Python 版一致：优先用每句的真实起止时间；没有实时记录时按行估算
    struct Item {
        double start, endd;
        std::string text;
    };
    std::vector<Item> items;
    for (const Segment& seg : segments_) {
        std::string src = trim(seg.src);
        std::string dst = trim(seg.dst);
        if (src.empty() && dst.empty()) continue;
        Item it;
        it.start = seg.start;
        it.endd = seg.end;
        it.text = dst.empty() ? src : dst;
        items.push_back(it);
    }

    if (items.empty()) {
        // 兜底：只用字幕框内容，按每行 3 秒估算
        std::string trans = trim(dstView_.text());
        std::string raw = trim(srcView_.text());
        if (trans.empty() && raw.empty()) {
            MessageBoxW(hwnd_, L"还没有字幕内容", L"SRT 导出", MB_OK | MB_ICONINFORMATION);
            return;
        }
        const std::string& base = trans.empty() ? raw : trans;
        size_t pos = 0;
        int i = 0;
        while (pos <= base.size()) {
            size_t nl = base.find('\\n', pos);
            std::string line = nl == std::string::npos ? base.substr(pos)
                                                       : base.substr(pos, nl - pos);
            line = trim(line);
            if (!line.empty()) {
                Item it;
                it.start = i * 3.0;
                it.endd = (i + 1) * 3.0;
                it.text = line;
                items.push_back(it);
                ++i;
            }
            if (nl == std::string::npos) break;
            pos = nl + 1;
        }
        if (items.empty()) {
            MessageBoxW(hwnd_, L"还没有字幕内容", L"SRT 导出", MB_OK | MB_ICONINFORMATION);
            return;
        }
    }

    // 时间轴换算成相对会话开始，并保证相邻句不重叠
    double t0 = items.front().start;
    std::string content;
    for (size_t i = 0; i < items.size(); ++i) {
        double st = items[i].start - t0;
        if (st < 0) st = 0;
        double en = items[i].endd - t0;
        if (en < st + 0.6) en = st + 0.6;
        if (i + 1 < items.size()) {
            double nxt = items[i + 1].start - t0;
            if (nxt < 0) nxt = 0;
            if (nxt > st + 0.2 && en > nxt - 0.02) en = nxt - 0.02;
        }
        if (en <= st) en = st + 0.6;

        std::string text = items[i].text;
        for (char& c : text) {
            if (c == '\\n' || c == '\\r') c = ' ';
        }
        content += std::to_string(i + 1) + "\\n";
        content += fmtSrtTime(st) + " --> " + fmtSrtTime(en) + "\\n";
        content += text + "\\n\\n";
    }

    std::string dir = outputDir();
    std::wstring name = toWide("同传字幕_" + fileTimestamp() + ".srt");
    std::string path = askSavePath(hwnd_, L"SRT 字幕\\0*.srt\\0所有文件\\0*.*\\0", name,
                                   toWide(dir));
    if (path.empty()) return;

    if (writeTextFile(path, content)) {
        MessageBoxW(hwnd_, toWide("SRT 已导出：\\n" + path).c_str(), L"SRT 已导出",
                    MB_OK | MB_ICONINFORMATION);
        setStatus("已导出 SRT：" + path, theme().green);
    } else {
        MessageBoxW(hwnd_, L"导出失败：没有写入权限", L"导出失败", MB_OK | MB_ICONWARNING);
        setStatus("导出失败", theme().red);
    }
}

'''
s = s[:old_start] + new_srt + s[old_end:]

io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("Dialogs.cpp ok")

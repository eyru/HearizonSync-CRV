# -*- coding: utf-8 -*-
"""对齐 Python 版：真实时间轴 SRT、带时间戳的文件名、可写输出目录。"""
import io

ROOT = "D:/Audivox事务/HearizonSync C.R.V/"

# ============================================================ 1) Util.h 声明
p = ROOT + "src/core/Util.h"
s = io.open(p, encoding="utf-8").read()
if "fileTimestamp" not in s:
    s = s.replace(
        "std::string nowDateString();          // YYYY-MM-DD",
        """std::string nowDateString();          // YYYY-MM-DD
std::string fileTimestamp();          // YYYYmmdd_HHMMSS（文件名用）

// 保存/导出目录：优先「文档\\Hearizon Sync」，逐个探测可写位置，
// 保证装在 Program Files 时也能落地（对应 Python 版 _output_dir()）。
std::string outputDir();

// 秒 -> SRT 时间戳 HH:MM:SS,mmm
std::string fmtSrtTime(double seconds);""", 1)
    io.open(p, "w", encoding="utf-8", newline="\n").write(s)
    print("Util.h ok")

# ============================================================ 2) Util.cpp 实现
p = ROOT + "src/core/Util.cpp"
s = io.open(p, encoding="utf-8").read()
if "fileTimestamp" not in s:
    impl = '''
std::string fileTimestamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d", st.wYear, st.wMonth,
                  st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

std::string outputDir() {
    // 与 Python 版一致的候选顺序：文档 / 桌面 / 用户目录 / 临时目录
    const char* kSub = "Hearizon Sync";
    std::vector<std::string> candidates;

    wchar_t docs[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, 0x0005 /*CSIDL_PERSONAL*/ | 0x4000 /*PER_USER*/,
                                   nullptr, 0, docs))) {
        candidates.push_back(joinPath(toUtf8(docs), kSub));
    }
    wchar_t desktop[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, 0x0010 /*CSIDL_DESKTOPDIRECTORY*/ | 0x4000, nullptr,
                                   0, desktop))) {
        candidates.push_back(joinPath(toUtf8(desktop), kSub));
    }
    candidates.push_back(joinPath(userHome(), kSub));
    candidates.push_back(joinPath(tempDir(), kSub));

    for (const std::string& dir : candidates) {
        if (dirExists(dir)) return dir;
        std::wstring w = toWide(dir);
        // 逐级创建（Documents 一定存在，这里只差最后一级）
        if (CreateDirectoryW(w.c_str(), nullptr)) return dir;
        if (GetLastError() == ERROR_ALREADY_EXISTS) return dir;
    }
    return userHome();
}

std::string fmtSrtTime(double seconds) {
    if (seconds < 0) seconds = 0;
    long long total = static_cast<long long>(seconds * 1000.0 + 0.5);
    long long ms = total % 1000;
    long long sec = (total / 1000) % 60;
    long long min = (total / 60000) % 60;
    long long hour = total / 3600000;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld,%03lld", hour, min, sec, ms);
    return buf;
}

}  // namespace hs
'''
    # 替换文件末尾的 namespace 结束
    idx = s.rfind("}  // namespace hs")
    assert idx > 0
    s = s[:idx] + impl
    # 头部加依赖
    if "#include <shlobj.h>" not in s:
        s = s.replace('#include "Util.h"', '#include "Util.h"\n\n#include <shlobj.h>\n#include <vector>', 1)
    io.open(p, "w", encoding="utf-8", newline="\n").write(s)
    print("Util.cpp ok")

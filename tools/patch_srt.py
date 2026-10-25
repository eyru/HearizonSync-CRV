# -*- coding: utf-8 -*-
"""对齐 Python 版：真实时间轴 SRT 导出、带时间戳的文件名、可写输出目录。"""
import io

BASE = "D:/Audivox事务/HearizonSync C.R.V/src/ui/"

# ============================================================ 1) MainWindow.h
p = BASE + "MainWindow.h"
s = io.open(p, encoding="utf-8").read()

if "struct Segment" not in s:
    s = s.replace(
        "// 界面消息\nstruct UiMessage {",
        """// SRT 用的真实时间轴记录（每句的起止时间 + 原文/译文）
struct Segment {
    double start = 0.0;   // 相对会话开始的秒数
    double end = 0.0;
    std::string src;
    std::string dst;
};

// 界面消息
struct UiMessage {""", 1)

    s = s.replace(
        "    // GPU 探测（后台线程，结果经 WM_APP+2 回到 UI 线程）",
        """    // 真实时间轴（导出 SRT 用）
    std::vector<Segment> segments_;
    double sessionStart_ = 0.0;

    // GPU 探测（后台线程，结果经 WM_APP+2 回到 UI 线程）""", 1)

io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("MainWindow.h ok")

# ============================================================ 2) MainWindow.cpp
p = BASE + "MainWindow.cpp"
s = io.open(p, encoding="utf-8").read()

# 2a) 开始会话时清空时间轴
old = """    startTick_ = GetTickCount();
    sentences_ = charsIn_ = charsOut_ = 0;"""
new = """    startTick_ = GetTickCount();
    sentences_ = charsIn_ = charsOut_ = 0;
    segments_.clear();
    sessionStart_ = nowSeconds();"""
assert old in s, "startCapture anchor"
s = s.replace(old, new, 1)

# 2b) 识别出一句时记录起始时间（音频片段时长≈说话时长）
old = """    std::string ts = nowTimeString();
    UiMessage srcMsg;"""
new = """    std::string ts = nowTimeString();

    // 真实时间轴：音频片段时长 ≈ 说话时长，宁可早一点也不要晚
    Segment seg;
    seg.start = nowSeconds() - static_cast<double>(audio.size()) / kSampleRate;
    seg.end = nowSeconds();
    seg.src = text;
    segments_.push_back(seg);

    UiMessage srcMsg;"""
assert old in s, "onSentence anchor"
s = s.replace(old, new, 1)

# 2c) 译文落地时填进最后一条 segment
old = """        case UiMessage::Kind::DstEnd: {
            std::string line = msg.s1;
            if (!msg.s2.empty()) line += "    — " + msg.s2;
            dstView_.appendLine(line);"""
new = """        case UiMessage::Kind::DstEnd: {
            std::string line = msg.s1;
            if (!msg.s2.empty()) line += "    — " + msg.s2;
            dstView_.appendLine(line);
            if (!segments_.empty() && segments_.back().dst.empty()) {
                segments_.back().dst = msg.s1;
                segments_.back().end = nowSeconds();
            }"""
assert old in s, "DstEnd anchor"
s = s.replace(old, new, 1)

io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("MainWindow.cpp ok")

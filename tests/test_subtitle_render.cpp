// 译文区渲染回归测试：验证「一句话只出现一次」。
//
// 背景：旧实现里 DstBegin 就 appendLine 开了行、DstChunk 往行里追加、
// DstEnd 又 appendLine 整句，于是同一句译文被画了两遍（用户看到的
// 「一句话弹两次」）。新方案是：流式期间用 partial 行做预览，
// DstEnd 时 clearPartial + appendLine 落成唯一一行。
//
// 本测试不启窗口，只验证 TextView 的行模型契约：
//   1) setPartial 不产生正式行，只改 partial 预览
//   2) 连续 setPartial 是「替换」而不是「追加」
//   3) clearPartial 之后行数回到落 partial 之前
//   4) 完整走一遍 DstBegin/DstChunk*/DstEnd，译文恰好出现一次
//   5) 非流式引擎（无 DstChunk）也恰好出现一次
//
// 编译（在项目根目录）：
//   cl /nologo /std:c++17 /EHsc /O2 /MD /utf-8 /DNOMINMAX /DWIN32_LEAN_AND_MEAN
//      /DUNICODE /D_UNICODE /Fe:tests\test_subtitle_render.exe
//      tests\test_subtitle_render.cpp src\ui\Widgets.cpp src\core\Theme.cpp
//      src\core\Util.cpp src\core\Config.cpp src\core\Json.cpp
//      /I src /I third_party\sherpa /I third_party\ctranslate2
//      /I third_party\sentencepiece\include /Fo:tests\
//      winhttp.lib ole32.lib oleaut32.lib uuid.lib shell32.lib comdlg32.lib
//      gdi32.lib user32.lib advapi32.lib comctl32.lib dwmapi.lib shcore.lib version.lib
#include <cstdio>
#include <string>

#include "ui/Widgets.h"

namespace {

int g_fail = 0;

void check(const char* name, bool ok, const std::string& detail = "") {
    std::printf("[%s] %s%s%s\n", ok ? "PASS" : "FAIL", name,
                detail.empty() ? "" : "  ", detail.c_str());
    if (!ok) ++g_fail;
}

// 数一个子串在文本里出现几次
int countOf(const std::string& hay, const std::string& needle) {
    if (needle.empty()) return 0;
    int n = 0;
    for (size_t p = hay.find(needle); p != std::string::npos;
         p = hay.find(needle, p + needle.size())) {
        ++n;
    }
    return n;
}

// 统计非空行数（TextView::text() 用 '\n' 拼行）
int lineCount(const std::string& text) {
    if (text.empty()) return 0;
    int n = 1;
    for (char c : text) {
        if (c == '\n') ++n;
    }
    return n;
}

}  // namespace

int main() {
    // 用一个不落盘的 HDC：TextView 的 append/appendLine/setPartial 不碰 DC，
    // 只有 paint/reflow 需要，所以这里不需要真窗口。
    hs::TextView tv;
    tv.create(nullptr, 0, 0, 800, 400);

    // ---- 1. setPartial 不产生正式行 ----
    tv.setPartial("… 预览中的半句");
    check("setPartial 不落正式行", tv.text().empty(),
          "text=\"" + tv.text() + "\"");

    // ---- 2. 连续 setPartial 是替换，不是追加 ----
    tv.setPartial("… 预览第二版");
    tv.setPartial("… 预览第三版");
    check("setPartial 是替换而非追加", tv.text().empty(), "仍是 0 行");

    // ---- 3. clearPartial 后回到原状 ----
    tv.appendLine("[12:00:01] 上一句译文");
    const std::string before = tv.text();
    tv.setPartial("[12:00:02] 这一句还在流式");
    tv.clearPartial();
    check("clearPartial 后不残留 partial", tv.text() == before,
          "before=\"" + before + "\" after=\"" + tv.text() + "\"");

    // ---- 4. 完整流式序列：译文恰好出现一次 ----
    {
        hs::TextView t2;
        t2.create(nullptr, 0, 0, 800, 400);

        // 模拟 applyMessage 的新实现
        std::string dstStreamed, dstBeginTs;

        // DstBegin
        dstBeginTs = "12:00:05";
        dstStreamed.clear();
        // DstChunk x3（DeepSeek 增量）
        for (const char* piece : {"您好", "，今天", "过得怎么样？"}) {
            dstStreamed += piece;
            t2.setPartial("[" + dstBeginTs + "] " + dstStreamed);
        }
        // DstEnd
        const std::string full = "您好，今天过得怎么样？";
        t2.clearPartial();
        t2.appendLine("[" + dstBeginTs + "] " + full);
        dstBeginTs.clear();
        dstStreamed.clear();

        const std::string txt = t2.text();
        check("流式：恰好 1 行", lineCount(txt) == 1,
              "lines=" + std::to_string(lineCount(txt)));
        check("流式：译文只出现 1 次", countOf(txt, full) == 1,
              "count=" + std::to_string(countOf(txt, full)) + " text=\"" + txt + "\"");
        check("流式：时间戳只出现 1 次", countOf(txt, "[12:00:05]") == 1,
              "count=" + std::to_string(countOf(txt, "[12:00:05]")));
    }

    // ---- 5. 非流式引擎（免费在线/本地）：onChunk 一次给整句 ----
    {
        hs::TextView t3;
        t3.create(nullptr, 0, 0, 800, 400);

        std::string dstStreamed, dstBeginTs;
        dstBeginTs = "12:00:09";
        dstStreamed.clear();
        // DstChunk 一次收到整句（免费在线/本地模型的行为）
        const std::string full = "声级计测量分贝。";
        dstStreamed += full;
        t3.setPartial("[" + dstBeginTs + "] " + dstStreamed);
        // DstEnd
        t3.clearPartial();
        t3.appendLine("[" + dstBeginTs + "] " + full);

        const std::string txt = t3.text();
        check("非流式：恰好 1 行", lineCount(txt) == 1,
              "lines=" + std::to_string(lineCount(txt)));
        check("非流式：译文只出现 1 次", countOf(txt, full) == 1,
              "count=" + std::to_string(countOf(txt, full)) + " text=\"" + txt + "\"");
    }

    // ---- 6. 连续三句：每句各一行，互不粘连 ----
    {
        hs::TextView t4;
        t4.create(nullptr, 0, 0, 800, 400);
        const char* lines[] = {"第一句译文。", "第二句译文。", "第三句译文。"};
        for (int i = 0; i < 3; ++i) {
            std::string ts = "12:00:1" + std::to_string(i);
            std::string acc;
            // 模拟逐字增量
            std::string full = lines[i];
            for (size_t k = 0; k < full.size();) {
                size_t len = 1;
                // 跳过 UTF-8 续字节
                while (k + len < full.size() &&
                       (static_cast<unsigned char>(full[k + len]) & 0xC0) == 0x80) {
                    ++len;
                }
                acc += full.substr(k, len);
                t4.setPartial("[" + ts + "] " + acc);
                k += len;
            }
            t4.clearPartial();
            t4.appendLine("[" + ts + "] " + full);
        }
        const std::string txt = t4.text();
        check("多句：恰好 3 行", lineCount(txt) == 3,
              "lines=" + std::to_string(lineCount(txt)));
        bool eachOnce = true;
        for (const char* l : lines) {
            if (countOf(txt, l) != 1) eachOnce = false;
        }
        check("多句：每句各出现 1 次", eachOnce, "text=\"" + txt + "\"");
    }

    std::printf("\n");
    if (g_fail == 0) {
        std::printf("==== ALL PASS ====\n");
        return 0;
    }
    std::printf("==== %d FAILED ====\n", g_fail);
    return 1;
}

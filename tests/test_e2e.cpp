// 端到端联调：录音（或合成音频）→ 识别 → 翻译
#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

#include "asr/Recognizer.h"
#include "core/Config.h"
#include "core/Util.h"
#include "mt/Translator.h"

static int fails = 0, oks = 0;
static void check(const char* n, bool c, const std::string& e = "") {
    if (c) {
        ++oks;
        std::printf("[PASS] %s\n", n);
    } else {
        ++fails;
        std::printf("[FAIL] %s %s\n", n, e.c_str());
    }
}

int main() {
    hs::loadConfig();
    hs::Config& cfg = hs::config();

    // 1) 模型路径
    std::string svModel = hs::senseVoiceModelPath();
    std::string opusDir = hs::opusModelDir();
    check("SenseVoice 模型存在", hs::fileExists(svModel), svModel);
    check("opus 模型目录存在", hs::dirExists(opusDir), opusDir);

    // 2) 识别器加载
    hs::Recognizer rec;
    rec.setStatusCallback([](const std::string& s) {
        std::printf("      [asr] %s\n", s.c_str());
    });
    std::string err;
    bool loaded = rec.load(&err);
    check("识别器加载成功", loaded, err);

    // 3) 识别一段合成音频（正弦波，验证链路不崩）
    std::vector<float> audio(16000);
    for (int i = 0; i < 16000; ++i) {
        audio[i] = 0.2f * sinf(2.0f * 3.14159265f * 440.0f * i / 16000.0f);
    }
    hs::AsrResult r = rec.transcribe(audio, &err);
    check("识别调用成功（不崩）", true, "");
    std::printf("      识别结果: '%s' lang=%s\n", r.text.c_str(), r.lang.c_str());

    // 4) 本地翻译
    hs::LocalTranslator local;
    check("本地翻译模型可用", local.available());
    bool lt = local.load(&err);
    check("本地翻译加载", lt, err);
    if (lt) {
        std::string out = local.translate(
            "Can you hear the difference between these two tones?", &err);
        check("本地翻译输出非空", !out.empty(), err);
        std::printf("      译文: %s\n", out.c_str());
    }

    // 5) 免费在线翻译（真实网络）
    cfg.translatorMode = hs::TR_FREE;
    hs::Translator tr;
    hs::MtResult mt = tr.translate("The sound level meter measures sound pressure level.",
                                   "中文", "en");
    check("免费在线翻译成功", mt.ok(), mt.text);
    std::printf("      免费译文: %s  [%s]\n", mt.text.c_str(), mt.provider.c_str());

    // 6) 本地模式分发
    cfg.translatorMode = hs::TR_LOCAL;
    hs::MtResult mt2 = tr.translate("Please sit down and listen carefully.", "中文", "en");
    check("本地模式分发成功", mt2.ok(), mt2.text);
    std::printf("      本地译文: %s  [%s]\n", mt2.text.c_str(), mt2.provider.c_str());

    // 7) 文本工具
    check("纯标点被识别为无实义", !hs::isMeaningful("。。。"));
    check("正常文本有实义", hs::isMeaningful("你好"));

    std::printf("\n==== %d passed, %d failed ====\n", oks, fails);
    return fails ? 1 : 0;
}

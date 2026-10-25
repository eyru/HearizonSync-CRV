#include "../src/core/Config.h"
#include "../src/core/Util.h"
#include <cstdio>
#include <string>

static int fails = 0, oks = 0;
static void check(const char* n, bool c, const std::string& e = "") {
    if (c) { ++oks; std::printf("[PASS] %s\n", n); }
    else { ++fails; std::printf("[FAIL] %s %s\n", n, e.c_str()); }
}

int main() {
    hs::Config& c = hs::config();

    // 默认值
    check("default mode free", std::string(hs::TR_FREE) == "在线翻译（免费）");
    check("three modes distinct",
          std::string(hs::TR_LOCAL) != hs::TR_FREE &&
          std::string(hs::TR_FREE) != hs::TR_API);
    check("input langs count", hs::INPUT_LANG_NAMES.size() == 10);
    check("output langs count", hs::OUTPUT_LANG_NAMES.size() == 8);

    // 语言矩阵
    check("local input limited",
          hs::modeInputLangs(hs::TR_LOCAL).size() == 2);
    check("local output chinese only",
          hs::modeOutputLangs(hs::TR_LOCAL).size() == 1 &&
          hs::modeOutputLangs(hs::TR_LOCAL)[0] == "中文");
    check("free all inputs", hs::modeInputLangs(hs::TR_FREE).size() == 10);
    check("api all outputs", hs::modeOutputLangs(hs::TR_API).size() == 8);

    // 代码映射
    check("outputCode 中文", hs::outputCode("中文") == "zh-CHS");
    check("outputCode English", hs::outputCode("English") == "en");
    check("outputCode 日本語", hs::outputCode("日本語") == "ja");
    std::string sv, wh;
    hs::inputCodes("日语", &sv, &wh);
    check("inputCodes 日语", sv == "ja" && wh == "ja", sv + "/" + wh);
    hs::inputCodes("德语", &sv, &wh);
    check("inputCodes 德语 sv empty", sv.empty() && wh == "de", sv + "/" + wh);
    check("langDisplayName zh", hs::langDisplayName("zh") == "中文");

    // 版本比较
    check("ver equal", hs::compareVersion("2.5.6", "v2.5.6") == 0);
    check("ver newer", hs::compareVersion("v2.6.0", "2.5.6") > 0);
    check("ver older", hs::compareVersion("2.5.5", "2.5.6") < 0);
    check("ver 2.5.10 > 2.5.9", hs::compareVersion("2.5.10", "2.5.9") > 0);

    // 热词解析
    c.hotwords = "深度学习，神经网络,Transformer";
    auto hw = hs::hotwordsList();
    check("hotwords parsed", hw.size() >= 2, std::to_string(hw.size()));
    for (auto& w : hw) std::printf("      hotword: %s\n", w.c_str());

    c.hotwords = "";
    check("hotwords empty", hs::hotwordsList().empty());

    // 保存/读取
    c.translatorMode = hs::TR_API;
    c.apiEngine = hs::API_ENGINE_YOUDAO;
    c.deepseekKey = "sk-test-123";
    c.subTheme = "warm";
    c.hotwords = "测试词";
    bool saved = hs::saveConfig();
    check("save config", saved, hs::configPath());

    // 重置后重新加载
    c.translatorMode = "";
    c.deepseekKey = "";
    hs::loadConfig();
    check("reload mode", c.translatorMode == hs::TR_API, c.translatorMode);
    check("reload engine", c.apiEngine == hs::API_ENGINE_YOUDAO, c.apiEngine);
    check("reload key", c.deepseekKey == "sk-test-123", c.deepseekKey);
    check("reload theme", c.subTheme == "warm", c.subTheme);
    check("reload hotwords", c.hotwords == "测试词", c.hotwords);

    std::printf("\n==== %d passed, %d failed ====\n", oks, fails);
    return fails ? 1 : 0;
}

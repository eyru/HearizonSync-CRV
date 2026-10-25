#include "../src/core/Http.h"
#include "../src/core/Json.h"
#include "../src/core/Util.h"
#include <cstdio>
#include <string>

static int fails = 0, oks = 0;
static void check(const char* n, bool c, const std::string& e = "") {
    if (c) { ++oks; std::printf("[PASS] %s\n", n); }
    else { ++fails; std::printf("[FAIL] %s %s\n", n, e.c_str()); }
}

int main() {
    // 1) 真实 HTTPS GET（GitHub API，验证 TLS 与解析）
    hs::HttpResult r = hs::httpGet(
        "https://api.github.com/repos/eyru/hearizon-sync/releases/latest",
        {{"User-Agent", "HearizonSync-Test"}, {"Accept", "application/vnd.github+json"}},
        30);
    check("https get ok", r.ok(), "status=" + std::to_string(r.status) + " " + r.error);
    if (r.ok()) {
        hs::Json j = hs::Json::parse(r.body);
        check("json parse", j.isObject());
        std::string tag = j.getString("tag_name");
        check("has tag_name", !tag.empty(), tag);
        std::printf("      latest tag = %s\n", tag.c_str());
    }

    // 2) 404 处理
    hs::HttpResult r2 = hs::httpGet(
        "https://api.github.com/repos/eyru/this-repo-does-not-exist-xyz", {}, 20);
    check("404 detected", r2.status == 404, std::to_string(r2.status));

    // 3) 坏 URL
    hs::HttpResult r3 = hs::httpGet("not-a-url");
    check("bad url fails", r3.failed() && !r3.error.empty(), r3.error);

    // 4) MyMemory 真实翻译
    hs::HttpResult r4 = hs::httpGet(
        "https://api.mymemory.translated.net/get?q=" + hs::urlEncode("hello world") +
        "&langpair=" + hs::urlEncode("en|zh-CN"), {}, 20);
    check("mymemory ok", r4.ok(), std::to_string(r4.status));
    if (r4.ok()) {
        hs::Json j = hs::Json::parse(r4.body);
        const hs::Json* rd = j.find("responseData");
        std::string txt = rd ? rd->getString("translatedText") : "";
        check("mymemory translated", !txt.empty(), txt);
        std::printf("      translation = %s\n", txt.c_str());
    }

    // 5) 有道免费接口（仅译成中文）
    hs::HttpResult r5 = hs::httpGet(
        "https://aidemo.youdao.com/trans?q=" + hs::urlEncode("hello world") +
        "&from=auto&to=zh-CHS", {{"Referer", "https://ai.youdao.com/"}}, 20);
    check("youdao ok", r5.ok(), std::to_string(r5.status));
    if (r5.ok()) {
        hs::Json j = hs::Json::parse(r5.body);
        check("youdao errorCode 0", j.getString("errorCode") == "0",
              j.getString("errorCode"));
        const hs::Json* tr = j.find("translation");
        std::string txt = (tr && tr->at(0)) ? tr->at(0)->asString() : "";
        check("youdao translated", !txt.empty(), txt);
        std::printf("      youdao = %s\n", txt.c_str());
    }

    std::printf("\n==== %d passed, %d failed ====\n", oks, fails);
    return fails ? 1 : 0;
}

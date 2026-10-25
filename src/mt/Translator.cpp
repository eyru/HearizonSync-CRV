#include "Translator.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>

#include "core/Config.h"
#include "core/Http.h"
#include "core/Json.h"
#include "core/Util.h"

namespace hs {

namespace {
const char* kMyMemoryApi = "https://api.mymemory.translated.net/get";
const char* kYoudaoFreeApi = "https://aidemo.youdao.com/trans";
const char* kDeepseekApi = "https://api.deepseek.com/chat/completions";
const char* kYoudaoApi = "https://openapi.youdao.com/api";
const char* kFreeUa =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/120.0 Safari/537.36";

// 目标语言名 -> MyMemory 代码
std::string mymemoryTargetCode(const std::string& display) {
    if (display == "中文") return "zh-CN";
    if (display == "English") return "en";
    if (display == "日本語") return "ja";
    if (display == "한국어") return "ko";
    if (display == "Deutsch") return "de";
    if (display == "Français") return "fr";
    if (display == "Italiano") return "it";
    if (display == "Español") return "es";
    return "zh-CN";
}

// 识别语言码 -> MyMemory 代码
std::string mymemorySrcCode(const std::string& langCode) {
    if (langCode == "zh" || langCode == "yue") return "zh-CN";
    if (langCode == "en") return "en";
    if (langCode == "ja") return "ja";
    if (langCode == "ko") return "ko";
    if (langCode == "de") return "de";
    if (langCode == "fr") return "fr";
    if (langCode == "it") return "it";
    if (langCode == "es") return "es";
    return "en";
}

bool isCjkTarget(const std::string& tgt) {
    return tgt == "zh-CN" || tgt == "ja" || tgt == "ko";
}
}  // namespace

// ---------------------------------------------------------------- 文本工具
bool isMeaningful(const std::string& text) {
    if (text.empty()) return false;
    // UTF-8 下：只要出现非标点、非空白的字符即有意义
    // 标点集合（UTF-8 编码）
    static const char* puncts[] = {
        "。", "．", ".", "，", ",", "！", "!", "？", "?", "；", ";",
        "：", ":", "、", "…", "—", "－", "-", "~", "～", "·", "\"", "'",
        "“", "”", "‘", "’", "「", "」", "（", "）", "(", ")", "[", "]",
        "【", "】", "{", "}", "《", "》", "<", ">", " ", "\t", "\r", "\n"};
    size_t i = 0;
    while (i < text.size()) {
        bool matched = false;
        for (const char* p : puncts) {
            size_t n = std::strlen(p);
            if (text.compare(i, n, p) == 0) {
                i += n;
                matched = true;
                break;
            }
        }
        if (!matched) return true;
    }
    return false;
}

std::string ruleCorrect(const std::string& text) {
    if (text.empty()) return text;
    std::string out;
    out.reserve(text.size());
    // 压缩连续重复标点
    static const char* puncts[] = {"。", "，", "！", "？", "、", "；", "："};
    size_t i = 0;
    while (i < text.size()) {
        bool handled = false;
        for (const char* p : puncts) {
            size_t n = std::strlen(p);
            if (text.compare(i, n, p) == 0) {
                // 统计连续重复次数
                size_t count = 0;
                size_t j = i;
                while (text.compare(j, n, p) == 0) {
                    j += n;
                    ++count;
                }
                out += p;  // 只留一个
                i = j;
                handled = true;
                break;
            }
        }
        if (!handled) {
            out += text[i];
            ++i;
        }
    }
    return trim(out);
}

std::string markdownToPlain(const std::string& md, size_t limit) {
    std::string s = md;
    // 去代码块
    size_t pos;
    while ((pos = s.find("```")) != std::string::npos) {
        size_t end = s.find("```", pos + 3);
        if (end == std::string::npos) {
            s.erase(pos);
            break;
        }
        s.erase(pos, end + 3 - pos);
    }
    // 去图片/链接语法，保留文字
    s = replaceAll(s, "**", "");
    s = replaceAll(s, "__", "");
    s = replaceAll(s, "`", "");
    // 行首 # > - + 去掉
    std::string out;
    size_t start = 0;
    while (start <= s.size()) {
        size_t nl = s.find('\n', start);
        std::string line = nl == std::string::npos ? s.substr(start) : s.substr(start, nl - start);
        size_t k = 0;
        while (k < line.size() && (line[k] == '#' || line[k] == '>' || line[k] == '-' ||
                                   line[k] == '+' || line[k] == ' ')) {
            ++k;
        }
        out += line.substr(k);
        out += '\n';
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    // 折叠 3 个以上换行
    while (out.find("\n\n\n") != std::string::npos) {
        out = replaceAll(out, "\n\n\n", "\n\n");
    }
    out = trim(out);
    if (out.size() > limit) out = out.substr(0, limit) + "…";
    return out;
}

// ---------------------------------------------------------------- LocalTranslator
// LocalTranslator 的实现在 LocalTranslator.cpp

// ---------------------------------------------------------------- FreeOnlineTranslator
FreeOnlineTranslator::FreeOnlineTranslator(LocalTranslator* local,
                                           StatusCallback onStatus)
    : local_(local), onStatus_(std::move(onStatus)) {
    day_ = nowDateString();
    usedChars_ = config().mymemoryUsedChars;
    if (config().mymemoryUsageDay != day_) {
        usedChars_ = 0;
    }
}

void FreeOnlineTranslator::rollDay() {
    std::string today = nowDateString();
    if (today != day_) {
        day_ = today;
        usedChars_ = 0;
        mmDeadUntil_ = 0.0;
        saveUsage();
    }
}

void FreeOnlineTranslator::saveUsage() {
    config().mymemoryUsedChars = usedChars_;
    config().mymemoryUsageDay = day_;
    saveConfig();
}

long long FreeOnlineTranslator::dailyLimit() const {
    return config().mymemoryEmail.empty() ? 5000 : 50000;
}

std::string FreeOnlineTranslator::targetCode(const std::string& display) const {
    return mymemoryTargetCode(display);
}

std::string FreeOnlineTranslator::srcCode(const std::string& langCode) const {
    return mymemorySrcCode(langCode);
}

std::string FreeOnlineTranslator::tryMyMemory(const std::string& text,
                                              const std::string& src,
                                              const std::string& tgt) {
    double now = nowSeconds();
    if (now < mmDeadUntil_) return std::string();

    long long limit = dailyLimit();
    if (usedChars_ + static_cast<long long>(text.size()) > limit) {
        // 额度用完，次日恢复
        std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &t);
        tm.tm_mday += 1;
        tm.tm_hour = 0;
        tm.tm_min = 0;
        tm.tm_sec = 0;
        mmDeadUntil_ = now + 3600.0;  // 简化：一小时后重试
        if (onStatus_) onStatus_("MyMemory 今日免费额度已用完，已切到备用免费源");
        return std::string();
    }

    // 限速
    double gap = now - mmLast_;
    if (gap < 0.35) {
        Sleep(static_cast<DWORD>((0.35 - gap) * 1000));
    }
    mmLast_ = nowSeconds();

    // 分段（MyMemory 单请求约 500 字节）
    const size_t kMaxBytes = 450;
    std::vector<std::string> chunks;
    if (text.size() <= kMaxBytes) {
        chunks.push_back(text);
    } else {
        std::string buf;
        size_t i = 0;
        while (i < text.size()) {
            // 找断句点
            size_t cut = kMaxBytes;
            if (i + cut >= text.size()) {
                cut = text.size() - i;
            } else {
                size_t best = cut;
                for (size_t k = cut; k > cut / 2; --k) {
                    if (text[i + k - 1] == '.' || text[i + k - 1] == '!' ||
                        text[i + k - 1] == '?' || text[i + k - 1] == '\n') {
                        best = k;
                        break;
                    }
                }
                cut = best;
            }
            chunks.push_back(text.substr(i, cut));
            i += cut;
        }
    }

    std::vector<std::string> parts;
    for (const auto& chunk : chunks) {
        std::string url = std::string(kMyMemoryApi) + "?q=" + urlEncode(chunk) +
                          "&langpair=" + urlEncode(src + "|" + tgt);
        std::string email = config().mymemoryEmail;
        if (!email.empty()) url += "&de=" + urlEncode(email);

        HttpResult r = httpGet(url, {{"User-Agent", kFreeUa}}, 8);
        if (r.failed()) {
            mmDeadUntil_ = nowSeconds() + 30;
            if (onStatus_) onStatus_("MyMemory 连接失败，已切到备用免费源");
            return std::string();
        }
        Json j = Json::parse(r.body);
        const Json* rd = j.find("responseData");
        std::string out = rd ? rd->getString("translatedText") : "";
        out = trim(out);
        bool quota = j.getBool("quotaFinished", false) ||
                     toLower(out).find("mymemory warning") != std::string::npos;
        if (quota) {
            mmDeadUntil_ = nowSeconds() + 3600;
            usedChars_ = limit;
            saveUsage();
            if (onStatus_) onStatus_("MyMemory 今日免费额度已用完，已切到备用免费源");
            return std::string();
        }
        if (r.status != 200 || out.empty()) {
            mmDeadUntil_ = nowSeconds() + 15;
            return std::string();
        }
        usedChars_ += static_cast<long long>(chunk.size());
        saveUsage();
        parts.push_back(out);
    }

    if (parts.empty()) return std::string();
    std::string joined;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) joined += isCjkTarget(tgt) ? "" : " ";
        joined += parts[i];
    }
    return joined;
}

std::string FreeOnlineTranslator::tryYoudaoFree(const std::string& text) {
    double now = nowSeconds();
    if (now < ydDeadUntil_) return std::string();

    // 每分钟最多 6 次
    std::vector<double> recent;
    for (double t : ydTimes_) {
        if (now - t < 60.0) recent.push_back(t);
    }
    ydTimes_ = recent;
    if (ydTimes_.size() >= 6) {
        if (onStatus_) onStatus_("有道免费接口限频，稍后自动恢复");
        return std::string();
    }

    double gap = now - ydLast_;
    if (gap < 4.0) Sleep(static_cast<DWORD>((4.0 - gap) * 1000));

    std::string url = std::string(kYoudaoFreeApi) + "?q=" + urlEncode(text) +
                      "&from=auto&to=zh-CHS";
    HttpResult r = httpGet(url,
                           {{"User-Agent", kFreeUa},
                            {"Referer", "https://ai.youdao.com/"}},
                           8);
    if (r.failed()) {
        ydDeadUntil_ = nowSeconds() + 20;
        return std::string();
    }
    ydLast_ = nowSeconds();
    ydTimes_.push_back(ydLast_);

    Json j = Json::parse(r.body);
    std::string code = j.getString("errorCode");
    const Json* tr = j.find("translation");
    std::string out = (tr && tr->at(0)) ? trim(tr->at(0)->asString()) : "";

    if (code == "411") {
        ydDeadUntil_ = nowSeconds() + 60;
        return std::string();
    }
    if (code != "0" || out.empty()) return std::string();
    return out;
}

std::string FreeOnlineTranslator::tryLocal(const std::string& text) {
    if (!local_ || !local_->available()) return std::string();
    std::string err;
    return local_->translate(text, &err);
}

MtResult FreeOnlineTranslator::translate(const std::string& text,
                                         const std::string& target,
                                         const std::string& srcLang,
                                         ChunkCallback onChunk) {
    MtResult out;
    std::string input = trim(text);
    if (input.empty()) return out;

    rollDay();
    std::string tgt = targetCode(target);
    std::string src = srcCode(srcLang);
    if (src == tgt) {
        out.text = input;
        out.provider = "无需翻译";
        lastProvider_ = out.provider;
        if (onChunk) onChunk(out.text);
        return out;
    }

    // 缓存
    std::string key = input + "|" + src + "|" + tgt;
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        out.text = it->second;
        out.provider = "免费(缓存)";
        lastProvider_ = out.provider;
        if (onChunk) onChunk(out.text);
        return out;
    }

    // 1. MyMemory
    std::string r1 = tryMyMemory(input, src, tgt);
    if (!r1.empty()) {
        if (cache_.size() > 400) cache_.clear();
        cache_[key] = r1;
        out.text = r1;
        out.provider = "免费(MyMemory)";
        lastProvider_ = out.provider;
        if (onChunk) onChunk(out.text);
        return out;
    }

    // 2. 有道免费（仅中文目标）
    if (tgt == "zh-CN") {
        std::string r2 = tryYoudaoFree(input);
        if (!r2.empty()) {
            if (cache_.size() > 400) cache_.clear();
            cache_[key] = r2;
            out.text = r2;
            out.provider = "免费(有道)";
            lastProvider_ = out.provider;
            if (onChunk) onChunk(out.text);
            return out;
        }
    }

    // 3. 本地兜底（仅英译中）
    if (tgt == "zh-CN" && src == "en") {
        std::string r3 = tryLocal(input);
        if (!r3.empty()) {
            if (cache_.size() > 400) cache_.clear();
            cache_[key] = r3;
            out.text = r3;
            out.provider = "本地兜底";
            lastProvider_ = out.provider;
            if (onChunk) onChunk(out.text);
            return out;
        }
    }

    out.text = "[免费在线翻译暂时不可用：今日额度可能已用完或网络不通]";
    out.provider = "免费(失败)";
    lastProvider_ = out.provider;
    return out;
}

// ---------------------------------------------------------------- ApiTranslator
ApiTranslator::ApiTranslator(StatusCallback onStatus) : onStatus_(std::move(onStatus)) {}

bool ApiTranslator::deepseekAvailable() const {
    if (!config().deepseekKey.empty()) return true;
    const char* env = std::getenv("DEEPSEEK_API_KEY");
    return env && *env;
}

bool ApiTranslator::youdaoAvailable() const {
    return !config().youdaoAppKey.empty() && !config().youdaoAppSecret.empty();
}

MtResult ApiTranslator::deepseek(const std::string& text, const std::string& target,
                                 ChunkCallback onChunk) {
    MtResult out;
    std::string key = config().deepseekKey;
    if (key.empty()) {
        const char* env = std::getenv("DEEPSEEK_API_KEY");
        if (env) key = env;
    }
    if (key.empty()) {
        out.text = "[未配置 DeepSeek API Key]";
        return out;
    }

    std::string sysPrompt =
        "你是专业同声传译员。把用户给的内容翻译成" + target +
        "。\n要求：\n1. 只输出译文，不要任何解释、不要引号、不要重复原文\n"
        "2. 保持口语自然，像真人同传那样说人话\n"
        "3. 如果原文是语气词/半句话，就译成对应的口语\n"
        "4. 专有名词保留原文或按通用译法";

    Json body = Json::object();
    body.set("model", "deepseek-chat");
    Json messages = Json::array();
    Json m1 = Json::object();
    m1.set("role", "system");
    m1.set("content", sysPrompt);
    Json m2 = Json::object();
    m2.set("role", "user");
    m2.set("content", text);
    messages.push(m1);
    messages.push(m2);
    body.set("messages", messages);
    body.set("stream", true);
    body.set("temperature", 0.3);
    body.set("max_tokens", 1000);

    std::string payload = body.dump();

    struct Ctx {
        std::string out;
        ChunkCallback onChunk;
    } ctx{std::string(), onChunk};

    bool ok = httpPostStream(
        kDeepseekApi, payload,
        {{"Authorization", "Bearer " + key},
         {"Accept", "application/json"},
         {"User-Agent", "HearizonSync/2.5.6"}},
        [](const std::string& line, void* user) -> bool {
            auto* c = static_cast<Ctx*>(user);
            if (!startsWith(line, "data:")) return true;
            std::string payload2 = trim(line.substr(5));
            if (payload2 == "[DONE]") return false;
            Json j = Json::parse(payload2);
            const Json* choices = j.find("choices");
            if (!choices || choices->size() == 0) return true;
            const Json* c0 = choices->at(0);
            if (!c0) return true;
            const Json* delta = c0->find("delta");
            if (!delta) return true;
            std::string content = delta->getString("content");
            if (!content.empty()) {
                c->out += content;
                if (c->onChunk) c->onChunk(content);
            }
            return true;
        },
        &ctx, 60, nullptr);

    if (!ok && ctx.out.empty()) {
        out.text = "[翻译失败: DeepSeek 连接失败]";
        return out;
    }
    out.text = ctx.out;
    out.provider = "DeepSeek API";
    return out;
}

MtResult ApiTranslator::youdao(const std::string& text, const std::string& target) {
    MtResult out;
    std::string appKey = config().youdaoAppKey;
    std::string appSecret = config().youdaoAppSecret;
    if (appKey.empty() || appSecret.empty()) {
        out.text = "[未配置有道 appKey/appSecret]";
        return out;
    }

    std::string salt = std::to_string(static_cast<long long>(nowSeconds() * 1000));
    std::string curtime = std::to_string(static_cast<long long>(std::time(nullptr)));
    std::string sign = sha256Hex(appKey + text + salt + curtime + appSecret);

    std::string url = std::string(kYoudaoApi) + "?q=" + urlEncode(text) +
                      "&from=auto&to=" + urlEncode(outputCode(target)) +
                      "&appKey=" + urlEncode(appKey) + "&salt=" + salt +
                      "&sign=" + sign + "&signType=v3&curtime=" + curtime;
    if (!config().youdaoDomain.empty() && config().youdaoDomain != "general") {
        url += "&domain=" + urlEncode(config().youdaoDomain);
    }

    HttpResult r = httpGet(url, {{"User-Agent", "HearizonSync/2.5.6"}}, 20);
    if (r.failed()) {
        out.text = "[翻译失败: 有道连接失败]";
        return out;
    }
    Json j = Json::parse(r.body);
    std::string code = j.getString("errorCode");
    const Json* tr = j.find("translation");
    std::string txt = (tr && tr->at(0)) ? tr->at(0)->asString() : "";
    if (code != "0" || txt.empty()) {
        out.text = "[有道翻译失败: errorCode=" + code + "]";
        return out;
    }
    out.text = txt;
    out.provider = "有道 API";
    return out;
}

std::string ApiTranslator::correct(const std::string& text, const std::string& lang) {
    (void)lang;
    std::string key = config().deepseekKey;
    if (key.empty()) {
        const char* env = std::getenv("DEEPSEEK_API_KEY");
        if (env) key = env;
    }
    if (key.empty() || text.empty()) return text;

    std::string system =
        "你是语音识别（ASR）结果的纠错器（beta）。"
        "输入可能有同音字、错别字、漏字、重复词或标点缺失。\n"
        "要求：\n"
        "1. 只做最小必要修正，恢复明显错误；不改变原意，不润色，不扩写，不翻译\n"
        "2. 没有明显错误就原样返回\n"
        "3. 只输出修正后的文本，不要解释、不要引号、不要任何前缀";

    Json body = Json::object();
    body.set("model", "deepseek-chat");
    Json messages = Json::array();
    Json m1 = Json::object();
    m1.set("role", "system");
    m1.set("content", system);
    Json m2 = Json::object();
    m2.set("role", "user");
    m2.set("content", text);
    messages.push(m1);
    messages.push(m2);
    body.set("messages", messages);
    body.set("stream", false);
    body.set("temperature", 0.0);
    body.set("max_tokens", 300);

    HttpResult r = httpPostJson(kDeepseekApi, body.dump(),
                                {{"Authorization", "Bearer " + key},
                                 {"User-Agent", "HearizonSync/2.5.6"}},
                                15);
    if (!r.ok()) return text;
    Json j = Json::parse(r.body);
    const Json* choices = j.find("choices");
    if (!choices || choices->size() == 0) return text;
    const Json* c0 = choices->at(0);
    if (!c0) return text;
    const Json* msg = c0->find("message");
    if (!msg) return text;
    std::string out = trim(msg->getString("content"));
    return out.empty() ? text : out;
}

// ---------------------------------------------------------------- Translator
Translator::Translator(StatusCallback onStatus)
    : onStatus_(onStatus), local_(onStatus), free_(&local_, onStatus), api_(onStatus) {}

void Translator::setStatusCallback(StatusCallback cb) {
    onStatus_ = cb;
    local_.setStatusCallback(cb);
}

MtResult Translator::translate(const std::string& text, const std::string& target,
                               const std::string& srcLang, ChunkCallback onChunk) {
    MtResult out;
    Config& c = config();

    // 本地翻译
    if (c.translatorMode == TR_LOCAL) {
        std::string sv, wh;
        inputCodes("自动检测", &sv, &wh);
        std::string err;
        std::string r = local_.translate(text, &err);
        if (r.empty()) {
            out.text = "[本地模式只支持 英语→中文，请切到「在线翻译（免费）」]";
            out.provider = "本地模型";
            return out;
        }
        out.text = r;
        out.provider = "本地模型";
        if (onChunk) onChunk(r);
        return out;
    }

    // 免费在线
    if (c.translatorMode == TR_FREE) {
        return free_.translate(text, target, srcLang, onChunk);
    }

    // API 模式
    if (c.translatorMode == TR_API) {
        std::string engine = c.apiEngine;
        bool dsOk = api_.deepseekAvailable();
        bool ydOk = api_.youdaoAvailable();

        if (engine == API_ENGINE_YOUDAO) {
            if (!ydOk) {
                out.text = "[API 引擎选的是「有道智云」，但还没填 appKey/appSecret]";
                return out;
            }
            return api_.youdao(text, target);
        }
        if (engine == API_ENGINE_DEEPSEEK) {
            if (!dsOk) {
                out.text = "[API 引擎选的是「DeepSeek」，但还没填 API Key]";
                return out;
            }
            return api_.deepseek(text, target, onChunk);
        }
        // 自动兜底
        if (dsOk) {
            MtResult r = api_.deepseek(text, target, onChunk);
            if (r.ok()) return r;
            if (ydOk) {
                if (onStatus_) onStatus_("DeepSeek 不可用，已自动切到有道 API");
                MtResult r2 = api_.youdao(text, target);
                if (r2.ok()) {
                    r2.provider = "有道 API（兜底）";
                    return r2;
                }
            }
            return r;
        }
        if (ydOk) return api_.youdao(text, target);
        out.text = "[API 模式还没配置 Key：设置 → DeepSeek API Key / 有道 appKey+appSecret]";
        out.provider = "无";
        return out;
    }

    out.text = "[未知翻译模式，请在左侧重新选择]";
    return out;
}

std::string Translator::autoCorrect(const std::string& text, const std::string& lang) {
    std::string fixed = ruleCorrect(text);
    if (fixed.empty()) return text;
    // 只有配置了 Key 才走云端
    if (api_.deepseekAvailable() && fixed.size() >= 2) {
        std::string out = api_.correct(fixed, lang);
        if (!out.empty()) return out;
    }
    return fixed;
}

}  // namespace hs

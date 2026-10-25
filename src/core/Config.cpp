#include "Config.h"

#include <cstdlib>

#include "Json.h"
#include "Util.h"

namespace hs {

const char* TR_LOCAL = "本地翻译（离线）";
const char* TR_FREE = "在线翻译（免费）";
const char* TR_API = "API 模式（付费）";

const char* API_ENGINE_DEEPSEEK = "DeepSeek";
const char* API_ENGINE_YOUDAO = "有道智云";
const char* API_ENGINE_AUTO = "自动兜底";

const std::vector<std::string> OUTPUT_LANG_NAMES = {
    "中文", "English", "日本語", "한국어", "Deutsch", "Français", "Italiano", "Español"};

const std::vector<std::string> INPUT_LANG_NAMES = {
    "自动检测", "英语", "中文", "日语", "韩语", "粤语", "德语", "法语", "意大利语", "西班牙语"};

namespace {
struct OutputLang {
    const char* display;
    const char* code;
    bool localSupported;
};
const OutputLang kOutput[] = {
    {"中文", "zh-CHS", true},   {"English", "en", false},
    {"日本語", "ja", false},    {"한국어", "ko", false},
    {"Deutsch", "de", false},   {"Français", "fr", false},
    {"Italiano", "it", false},  {"Español", "es", false},
};

struct InputLang {
    const char* display;
    const char* sv;      // nullptr 表示 SenseVoice 不支持
    const char* wh;
};
const InputLang kInput[] = {
    {"自动检测", "auto", nullptr}, {"英语", "en", "en"},   {"中文", "zh", "zh"},
    {"日语", "ja", "ja"},          {"韩语", "ko", "ko"},   {"粤语", "yue", "yue"},
    {"德语", nullptr, "de"},       {"法语", nullptr, "fr"}, {"意大利语", nullptr, "it"},
    {"西班牙语", nullptr, "es"},
};

// 识别结果语言码 -> 中文名
struct LangName {
    const char* code;
    const char* display;
};
const LangName kLangNames[] = {
    {"zh", "中文"},   {"en", "英语"},   {"ja", "日语"},     {"ko", "韩语"},
    {"yue", "粤语"},  {"de", "德语"},   {"fr", "法语"},     {"it", "意大利语"},
    {"es", "西班牙语"}, {"auto", "自动"},
};
}  // namespace

Config& config() {
    static Config c;
    return c;
}

std::string configPath() {
    std::string home = userHome();
    if (home.empty()) home = exeDir();
    return joinPath(home, ".hearizon_sync.json");
}

bool loadConfig() {
    Config& c = config();
    std::string text = readTextFile(configPath());
    if (text.empty()) {
        c.translatorMode = TR_FREE;
        c.apiEngine = API_ENGINE_AUTO;
        c.youdaoDomain = "general";
        c.subTheme = "green";
        c.subPos = "bottom";
        return false;
    }
    Json j = Json::parse(text);
    if (!j.isObject()) return false;

    c.translatorMode = j.getString("translator_mode", TR_FREE);
    // 旧版遗留的 auto/local/deepseek/youdao 一律归到免费在线
    if (c.translatorMode != TR_LOCAL && c.translatorMode != TR_FREE &&
        c.translatorMode != TR_API) {
        c.translatorMode = TR_FREE;
    }
    c.apiEngine = j.getString("api_engine", API_ENGINE_AUTO);
    if (c.apiEngine != API_ENGINE_DEEPSEEK && c.apiEngine != API_ENGINE_YOUDAO &&
        c.apiEngine != API_ENGINE_AUTO) {
        c.apiEngine = API_ENGINE_AUTO;
    }

    c.deepseekKey = j.getString("deepseek_api_key");
    c.youdaoAppKey = j.getString("youdao_app_key");
    c.youdaoAppSecret = j.getString("youdao_app_secret");
    c.youdaoDomain = j.getString("youdao_domain", "general");
    c.mymemoryEmail = j.getString("mymemory_email");
    c.mymemoryUsedChars = j.getInt("mymemory_used_chars", 0);
    c.mymemoryUsageDay = j.getString("mymemory_usage_day");

    c.gpuAccel = j.getBool("gpu_accel", false);
    c.autoCorrect = j.getBool("auto_correct", false);
    c.correctWarnSkip = j.getBool("correct_warn_skip", false);
    c.hotwords = j.getString("hotwords");

    c.subTheme = j.getString("sub_theme", "green");
    c.subPos = j.getString("sub_pos", "bottom");
    c.subSizeDst = j.getInt("sub_size_dst", 18);
    c.subSizeSrc = j.getInt("sub_size_src", 14);
    c.windowX = j.getInt("window_x", -1);
    c.windowY = j.getInt("window_y", -1);
    c.windowW = j.getInt("window_w", 1180);
    c.windowH = j.getInt("window_h", 780);

    c.updateCheck = j.getBool("update_check", true);
    c.updateSkipTag = j.getString("update_skip_tag");
    return true;
}

bool saveConfig() {
    Config& c = config();
    Json j = Json::object();
    j.set("translator_mode", c.translatorMode);
    j.set("api_engine", c.apiEngine);
    j.set("deepseek_api_key", c.deepseekKey);
    j.set("youdao_app_key", c.youdaoAppKey);
    j.set("youdao_app_secret", c.youdaoAppSecret);
    j.set("youdao_domain", c.youdaoDomain);
    j.set("mymemory_email", c.mymemoryEmail);
    j.set("mymemory_used_chars", static_cast<int>(c.mymemoryUsedChars));
    j.set("mymemory_usage_day", c.mymemoryUsageDay);
    j.set("gpu_accel", c.gpuAccel);
    j.set("auto_correct", c.autoCorrect);
    j.set("correct_warn_skip", c.correctWarnSkip);
    j.set("hotwords", c.hotwords);
    j.set("sub_theme", c.subTheme);
    j.set("sub_pos", c.subPos);
    j.set("sub_size_dst", c.subSizeDst);
    j.set("sub_size_src", c.subSizeSrc);
    j.set("window_x", c.windowX);
    j.set("window_y", c.windowY);
    j.set("window_w", c.windowW);
    j.set("window_h", c.windowH);
    j.set("update_check", c.updateCheck);
    j.set("update_skip_tag", c.updateSkipTag);
    return writeTextFile(configPath(), j.dump(2));
}

std::vector<std::string> hotwordsList() {
    std::vector<std::string> out;
    std::string raw = config().hotwords;
    if (raw.empty()) return out;

    // 统一把全角标点替换成半角，避免按字节切分破坏 UTF-8
    raw = replaceAll(raw, "，", ",");
    raw = replaceAll(raw, "；", ";");
    raw = replaceAll(raw, "、", ",");
    raw = replaceAll(raw, "\r\n", "\n");
    raw = replaceAll(raw, "\r", "\n");

    std::string cur;
    auto push = [&]() {
        std::string w = trim(cur);
        cur.clear();
        if (w.empty()) return;
        for (const auto& e : out)
            if (e == w) return;
        out.push_back(w);
    };
    for (char ch : raw) {
        if (ch == ',' || ch == ';' || ch == '\n') {
            push();
        } else {
            cur += ch;
        }
    }
    push();
    return out;
}

std::string outputCode(const std::string& displayName) {
    for (const auto& o : kOutput) {
        if (displayName == o.display) return o.code;
    }
    return "zh-CHS";
}

void inputCodes(const std::string& displayName, std::string* svCode,
                std::string* whCode) {
    for (const auto& i : kInput) {
        if (displayName == i.display) {
            if (svCode) *svCode = i.sv ? i.sv : "";
            if (whCode) *whCode = i.wh ? i.wh : "";
            return;
        }
    }
    if (svCode) *svCode = "auto";
    if (whCode) whCode->clear();
}

std::string langDisplayName(const std::string& code) {
    for (const auto& l : kLangNames) {
        if (code == l.code) return l.display;
    }
    return code.empty() ? "?" : code;
}

std::vector<std::string> modeInputLangs(const std::string& mode) {
    if (mode == TR_LOCAL) return {"自动检测", "英语"};
    return INPUT_LANG_NAMES;
}

std::vector<std::string> modeOutputLangs(const std::string& mode) {
    if (mode == TR_LOCAL) return {"中文"};
    return OUTPUT_LANG_NAMES;
}

int compareVersion(const std::string& a, const std::string& b) {
    auto parse = [](const std::string& s) {
        std::vector<int> v;
        std::string cur;
        for (char c : s) {
            if (c >= '0' && c <= '9') {
                cur += c;
            } else if (!cur.empty()) {
                v.push_back(std::atoi(cur.c_str()));
                cur.clear();
            }
        }
        if (!cur.empty()) v.push_back(std::atoi(cur.c_str()));
        while (v.size() < 3) v.push_back(0);
        return v;
    };
    std::vector<int> x = parse(a), y = parse(b);
    for (size_t i = 0; i < 3; ++i) {
        if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    }
    return 0;
}

// ---------------------------------------------------------------- 模型路径
std::string findModelDir(const std::string& name) {
    std::vector<std::string> candidates;
    std::string envKey;
    if (name == "sv") {
        envKey = "SLM_SV_DIR";
    } else if (name == "opus_en_zh") {
        envKey = "SLM_OPUS_EN_ZH_DIR";
    } else if (name == "whisper") {
        envKey = "SLM_WHISPER_DIR";
    }
    if (!envKey.empty()) {
        const char* v = std::getenv(envKey.c_str());
        if (v && *v) candidates.push_back(v);
    }
    std::string dir = exeDir();
    candidates.push_back(joinPath(dir, joinPath("models", name)));
    candidates.push_back(joinPath(dir, joinPath("_internal", joinPath("models", name))));
    candidates.push_back(joinPath(dir, joinPath("..", joinPath("models", name))));
    candidates.push_back(
        joinPath(dir, joinPath("..", joinPath("..", joinPath("models", name)))));
    candidates.push_back("D:\\slm-models\\" + name);

    for (const auto& c : candidates) {
        if (dirExists(c)) return c;
    }
    return candidates.empty() ? std::string() : candidates[0];
}

std::string senseVoiceModelPath() {
    return joinPath(findModelDir("sv"), "model.int8.onnx");
}

std::string senseVoiceTokensPath() { return joinPath(findModelDir("sv"), "tokens.txt"); }

std::string opusModelDir() { return findModelDir("opus_en_zh"); }

}  // namespace hs

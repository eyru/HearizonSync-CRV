// Hearizon Sync C.R.V. - 配置（对应 Python 版 ~/.hearizon_sync.json）
#pragma once

#include <string>
#include <vector>

namespace hs {

// 翻译模式
extern const char* TR_LOCAL;
extern const char* TR_FREE;
extern const char* TR_API;

// API 引擎
extern const char* API_ENGINE_DEEPSEEK;
extern const char* API_ENGINE_YOUDAO;
extern const char* API_ENGINE_AUTO;

// 目标语言（界面显示名）
extern const std::vector<std::string> OUTPUT_LANG_NAMES;
// 输入语言（界面显示名）
extern const std::vector<std::string> INPUT_LANG_NAMES;

struct Config {
    // 翻译
    std::string translatorMode;
    std::string apiEngine;

    // API 凭据
    std::string deepseekKey;
    std::string youdaoAppKey;
    std::string youdaoAppSecret;
    std::string youdaoDomain;

    // 免费在线
    std::string mymemoryEmail;
    long long mymemoryUsedChars = 0;
    std::string mymemoryUsageDay;

    // 识别
    bool gpuAccel = false;
    bool autoCorrect = false;
    bool correctWarnSkip = false;
    std::string hotwords;

    // 界面
    std::string subTheme;
    std::string subPos;
    int subSizeDst = 18;
    int subSizeSrc = 14;
    int windowX = -1;
    int windowY = -1;
    int windowW = 1180;
    int windowH = 780;

    // 更新
    bool updateCheck = true;
    std::string updateSkipTag;
};

// 全局配置实例
Config& config();

// 读写
std::string configPath();
bool loadConfig();
bool saveConfig();

// 热词解析：逗号/分号/换行分隔，去重
std::vector<std::string> hotwordsList();

// 目标语言名 -> 有道/MyMemory 代码
std::string outputCode(const std::string& displayName);
// 输入语言名 -> (sensevoice code, whisper code)
void inputCodes(const std::string& displayName, std::string* svCode,
                std::string* whCode);
// 语言码 -> 中文显示名
std::string langDisplayName(const std::string& code);

// 某翻译模式下可用的输入/输出语言
std::vector<std::string> modeInputLangs(const std::string& mode);
std::vector<std::string> modeOutputLangs(const std::string& mode);

// 模型目录解析
std::string findModelDir(const std::string& name);
std::string senseVoiceModelPath();
std::string senseVoiceTokensPath();
std::string opusModelDir();

// 版本比较：返回 -1/0/1
int compareVersion(const std::string& a, const std::string& b);

}  // namespace hs

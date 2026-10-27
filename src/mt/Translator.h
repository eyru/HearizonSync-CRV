// Hearizon Sync C.R.V. - 翻译层
#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace hs {

// 翻译结果
struct MtResult {
    std::string text;
    std::string provider;   // 实际使用的引擎名
    bool ok() const { return !text.empty() && text[0] != '['; }
};

// 流式回调：收到一段就调一次
using ChunkCallback = std::function<void(const std::string&)>;
using StatusCallback = std::function<void(const std::string&)>;

// ---------------------------------------------------------------- 本地 opus-mt
class LocalTranslator {
public:
    explicit LocalTranslator(StatusCallback onStatus = nullptr);
    ~LocalTranslator();

    bool available() const;
    bool load(std::string* error);
    void unload();
    void setStatusCallback(StatusCallback cb);

    // 仅支持 英语 -> 中文
    std::string translate(const std::string& text, std::string* error);

private:
    StatusCallback onStatus_;
    void* impl_ = nullptr;   // ctranslate2::Translator*
    void* spSrc_ = nullptr;  // sentencepiece 处理器（简化版：用内置分词）
    bool loaded_ = false;
};

// ---------------------------------------------------------------- 免费在线
class FreeOnlineTranslator {
public:
    explicit FreeOnlineTranslator(LocalTranslator* local = nullptr,
                                  StatusCallback onStatus = nullptr);

    bool available() const { return true; }

    // 主入口：target 是界面显示名（中文/English/...）
    // srcLang 是识别出的语言码（zh/en/ja/...），可为空
    MtResult translate(const std::string& text, const std::string& target,
                       const std::string& srcLang, ChunkCallback onChunk = nullptr);

    std::string lastProvider() const { return lastProvider_; }

    // 额度
    long long usedChars() const { return usedChars_; }
    long long dailyLimit() const;

private:
    std::string targetCode(const std::string& display) const;
    std::string srcCode(const std::string& langCode) const;

    std::string tryMyMemory(const std::string& text, const std::string& src,
                            const std::string& tgt);
    std::string tryYoudaoFree(const std::string& text, const std::string& tgt);
    std::string tryLocal(const std::string& text);

    void rollDay();
    void saveUsage();

    LocalTranslator* local_;
    StatusCallback onStatus_;
    std::string lastProvider_;

    std::string day_;
    long long usedChars_ = 0;
    double mmDeadUntil_ = 0.0;
    double mmLast_ = 0.0;
    double ydDeadUntil_ = 0.0;
    double ydLast_ = 0.0;
    std::vector<double> ydTimes_;

    std::map<std::string, std::string> cache_;
};

// ---------------------------------------------------------------- API（DeepSeek / 有道）
class ApiTranslator {
public:
    explicit ApiTranslator(StatusCallback onStatus = nullptr);

    bool deepseekAvailable() const;
    bool youdaoAvailable() const;
    bool anyAvailable() const { return deepseekAvailable() || youdaoAvailable(); }

    // DeepSeek 流式翻译
    MtResult deepseek(const std::string& text, const std::string& target,
                      ChunkCallback onChunk);

    // 有道智云
    MtResult youdao(const std::string& text, const std::string& target);

    // 自动纠错（DeepSeek 非流式）
    std::string correct(const std::string& text, const std::string& lang);

private:
    StatusCallback onStatus_;
};

// ---------------------------------------------------------------- 统一分发
class Translator {
public:
    explicit Translator(StatusCallback onStatus = nullptr);

    // 按当前配置的模式分发，返回译文与实际引擎名
    MtResult translate(const std::string& text, const std::string& target,
                       const std::string& srcLang, ChunkCallback onChunk = nullptr);

    // 自动纠错
    std::string autoCorrect(const std::string& text, const std::string& lang);

    void setStatusCallback(StatusCallback cb);

    LocalTranslator& local() { return local_; }
    FreeOnlineTranslator& free() { return free_; }
    ApiTranslator& api() { return api_; }

    // GPU 设置变化
    void invalidateLocal() { local_.unload(); }

private:
    StatusCallback onStatus_;
    LocalTranslator local_;
    FreeOnlineTranslator free_;
    ApiTranslator api_;
};

// ---------------------------------------------------------------- 工具
// 模型目录
std::string opusModelDir();

// 文本是否有实义（纯标点丢弃）
bool isMeaningful(const std::string& text);
// 规则纠错（标点重复、单字重复）
std::string ruleCorrect(const std::string& text);
// Markdown 转纯文本（更新说明展示）
std::string markdownToPlain(const std::string& md, size_t limit);

}  // namespace hs

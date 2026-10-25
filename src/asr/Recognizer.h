// Hearizon Sync C.R.V. - 识别引擎（SenseVoice + Whisper 回退）
#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace hs {

// 识别结果
struct AsrResult {
    std::string text;
    std::string lang;      // zh / en / ja / ko / yue ...
    std::string emotion;   // HAPPY / SAD / NEUTRAL ...
    std::string event;     // LAUGHTER / APPLAUSE ...
    bool empty() const { return text.empty(); }
};

// 识别引擎类型
enum class AsrEngine {
    SenseVoice,   // 默认，中英日韩粤
    Whisper,      // 回退，德法意西等
};

class Recognizer {
public:
    using StatusCallback = std::function<void(const std::string&)>;

    Recognizer();
    ~Recognizer();

    void setStatusCallback(StatusCallback cb) { onStatus_ = std::move(cb); }

    // 设置输入语言（界面显示名），必要时自动切引擎
    void setLanguage(const std::string& displayName);
    std::string language() const { return langName_; }

    // 显式切换引擎
    void setEngine(AsrEngine e);
    AsrEngine engine() const { return engine_; }

    // 模型是否就绪
    bool ready() const;
    // 加载模型（阻塞，首次调用慢）
    bool load(std::string* error);
    // 卸载（切换引擎/GPU 设置后调用）
    void unload();

    // 识别一段 16k 单声道 float
    AsrResult transcribe(const std::vector<float>& audio, std::string* error);

    // 是否应该用 Whisper（当前语言 SenseVoice 不支持）
    bool needsWhisper() const;

    // 音频增强开关（热词纠错）
    void setHotwordBoost(bool on) { hotwordBoost_ = on; }

    // GPU 设置变化后需要重新加载
    void invalidate() { unload(); }

private:
    bool loadSenseVoice(std::string* error);
    bool loadWhisper(std::string* error);
    AsrResult transcribeSenseVoice(const std::vector<float>& audio, std::string* error);
    AsrResult transcribeWhisper(const std::vector<float>& audio, std::string* error);
    std::string applyHotwords(const std::string& text) const;

    StatusCallback onStatus_;
    std::string langName_ = "自动检测";
    AsrEngine engine_ = AsrEngine::SenseVoice;
    bool hotwordBoost_ = true;

    // sherpa-onnx 句柄（void* 避免头文件污染）
    void* svRecognizer_ = nullptr;
    // faster-whisper 等价物：本版用 sherpa-onnx 的 whisper 模型
    void* whisperRecognizer_ = nullptr;
    std::string whisperModelDir_;

    mutable std::mutex lock_;
    std::atomic<bool> loaded_{false};
};

}  // namespace hs

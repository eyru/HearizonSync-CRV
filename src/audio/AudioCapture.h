// Hearizon Sync C.R.V. - 音频采集（WASAPI 共享模式）
#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace hs {

constexpr int kSampleRate = 16000;
constexpr int kBlockMs = 30;
constexpr int kBlockSize = kSampleRate * kBlockMs / 1000;

// 输入设备信息
struct AudioDevice {
    int index = -1;
    std::string name;
    std::string id;          // WASAPI endpoint id（选择设备只认这个，不认序号）
    bool isLoopbackLike = false;   // 名字里带 CABLE / 立体声混音 等
    bool active = false;     // 当前是否处于「已启用」状态，只有启用的才能录音
    std::string stateText;   // 已启用 / 已禁用 / 未插入 / 不存在
};

// 枚举所有可用输入设备
std::vector<AudioDevice> enumerateInputDevices();

// 能量 VAD + 降噪 + 增益（与 Python 版行为一致）
class VadProcessor {
public:
    using SentenceCallback = std::function<void(const std::vector<float>&)>;
    using LevelCallback = std::function<void(double rms)>;
    using PartialCallback = std::function<void(const std::vector<float>&)>;

    void setCallbacks(SentenceCallback onSentence, LevelCallback onLevel,
                      PartialCallback onPartial) {
        onSentence_ = std::move(onSentence);
        onLevel_ = std::move(onLevel);
        onPartial_ = std::move(onPartial);
    }

    void setDenoise(bool on) { denoiseOn_ = on; }
    bool denoise() const { return denoiseOn_; }

    // 输入一块 16k 单声道 float，驱动切句状态机
    void process(const float* data, int frames);

    void reset();

    // 环境噪声自适应门限
    double noiseFloor() const;
    double threshold() const;

private:
    void emitSentence();

    std::vector<float> buf_;
    std::vector<float> tail_;
    int speechMs_ = 0;
    int silenceMs_ = 0;
    bool active_ = false;
    double lastPartial_ = 0.0;
    bool denoiseOn_ = true;

    std::vector<double> noiseHist_;

    SentenceCallback onSentence_;
    LevelCallback onLevel_;
    PartialCallback onPartial_;
};

// 音频采集线程
class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    // 启动；返回空字符串表示成功，否则是错误信息
    std::string start(int deviceIndex);
    void stop();
    bool running() const { return running_.load(); }

    VadProcessor& vad() { return vad_; }
    std::string lastError() const { return lastError_; }

    // 录音测试：录 durationMs 毫秒，返回 rms / peak
    static bool testMic(int deviceIndex, int durationMs, double* rms, double* peak,
                        std::string* error);

private:
    void run();
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopFlag_{false};
    int deviceIndex_ = -1;
    VadProcessor vad_;
    std::string lastError_;
    void* impl_ = nullptr;
};

// 降噪：只压低明显低于噪声底的帧
void denoiseBuffer(std::vector<float>& audio);
// 远场增益归一
void normalizeGain(std::vector<float>& audio, float target = 0.12f,
                   float maxGain = 6.0f);
// 把片段起点挪到能量低谷
std::vector<float> trimToValley(const std::vector<float>& seg, int searchMs = 260,
                                int stepMs = 10);

}  // namespace hs

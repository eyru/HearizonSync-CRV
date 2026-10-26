// 合成音频驱动 VadProcessor，验证「有语音就一定能断句」。
// 不进正式构建，只作为排查工具保留。
#include <cmath>
#include <cstdio>
#include <vector>

#include "audio/AudioCapture.h"

using namespace hs;

namespace {
const double kPi = 3.14159265358979323846;

// 静音段：给一点确定性噪声，避免噪声底算成 0
void pushSilence(std::vector<float>& audio, double sec, double amp) {
    const int rate = kSampleRate;
    const int n = static_cast<int>(rate * sec);
    for (int i = 0; i < n; ++i) {
        const double v = amp * (std::sin(i * 0.7) * 0.5 + 0.5) * 0.02;
        audio.push_back(static_cast<float>(v));
    }
}

// 语音段：带音节起伏的 180Hz 基波
void pushSpeech(std::vector<float>& audio, double sec, double amp) {
    const int rate = kSampleRate;
    const int n = static_cast<int>(rate * sec);
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / rate;
        const double mod = 0.55 + 0.45 * std::sin(2 * kPi * 3.5 * t);
        const double v = amp * mod * std::sin(2 * kPi * 180 * t);
        audio.push_back(static_cast<float>(v));
    }
}
}  // namespace

int main() {
    VadProcessor vad;
    int sentences = 0;
    int partials = 0;
    int levels = 0;
    double peakLevel = 0.0;
    double longestSentence = 0.0;

    vad.setCallbacks(
        [&](const std::vector<float>& a) {
            ++sentences;
            const double sec = static_cast<double>(a.size()) / kSampleRate;
            if (sec > longestSentence) longestSentence = sec;
            std::printf("SENTENCE #%d frames=%zu (%.2fs)\n", sentences, a.size(), sec);
        },
        [&](double rms) {
            ++levels;
            if (rms > peakLevel) peakLevel = rms;
        },
        [&](const std::vector<float>&) { ++partials; });

    std::vector<float> audio;
    // 用户真实操作顺序：按下开始 → 环境静音 0.6s → 说话 1.6s → 停顿 1.0s
    pushSilence(audio, 0.6, 0.01);
    pushSpeech(audio, 1.6, 0.12);
    pushSilence(audio, 1.0, 0.01);

    for (size_t off = 0; off + kBlockSize <= audio.size(); off += kBlockSize) {
        vad.process(audio.data() + off, kBlockSize);
    }

    std::printf("levels=%d partials=%d sentences=%d peak=%.4f longest=%.2fs\n", levels,
                partials, sentences, peakLevel, longestSentence);
    // 期望：至少断出 1 句，且句子时长接近 1.6s 而不是被 hang 时间截断
    const bool ok = (sentences >= 1) && (longestSentence > 1.0);
    std::printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

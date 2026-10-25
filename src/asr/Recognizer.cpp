#include "Recognizer.h"

#include <windows.h>

#include <algorithm>
#include <cstring>

#include "sherpa-onnx/c-api/c-api.h"

#include "core/Config.h"
#include "core/Util.h"

namespace hs {

namespace {

// 把 sherpa 的标签去掉 <| |> 包裹
std::string cleanTag(const char* tag) {
    if (!tag) return std::string();
    std::string s(tag);
    s = replaceAll(s, "<|", "");
    s = replaceAll(s, "|>", "");
    return trim(s);
}

}  // namespace

// ---------------------------------------------------------------- Recognizer
Recognizer::Recognizer() = default;

Recognizer::~Recognizer() { unload(); }

bool Recognizer::needsWhisper() const {
    std::string sv, wh;
    inputCodes(langName_, &sv, &wh);
    return sv.empty();
}

void Recognizer::setLanguage(const std::string& displayName) {
    langName_ = displayName;
    if (needsWhisper()) {
        engine_ = AsrEngine::Whisper;
        unload();
    } else if (engine_ == AsrEngine::Whisper) {
        // 切回 SenseVoice
        engine_ = AsrEngine::SenseVoice;
        unload();
    }
}

void Recognizer::setEngine(AsrEngine e) {
    if (engine_ == e) return;
    engine_ = e;
    unload();
}

bool Recognizer::ready() const { return loaded_.load(); }

void Recognizer::unload() {
    std::lock_guard<std::mutex> guard(lock_);
    if (svRecognizer_) {
        SherpaOnnxDestroyOfflineRecognizer(
            static_cast<const SherpaOnnxOfflineRecognizer*>(svRecognizer_));
        svRecognizer_ = nullptr;
    }
    if (whisperRecognizer_) {
        SherpaOnnxDestroyOfflineRecognizer(
            static_cast<const SherpaOnnxOfflineRecognizer*>(whisperRecognizer_));
        whisperRecognizer_ = nullptr;
    }
    loaded_.store(false);
}

bool Recognizer::load(std::string* error) {
    std::lock_guard<std::mutex> guard(lock_);
    if (loaded_.load()) return true;
    bool ok = engine_ == AsrEngine::Whisper ? loadWhisper(error)
                                            : loadSenseVoice(error);
    loaded_.store(ok);
    return ok;
}

bool Recognizer::loadSenseVoice(std::string* error) {
    std::string model = senseVoiceModelPath();
    std::string tokens = senseVoiceTokensPath();
    if (!fileExists(model)) {
        if (error) *error = "找不到 SenseVoice 模型：" + model;
        return false;
    }
    if (!fileExists(tokens)) {
        if (error) *error = "找不到 tokens.txt：" + tokens;
        return false;
    }
    if (onStatus_) onStatus_("正在加载 SenseVoice 模型...");
    DWORD t0 = GetTickCount();

    std::string svCode, whCode;
    inputCodes(langName_, &svCode, &whCode);
    if (svCode.empty()) svCode = "auto";

    SherpaOnnxOfflineSenseVoiceModelConfig sv;
    std::memset(&sv, 0, sizeof(sv));
    sv.model = model.c_str();
    sv.language = svCode.c_str();
    sv.use_itn = 1;

    SherpaOnnxOfflineModelConfig mc;
    std::memset(&mc, 0, sizeof(mc));
    mc.sense_voice = sv;
    mc.tokens = tokens.c_str();
    mc.num_threads = 4;
    mc.debug = 0;
    mc.provider = "cpu";

    SherpaOnnxOfflineRecognizerConfig rc;
    std::memset(&rc, 0, sizeof(rc));
    rc.model_config = mc;

    const SherpaOnnxOfflineRecognizer* rec = SherpaOnnxCreateOfflineRecognizer(&rc);
    if (!rec) {
        if (error) *error = "创建 SenseVoice 识别器失败";
        return false;
    }
    svRecognizer_ = const_cast<SherpaOnnxOfflineRecognizer*>(rec);
    if (onStatus_) {
        onStatus_("SenseVoice 已就绪（" +
                  std::to_string((GetTickCount() - t0) / 1000.0) + "s）");
    }
    return true;
}

bool Recognizer::loadWhisper(std::string* error) {
    // 本版 Whisper 用 sherpa-onnx 的 whisper 模型；若模型缺失则回退 SenseVoice
    std::string dir = findModelDir("whisper");
    std::string encoder = joinPath(dir, "tiny.en-encoder.int8.onnx");
    std::string decoder = joinPath(dir, "tiny.en-decoder.int8.onnx");
    std::string tokens = joinPath(dir, "tiny.en-tokens.txt");

    if (!fileExists(encoder) || !fileExists(decoder) || !fileExists(tokens)) {
        // 没有 Whisper 模型时退回 SenseVoice，保证程序可用
        if (onStatus_) onStatus_("未找到 Whisper 模型，改用 SenseVoice");
        engine_ = AsrEngine::SenseVoice;
        return loadSenseVoice(error);
    }

    if (onStatus_) onStatus_("正在加载 Whisper 模型...");
    SherpaOnnxOfflineWhisperModelConfig wh;
    std::memset(&wh, 0, sizeof(wh));
    wh.encoder = encoder.c_str();
    wh.decoder = decoder.c_str();

    std::string svCode, whCode;
    inputCodes(langName_, &svCode, &whCode);
    if (!whCode.empty()) wh.language = whCode.c_str();
    wh.task = "transcribe";

    SherpaOnnxOfflineModelConfig mc;
    std::memset(&mc, 0, sizeof(mc));
    mc.whisper = wh;
    mc.tokens = tokens.c_str();
    mc.num_threads = 4;
    mc.debug = 0;
    mc.provider = "cpu";

    SherpaOnnxOfflineRecognizerConfig rc;
    std::memset(&rc, 0, sizeof(rc));
    rc.model_config = mc;

    const SherpaOnnxOfflineRecognizer* rec = SherpaOnnxCreateOfflineRecognizer(&rc);
    if (!rec) {
        if (error) *error = "创建 Whisper 识别器失败";
        return false;
    }
    whisperRecognizer_ = const_cast<SherpaOnnxOfflineRecognizer*>(rec);
    if (onStatus_) onStatus_("Whisper 已就绪");
    return true;
}

AsrResult Recognizer::transcribe(const std::vector<float>& audio, std::string* error) {
    if (audio.empty()) return AsrResult();
    if (!loaded_.load()) {
        if (!load(error)) return AsrResult();
    }
    if (engine_ == AsrEngine::Whisper && whisperRecognizer_) {
        return transcribeWhisper(audio, error);
    }
    return transcribeSenseVoice(audio, error);
}

AsrResult Recognizer::transcribeSenseVoice(const std::vector<float>& audio,
                                           std::string* error) {
    if (!svRecognizer_) {
        if (error) *error = "识别器未加载";
        return AsrResult();
    }
    auto* rec = static_cast<const SherpaOnnxOfflineRecognizer*>(svRecognizer_);
    const SherpaOnnxOfflineStream* stream = SherpaOnnxCreateOfflineStream(rec);
    if (!stream) {
        if (error) *error = "创建音频流失败";
        return AsrResult();
    }
    SherpaOnnxAcceptWaveformOffline(stream, 16000, audio.data(),
                                    static_cast<int32_t>(audio.size()));
    SherpaOnnxDecodeOfflineStream(rec, stream);
    const SherpaOnnxOfflineRecognizerResult* r =
        SherpaOnnxGetOfflineStreamResult(stream);

    AsrResult out;
    if (r) {
        out.text = r->text ? r->text : "";
        out.lang = cleanTag(r->lang);
        out.emotion = cleanTag(r->emotion);
        out.event = cleanTag(r->event);
        SherpaOnnxDestroyOfflineRecognizerResult(r);
    }
    SherpaOnnxDestroyOfflineStream(stream);

    out.text = trim(out.text);
    if (hotwordBoost_ && !out.text.empty()) out.text = applyHotwords(out.text);
    return out;
}

AsrResult Recognizer::transcribeWhisper(const std::vector<float>& audio,
                                        std::string* error) {
    if (!whisperRecognizer_) {
        if (error) *error = "Whisper 未加载";
        return AsrResult();
    }
    auto* rec = static_cast<const SherpaOnnxOfflineRecognizer*>(whisperRecognizer_);
    const SherpaOnnxOfflineStream* stream = SherpaOnnxCreateOfflineStream(rec);
    if (!stream) {
        if (error) *error = "创建音频流失败";
        return AsrResult();
    }
    SherpaOnnxAcceptWaveformOffline(stream, 16000, audio.data(),
                                    static_cast<int32_t>(audio.size()));
    SherpaOnnxDecodeOfflineStream(rec, stream);
    const SherpaOnnxOfflineRecognizerResult* r =
        SherpaOnnxGetOfflineStreamResult(stream);

    AsrResult out;
    if (r) {
        out.text = r->text ? r->text : "";
        out.lang = cleanTag(r->lang);
        SherpaOnnxDestroyOfflineRecognizerResult(r);
    }
    SherpaOnnxDestroyOfflineStream(stream);

    out.text = trim(out.text);
    std::string sv, wh;
    inputCodes(langName_, &sv, &wh);
    if (out.lang.empty()) out.lang = wh;
    if (hotwordBoost_ && !out.text.empty()) out.text = applyHotwords(out.text);
    return out;
}

std::string Recognizer::applyHotwords(const std::string& text) const {
    std::vector<std::string> words = hotwordsList();
    if (words.empty()) return text;

    std::string out = text;
    for (const auto& w : words) {
        if (w.size() < 3) continue;  // UTF-8 下至少 1 个汉字
        if (out.find(w) != std::string::npos) continue;
        // 按 UTF-8 字符数比较：允许 1 个字符不同
        // 简化处理：滑动窗口按字节长度匹配
        size_t wlen = w.size();
        if (out.size() < wlen) continue;
        for (size_t i = 0; i + wlen <= out.size(); ++i) {
            std::string seg = out.substr(i, wlen);
            // 只在同一 UTF-8 字符边界上比较
            int diff = 0;
            bool boundaryOk = true;
            for (size_t k = 0; k < wlen; ++k) {
                unsigned char a = static_cast<unsigned char>(seg[k]);
                unsigned char b = static_cast<unsigned char>(w[k]);
                if ((a & 0xC0) == 0x80 && (b & 0xC0) == 0x80) continue;  // 续字节
                if (a != b) ++diff;
                if ((a & 0xC0) == 0x80 && (b & 0xC0) != 0x80) {
                    boundaryOk = false;
                    break;
                }
            }
            if (boundaryOk && diff == 1) {
                out.replace(i, wlen, w);
                break;
            }
        }
    }
    return out;
}

}  // namespace hs

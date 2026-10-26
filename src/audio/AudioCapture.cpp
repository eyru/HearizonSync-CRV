#include "AudioCapture.h"

#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <initguid.h>

// 显式实例化 WASAPI 相关 GUID（避免链接期未解析）
DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D,
            0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46,
            0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(IID_IAudioClient, 0x1CB9AD4C, 0xDBFA, 0x4C32, 0xB1, 0x78, 0xC2, 0xF5,
            0x68, 0xA7, 0x03, 0xB2);
DEFINE_GUID(IID_IAudioCaptureClient, 0xC8ADBD64, 0xE71E, 0x48A0, 0xA4, 0xDE, 0x18,
            0x5C, 0x39, 0x5C, 0xD3, 0x17);

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/Util.h"

namespace hs {

// 采集链路诊断开关：/DHS_AUDIO_DEBUG 编译时把每一步 HRESULT 打到 stderr
#ifdef HS_AUDIO_DEBUG
#define AUD_DBG(...)                          \
    do {                                      \
        std::fprintf(stderr, "[AUD] " __VA_ARGS__); \
        std::fprintf(stderr, "\n");           \
        std::fflush(stderr);                  \
    } while (0)
#else
#define AUD_DBG(...) ((void)0)
#endif

namespace {

constexpr int kVadMinRms = 8;          // 0.008 * 1000
constexpr double kVadMinRmsD = 0.008;
constexpr int kVadHangMs = 700;
constexpr int kVadMinMs = 400;
constexpr int kVadMaxMs = 15000;
constexpr double kStreamWindowS = 6.0;
constexpr double kStreamInterval = 0.32;
constexpr double kStreamMinS = 0.30;

double rmsOf(const float* data, int n) {
    if (!data || n <= 0) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < n; ++i) sum += static_cast<double>(data[i]) * data[i];
    return std::sqrt(sum / n);
}

}  // namespace

// ---------------------------------------------------------------- 降噪 / 增益
void denoiseBuffer(std::vector<float>& audio) {
    const size_t frame = 320;  // 20ms
    if (audio.size() < frame * 2) return;

    size_t n = audio.size() / frame;
    std::vector<double> energy(n);
    for (size_t i = 0; i < n; ++i) {
        energy[i] = rmsOf(audio.data() + i * frame, static_cast<int>(frame));
    }
    std::vector<double> sorted = energy;
    std::sort(sorted.begin(), sorted.end());
    double noise = sorted[static_cast<size_t>(sorted.size() * 0.10)];
    if (noise <= 1e-6) return;

    double gate = noise * 1.1;
    bool touched = false;
    for (size_t i = 0; i < n; ++i) {
        if (energy[i] < gate) {
            for (size_t k = 0; k < frame; ++k) audio[i * frame + k] *= 0.30f;
            touched = true;
        }
    }
    if (!touched) return;

    float peak = 0.0f;
    for (float v : audio) peak = std::max(peak, std::fabs(v));
    if (peak > 0.98f) {
        float g = 0.98f / peak;
        for (auto& v : audio) v *= g;
    }
}

void normalizeGain(std::vector<float>& audio, float target, float maxGain) {
    if (audio.empty()) return;
    double rms = rmsOf(audio.data(), static_cast<int>(audio.size()));
    if (rms <= 1e-5) return;
    double gain = target / rms;
    if (gain <= 1.0) return;
    if (gain > maxGain) gain = maxGain;
    float peak = 0.0f;
    for (auto& v : audio) {
        v = static_cast<float>(v * gain);
        peak = std::max(peak, std::fabs(v));
    }
    if (peak > 0.97f) {
        float g = 0.97f / peak;
        for (auto& v : audio) v *= g;
    }
}

std::vector<float> trimToValley(const std::vector<float>& seg, int searchMs,
                                int stepMs) {
    if (seg.empty()) return seg;
    int search = kSampleRate * searchMs / 1000;
    int step = kSampleRate * stepMs / 1000;
    if (step < 1) step = 1;
    if (static_cast<int>(seg.size()) <= search * 2) return seg;

    int bestPos = 0;
    double bestEnergy = 1e18;
    for (int pos = 0; pos + step <= search; pos += step) {
        double e = rmsOf(seg.data() + pos, step);
        if (e < bestEnergy) {
            bestEnergy = e;
            bestPos = pos;
        }
    }
    if (bestPos <= 0) return seg;
    return std::vector<float>(seg.begin() + bestPos, seg.end());
}

// ---------------------------------------------------------------- VadProcessor
void VadProcessor::reset() {
    buf_.clear();
    tail_.clear();
    speechMs_ = 0;
    silenceMs_ = 0;
    active_ = false;
    lastPartial_ = 0.0;
}

double VadProcessor::noiseFloor() const {
    if (noiseHist_.empty()) return 0.0;
    std::vector<double> s = noiseHist_;
    std::sort(s.begin(), s.end());
    double v = s[static_cast<size_t>(s.size() * 0.20)];
    // 上限保护：再差的底噪也不该把阈值顶到人声之上，
    // 否则安静环境里音量偏小的用户会被全程判成静音。
    return std::min(v, 0.045);
}

double VadProcessor::threshold() const {
    double floorVal = noiseFloor() * 2.4;
    return std::max(kVadMinRmsD, floorVal);
}

void VadProcessor::emitSentence() {
    if (buf_.empty() || speechMs_ < kVadMinMs) {
        buf_.clear();
        tail_.clear();
        speechMs_ = 0;
        silenceMs_ = 0;
        active_ = false;
        return;
    }
    std::vector<float> audio = buf_;
    buf_.clear();
    tail_.clear();
    speechMs_ = 0;
    silenceMs_ = 0;
    active_ = false;
    lastPartial_ = 0.0;

    if (denoiseOn_) {
        denoiseBuffer(audio);
        normalizeGain(audio);
    }
    if (onSentence_) onSentence_(audio);
}

void VadProcessor::process(const float* data, int frames) {
    if (!data || frames <= 0) return;

    double rms = rmsOf(data, frames);
    if (onLevel_) onLevel_(rms);

    // 噪声底只统计「不像语音」的块。旧版本把每一块都塞进历史，
    // 用户一按开始就说话 → 噪声底被自己的声音抬到人声之上 → 阈值锁死，
    // 之后全程判成静音，表现出来同样是「麦克风不收音」。
    const bool voiced = rms > threshold();
    if (!voiced) {
        noiseHist_.push_back(rms);
        if (noiseHist_.size() > 180) noiseHist_.erase(noiseHist_.begin());
    }

    if (voiced) {
        active_ = true;
        silenceMs_ = 0;
        buf_.insert(buf_.end(), data, data + frames);
        tail_.insert(tail_.end(), data, data + frames);
        // 只保留最近 6 秒用于流式
        size_t maxTail = static_cast<size_t>(kSampleRate * kStreamWindowS);
        if (tail_.size() > maxTail) tail_.erase(tail_.begin(), tail_.end() - maxTail);
        speechMs_ += kBlockMs;

        if (onPartial_ && speechMs_ / 1000.0 >= kStreamMinS) {
            double now = nowSeconds();
            if (now - lastPartial_ >= kStreamInterval) {
                lastPartial_ = now;
                if (!tail_.empty()) onPartial_(trimToValley(tail_));
            }
        }
    } else if (active_) {
        buf_.insert(buf_.end(), data, data + frames);
        tail_.insert(tail_.end(), data, data + frames);
        size_t maxTail = static_cast<size_t>(kSampleRate * kStreamWindowS);
        if (tail_.size() > maxTail) tail_.erase(tail_.begin(), tail_.end() - maxTail);
        silenceMs_ += kBlockMs;
    }

    if (active_ && (silenceMs_ >= kVadHangMs || speechMs_ >= kVadMaxMs)) {
        emitSentence();
    }
}

// ---------------------------------------------------------------- 设备枚举
std::vector<AudioDevice> enumerateInputDevices() {
    std::vector<AudioDevice> out;
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                  IID_IMMDeviceEnumerator,
                                  reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) return out;

    // 把已启用、已禁用、未插入、不存在四种状态的录音设备全部列出来。
    // 只列「已启用」会让用户在自己机器上找不到熟悉的那个麦克风。
    IMMDeviceCollection* collection = nullptr;
    hr = enumerator->EnumAudioEndpoints(
        eCapture,
        DEVICE_STATE_ACTIVE | DEVICE_STATE_DISABLED |
            DEVICE_STATE_UNPLUGGED | DEVICE_STATE_NOTPRESENT,
        &collection);
    if (FAILED(hr) || !collection) {
        enumerator->Release();
        return out;
    }

    UINT count = 0;
    collection->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* dev = nullptr;
        if (FAILED(collection->Item(i, &dev)) || !dev) continue;

        AudioDevice d;
        d.index = static_cast<int>(i);
        DWORD st = 0;
        if (SUCCEEDED(dev->GetState(&st))) {
            d.active = (st & DEVICE_STATE_ACTIVE) != 0;
            if (st & DEVICE_STATE_ACTIVE) d.stateText = "已启用";
            else if (st & DEVICE_STATE_DISABLED) d.stateText = "已禁用";
            else if (st & DEVICE_STATE_UNPLUGGED) d.stateText = "未插入";
            else d.stateText = "不存在";
        }

        LPWSTR id = nullptr;
        if (SUCCEEDED(dev->GetId(&id)) && id) {
            d.id = toUtf8(id);
            CoTaskMemFree(id);
        }

        IPropertyStore* props = nullptr;
        if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)) && props) {
            PROPVARIANT v;
            PropVariantInit(&v);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &v)) &&
                v.vt == VT_LPWSTR && v.pwszVal) {
                d.name = toUtf8(v.pwszVal);
            }
            PropVariantClear(&v);
            props->Release();
        }
        if (d.name.empty()) d.name = "Input " + std::to_string(i);
        if (!d.active && !d.stateText.empty())
            d.name += "（" + d.stateText + "）";

        std::string upper = toLower(d.name);
        d.isLoopbackLike = upper.find("cable") != std::string::npos ||
                           d.name.find("立体声混音") != std::string::npos ||
                           upper.find("stereo mix") != std::string::npos;
        out.push_back(d);
        dev->Release();
    }

    collection->Release();
    enumerator->Release();
    return out;
}

// ---------------------------------------------------------------- WASAPI 采集
namespace {
class WasapiCapture {
public:
    bool open(int deviceIndex, std::string* err) {
        IMMDeviceEnumerator* enumerator = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                                      IID_IMMDeviceEnumerator,
                                      reinterpret_cast<void**>(&enumerator));
        if (FAILED(hr)) {
            *err = "无法创建音频设备枚举器";
            return false;
        }

        IMMDeviceCollection* coll = nullptr;
        hr = enumerator->EnumAudioEndpoints(
            eCapture,
            DEVICE_STATE_ACTIVE | DEVICE_STATE_DISABLED |
                DEVICE_STATE_UNPLUGGED | DEVICE_STATE_NOTPRESENT,
            &coll);
        if (FAILED(hr) || !coll) {
            enumerator->Release();
            *err = "无法枚举输入设备";
            return false;
        }
        UINT count = 0;
        coll->GetCount(&count);
        if (deviceIndex < 0 || static_cast<UINT>(deviceIndex) >= count) deviceIndex = 0;

        IMMDevice* dev = nullptr;
        hr = coll->Item(static_cast<UINT>(deviceIndex), &dev);
        coll->Release();
        enumerator->Release();
        if (FAILED(hr) || !dev) {
            *err = "无法打开所选设备";
            return false;
        }

        hr = dev->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr,
                           reinterpret_cast<void**>(&client_));
        dev->Release();
        if (FAILED(hr) || !client_) {
            *err = "无法激活音频客户端";
            return false;
        }

        WAVEFORMATEX* fmt = nullptr;
        hr = client_->GetMixFormat(&fmt);
        if (FAILED(hr) || !fmt) {
            *err = "无法获取设备格式";
            return false;
        }
        srcRate_ = fmt->nSamplesPerSec;
        srcChannels_ = fmt->nChannels;
        srcBits_ = fmt->wBitsPerSample;
        bool isFloat = fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
        if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(fmt);
            isFloat = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != 0;
        }
        srcFloat_ = isFloat;

        REFERENCE_TIME dur = 10000000 / 100;  // 10ms
        hr = client_->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, dur, 0, fmt, nullptr);
        CoTaskMemFree(fmt);
        if (FAILED(hr)) {
            *err = "音频客户端初始化失败";
            return false;
        }

        hr = client_->GetBufferSize(&bufferFrames_);
        if (FAILED(hr)) {
            *err = "无法获取缓冲区大小";
            return false;
        }
        hr = client_->GetService(IID_IAudioCaptureClient,
                                 reinterpret_cast<void**>(&capture_));
        if (FAILED(hr) || !capture_) {
            *err = "无法获取采集接口";
            return false;
        }
        return true;
    }

    // 读取一块并重采样到 16k 单声道；返回 false 表示结束
    bool read(std::vector<float>* out) {
        UINT32 packet = 0;
        HRESULT ghr = capture_->GetNextPacketSize(&packet);
#ifdef HS_AUDIO_DEBUG
        static int dbgNext = 0;
        if (dbgNext < 40) {
            ++dbgNext;
            AUD_DBG("GetNextPacketSize hr=0x%08lX packet=%u",
                    static_cast<unsigned long>(ghr), packet);
        }
#endif
        if (FAILED(ghr)) return false;

        bool got = false;
        while (packet > 0) {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            HRESULT bhr = capture_->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
            AUD_DBG("GetBuffer hr=0x%08lX frames=%u flags=0x%08lX",
                    static_cast<unsigned long>(bhr), frames, flags);
            if (FAILED(bhr)) break;
            if (frames > 0 && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                appendFrames(data, frames);
                got = true;
            } else if (frames > 0) {
                appendSilence(frames);
                got = true;
            }
            capture_->ReleaseBuffer(frames);
            if (FAILED(capture_->GetNextPacketSize(&packet))) break;
        }

        // 重采样到 16k
        if (got) {
            resample(out);
        }
        return got;
    }

    HRESULT start() {
        if (!client_) return E_POINTER;
        return client_->Start();
    }
    void stop() {
        if (client_) client_->Stop();
    }

    void close() {
        if (capture_) { capture_->Release(); capture_ = nullptr; }
        if (client_) { client_->Release(); client_ = nullptr; }
    }

    int sampleRate() const { return srcRate_; }

private:
    void appendFrames(const BYTE* data, UINT32 frames) {
        for (UINT32 i = 0; i < frames; ++i) {
            float sum = 0.0f;
            for (int c = 0; c < srcChannels_; ++c) {
                float v = 0.0f;
                if (srcFloat_) {
                    const float* p = reinterpret_cast<const float*>(data);
                    v = p[i * srcChannels_ + c];
                } else if (srcBits_ == 16) {
                    const int16_t* p = reinterpret_cast<const int16_t*>(data);
                    v = p[i * srcChannels_ + c] / 32768.0f;
                } else if (srcBits_ == 32) {
                    const int32_t* p = reinterpret_cast<const int32_t*>(data);
                    v = static_cast<float>(p[i * srcChannels_ + c] / 2147483648.0);
                }
                sum += v;
            }
            pending_.push_back(sum / srcChannels_);
        }
    }

    void appendSilence(UINT32 frames) {
        pending_.insert(pending_.end(), frames, 0.0f);
    }

    void resample(std::vector<float>* out) {
        if (srcRate_ == kSampleRate) {
            out->swap(pending_);
            pending_.clear();
            return;
        }
        // 简单线性插值重采样
        double ratio = static_cast<double>(srcRate_) / kSampleRate;
        out->clear();
        size_t n = static_cast<size_t>(pending_.size() / ratio);
        out->reserve(n);
        for (size_t i = 0; i < n; ++i) {
            double pos = i * ratio;
            size_t idx = static_cast<size_t>(pos);
            double frac = pos - idx;
            float a = idx < pending_.size() ? pending_[idx] : 0.0f;
            float b = (idx + 1) < pending_.size() ? pending_[idx + 1] : a;
            out->push_back(static_cast<float>(a + (b - a) * frac));
        }
        pending_.clear();
    }

    IAudioClient* client_ = nullptr;
    IAudioCaptureClient* capture_ = nullptr;
    UINT32 bufferFrames_ = 0;
    int srcRate_ = 48000;
    int srcChannels_ = 2;
    int srcBits_ = 32;
    bool srcFloat_ = true;
    std::vector<float> pending_;
};
}  // namespace

AudioCapture::AudioCapture() = default;

AudioCapture::~AudioCapture() { stop(); }

std::string AudioCapture::start(int deviceIndex) {
    if (running_.load()) return "已在运行";
    deviceIndex_ = deviceIndex;
    stopFlag_.store(false);
    lastError_.clear();
    startupDone_.store(false);
    running_.store(true);
    thread_ = std::thread([this]() { run(); });

    // 等后台线程完成 open + Start（正常几十毫秒），把失败原因同步返回给界面。
    // 超时也继续跑：慢机器上首次打开设备可能偏慢，不该因此误报失败。
    for (int i = 0; i < 60 && !startupDone_.load(); ++i) Sleep(10);

    std::string err = lastError_;
    if (!err.empty()) {
        stopFlag_.store(true);
        if (thread_.joinable()) thread_.join();
        running_.store(false);
        return err;
    }
    return std::string();
}

void AudioCapture::stop() {
    stopFlag_.store(true);
    if (thread_.joinable()) thread_.join();
    running_.store(false);
}

void AudioCapture::run() {
    HRESULT cohr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    AUD_DBG("run enter coinit=0x%08lX devIndex=%d",
            static_cast<unsigned long>(cohr), deviceIndex_);
    WasapiCapture cap;
    std::string err;
    if (!cap.open(deviceIndex_, &err)) {
        AUD_DBG("open FAILED: %s", err.c_str());
        lastError_ = err;
        running_.store(false);
        startupDone_.store(true);
        CoUninitialize();
        return;
    }
    AUD_DBG("open ok srcRate=%d", cap.sampleRate());
    HRESULT shr = cap.start();
    AUD_DBG("Start hr=0x%08lX", static_cast<unsigned long>(shr));
    if (FAILED(shr)) {
        char buf[80];
        std::snprintf(buf, sizeof(buf), "音频流启动失败（0x%08lX）",
                      static_cast<unsigned long>(shr));
        lastError_ = buf;
        cap.close();
        running_.store(false);
        startupDone_.store(true);
        CoUninitialize();
        return;
    }

    startupDone_.store(true);
    std::vector<float> chunk;
    std::vector<float> pending;  // 跨 read 累积，拼满 30ms 再喂 VAD
    pending.reserve(static_cast<size_t>(kBlockSize) * 8);
    int dbgIter = 0;
    while (!stopFlag_.load()) {
        chunk.clear();
        bool okRead = cap.read(&chunk);
        if (dbgIter < 12) {
            AUD_DBG("loop#%d read=%d size=%d", dbgIter, static_cast<int>(okRead),
                    static_cast<int>(chunk.size()));
        }
        ++dbgIter;
        if (okRead && !chunk.empty()) {
            // 单次 WASAPI 包重采样后通常只有 160~320 帧，小于 30ms 的 480 帧；
            // 直接按 480 切片会一次都切不出来（旧版本就是这么让麦克风"不收音"的）。
            pending.insert(pending.end(), chunk.begin(), chunk.end());
            size_t off = 0;
            for (; off + static_cast<size_t>(kBlockSize) <= pending.size();
                 off += static_cast<size_t>(kBlockSize)) {
                vad_.process(pending.data() + off, kBlockSize);
            }
            if (off > 0) pending.erase(pending.begin(), pending.begin() + off);
            // 兜底：万一本机时钟异常导致缓冲失控，丢掉旧数据重新积累
            if (pending.size() > static_cast<size_t>(kBlockSize) * 64) pending.clear();
        } else {
            Sleep(5);
        }
    }
    AUD_DBG("run exit iters=%d", dbgIter);

    cap.stop();
    cap.close();
    running_.store(false);
    CoUninitialize();
}

bool AudioCapture::testMic(int deviceIndex, int durationMs, double* rms, double* peak,
                           std::string* error) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    WasapiCapture cap;
    std::string err;
    if (!cap.open(deviceIndex, &err)) {
        if (error) *error = err;
        CoUninitialize();
        return false;
    }
    HRESULT testHr = cap.start();
    if (FAILED(testHr)) {
        cap.close();
        if (error) *error = "音频流启动失败";
        CoUninitialize();
        return false;
    }

    double sumSq = 0.0;
    long long total = 0;
    float maxAbs = 0.0f;
    std::vector<float> chunk;
    DWORD startTick = GetTickCount();
    while (static_cast<int>(GetTickCount() - startTick) < durationMs) {
        chunk.clear();
        if (cap.read(&chunk) && !chunk.empty()) {
            for (float v : chunk) {
                sumSq += static_cast<double>(v) * v;
                maxAbs = std::max(maxAbs, std::fabs(v));
            }
            total += static_cast<long long>(chunk.size());
        } else {
            Sleep(5);
        }
    }
    cap.stop();
    cap.close();
    CoUninitialize();

    if (rms) *rms = total > 0 ? std::sqrt(sumSq / total) : 0.0;
    if (peak) *peak = maxAbs;
    return true;
}

}  // namespace hs

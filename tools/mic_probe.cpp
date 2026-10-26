// 麦克风诊断工具：列出所有输入设备（含被禁用/未插入状态），并对每个可用设备做 2 秒采集，报告音量。
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <cstdio>
#include <string>
#include <vector>
#include <cmath>

DEFINE_GUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT_, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71);
DEFINE_GUID(KSDATAFORMAT_SUBTYPE_PCM_, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71);

static std::string toUtf8(const wchar_t* w) {
    if (!w) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    return s;
}

static const char* stateName(DWORD st) {
    switch (st) {
        case DEVICE_STATE_ACTIVE: return "启用";
        case DEVICE_STATE_DISABLED: return "已禁用";
        case DEVICE_STATE_NOTPRESENT: return "不存在";
        case DEVICE_STATE_UNPLUGGED: return "未插入";
        default: return "其它";
    }
}

// 对单个设备采集 2 秒，返回峰值 RMS（0..1），失败返回 -1
static double probeLevel(IMMDevice* dev, std::string* info) {
    IAudioClient* client = nullptr;
    HRESULT hr = dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client));
    if (FAILED(hr) || !client) { *info = "无法激活音频客户端"; return -1; }

    WAVEFORMATEX* fmt = nullptr;
    hr = client->GetMixFormat(&fmt);
    if (FAILED(hr) || !fmt) { client->Release(); *info = "无法取得设备格式"; return -1; }

    bool isFloat = fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    if (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        auto* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(fmt);
        isFloat = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != 0;
    }
    char buf[256];
    sprintf_s(buf, "%u Hz / %u 声道 / %u bit / %s", fmt->nSamplesPerSec, fmt->nChannels,
              fmt->wBitsPerSample, isFloat ? "float" : "pcm");

    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, fmt, nullptr);
    if (FAILED(hr)) {
        sprintf_s(buf + strlen(buf), sizeof(buf) - strlen(buf), " | 初始化失败 0x%08lX", hr);
        *info = buf;
        CoTaskMemFree(fmt);
        client->Release();
        return -1;
    }

    IAudioCaptureClient* cap = nullptr;
    hr = client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&cap));
    if (FAILED(hr) || !cap) {
        *info = buf;
        CoTaskMemFree(fmt);
        client->Release();
        return -1;
    }

    client->Start();
    double peak = 0.0;
    double sumSq = 0.0;
    long long total = 0;
    DWORD deadline = GetTickCount() + 2000;
    while (GetTickCount() < deadline) {
        Sleep(50);
        UINT32 packet = 0;
        if (FAILED(cap->GetNextPacketSize(&packet))) break;
        while (packet > 0) {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            if (FAILED(cap->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data) {
                if (isFloat && fmt->wBitsPerSample == 32) {
                    auto* p = reinterpret_cast<float*>(data);
                    for (UINT32 i = 0; i < frames * fmt->nChannels; ++i) {
                        double v = std::fabs(p[i]);
                        if (v > peak) peak = v;
                        sumSq += v * v;
                        ++total;
                    }
                } else if (!isFloat && fmt->wBitsPerSample == 16) {
                    auto* p = reinterpret_cast<short*>(data);
                    for (UINT32 i = 0; i < frames * fmt->nChannels; ++i) {
                        double v = std::fabs(p[i]) / 32768.0;
                        if (v > peak) peak = v;
                        sumSq += v * v;
                        ++total;
                    }
                }
            }
            cap->ReleaseBuffer(frames);
            if (FAILED(cap->GetNextPacketSize(&packet))) break;
        }
    }
    client->Stop();

    double rms = total > 0 ? std::sqrt(sumSq / total) : 0.0;
    sprintf_s(buf + strlen(buf), sizeof(buf) - strlen(buf),
              " | 峰值 %.4f RMS %.4f 采样点 %lld", peak, rms, total);
    *info = buf;

    cap->Release();
    CoTaskMemFree(fmt);
    client->Release();
    return peak;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    IMMDeviceEnumerator* enumr = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumr));
    if (FAILED(hr) || !enumr) {
        printf("无法创建音频设备枚举器（0x%08lX）\n", hr);
        return 1;
    }

    // 先看默认设备
    IMMDevice* def = nullptr;
    if (SUCCEEDED(enumr->GetDefaultAudioEndpoint(eCapture, eConsole, &def)) && def) {
        IPropertyStore* ps = nullptr;
        std::string name = "(未知)";
        if (SUCCEEDED(def->OpenPropertyStore(STGM_READ, &ps)) && ps) {
            PROPVARIANT v;
            PropVariantInit(&v);
            if (SUCCEEDED(ps->GetValue(PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR)
                name = toUtf8(v.pwszVal);
            PropVariantClear(&v);
            ps->Release();
        }
        printf("系统默认录音设备: %s\n\n", name.c_str());
        def->Release();
    } else {
        printf("系统没有默认录音设备\n\n");
    }

    int activeIndex = 0;
    int totalFound = 0;
    int shown = 0;
    // 四种状态全查，这样能看到被禁用的麦克风
    const DWORD states[4] = {DEVICE_STATE_ACTIVE, DEVICE_STATE_DISABLED,
                             DEVICE_STATE_NOTPRESENT, DEVICE_STATE_UNPLUGGED};
    for (DWORD st : states) {
        IMMDeviceCollection* coll = nullptr;
        hr = enumr->EnumAudioEndpoints(eCapture, st, &coll);
        if (FAILED(hr) || !coll) continue;
        UINT count = 0;
        coll->GetCount(&count);
        for (UINT i = 0; i < count; ++i) {
            IMMDevice* dev = nullptr;
            if (FAILED(coll->Item(i, &dev)) || !dev) continue;
            ++totalFound;
            ++shown;

            IPropertyStore* ps = nullptr;
            std::string name = "(无名)";
            if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &ps)) && ps) {
                PROPVARIANT v;
                PropVariantInit(&v);
                if (SUCCEEDED(ps->GetValue(PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR)
                    name = toUtf8(v.pwszVal);
                PropVariantClear(&v);
                ps->Release();
            }

            printf("[%d] %s\n", activeIndex, name.c_str());
            printf("    状态: %s\n", stateName(st));
            if (st == DEVICE_STATE_ACTIVE) {
                std::string info;
                double peak = probeLevel(dev, &info);
                printf("    格式: %s\n", info.c_str());
                if (peak >= 0) {
                    if (peak < 0.001)
                        printf("    结论: 几乎没有声音（检查是否静音/被占用/麦克风权限）\n");
                    else if (peak < 0.02)
                        printf("    结论: 有微弱信号，请对着麦克风说话再测一次\n");
                    else
                        printf("    结论: 采到声音，设备工作正常\n");
                }
                ++activeIndex;
            } else {
                printf("    结论: 该状态不会被软件列出，需要先在 Windows 声音设置里启用\n");
            }
            printf("\n");
            dev->Release();
        }
        coll->Release();
    }

    if (totalFound == 0) printf("一个输入设备都没找到（连被禁用的也没有）\n");
    printf("设备总数（含未启用）: %d\n", totalFound);
    printf("可用录音设备数: %d\n", activeIndex);

    enumr->Release();
    CoUninitialize();
    return 0;
}

#include "core/Gpu.h"

#include <windows.h>

#include <cstdio>
#include <mutex>

#include "core/Util.h"

// ctranslate2 的 C++ 接口；设备枚举是轻量的，不会真加载 CUDA
#include <ctranslate2/devices.h>
#include <ctranslate2/translator.h>

namespace hs {
namespace gpu {
namespace {

struct Cache {
    bool probed = false;
    bool has = false;
};
Cache g_cache;
std::mutex g_mtx;

bool fileExists(const char* path) {
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// 注册表里有没有 NVIDIA 的驱动项：比 wmic 快得多，也不依赖 wmic 还在不在
bool registryHasNvidia() {
    const char* keys[] = {
        "SOFTWARE\\NVIDIA Corporation",
        "SOFTWARE\\WOW6432Node\\NVIDIA Corporation",
    };
    for (const char* k : keys) {
        HKEY h = nullptr;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, k, 0, KEY_READ, &h) == ERROR_SUCCESS) {
            RegCloseKey(h);
            return true;
        }
    }
    return false;
}

int cudaDeviceCount() {
    try {
        return ctranslate2::get_device_count(ctranslate2::Device::CUDA);
    } catch (...) {
        return 0;
    }
}

}  // namespace

bool nvidiaPresent() {
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_cache.probed) return g_cache.has;

    bool has = false;
    // 1) 驱动装好的 N 卡：ctranslate2 能枚举到 CUDA 设备
    if (cudaDeviceCount() > 0) has = true;
    // 2) nvidia-smi 存在（驱动装了，但 CUDA 运行时可能没装）
    if (!has) {
        if (fileExists("C:\\Windows\\System32\\nvidia-smi.exe") ||
            fileExists("C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvidia-smi.exe")) {
            has = true;
        }
    }
    // 3) 注册表兜底
    if (!has) has = registryHasNvidia();

    g_cache.has = has;
    g_cache.probed = true;
    return has;
}

ProbeResult probe(const std::string& opusModelDir) {
    ProbeResult r;

    int cnt = cudaDeviceCount();
    r.hasCard = cnt > 0 || nvidiaPresent();

    if (cnt <= 0) {
        if (r.hasCard) {
            r.usable = false;
            r.msg = "检测到 NVIDIA 显卡，但显卡驱动 / CUDA 不可用";
        } else {
            r.usable = false;
            r.msg = "本机没有可用的 NVIDIA 显卡";
        }
        return r;
    }

    // 必须真跑一次推理：创建 Translator 是懒加载，不碰 CUDA 就不会报缺 DLL
    try {
        ctranslate2::Translator tr(opusModelDir, ctranslate2::Device::CUDA,
                                   ctranslate2::ComputeType::INT8_FLOAT16);
        std::vector<std::vector<std::string>> batch{{"hello", "</s>"}};
        ctranslate2::TranslationOptions opt;
        opt.beam_size = 1;
        opt.max_decoding_length = 16;
        tr.translate_batch(batch, opt);
        r.usable = true;
        r.msg = "CUDA 可用";
    } catch (const std::exception& e) {
        std::string low = e.what();
        for (auto& ch : low) ch = static_cast<char>(::tolower(static_cast<unsigned char>(ch)));
        if (low.find("cublas") != std::string::npos || low.find("cudnn") != std::string::npos ||
            low.find("cudart") != std::string::npos) {
            r.usable = false;
            r.msg = "缺少 CUDA 12 运行时（cublas64_12.dll / cudnn），需安装 CUDA Toolkit 12.x";
        } else {
            r.usable = false;
            std::string what = e.what();
            r.msg = what.size() > 90 ? what.substr(0, 90) : what;
        }
    }
    return r;
}

std::string quickSummary() {
    if (cudaDeviceCount() > 0) {
        return "本机 GPU 检测：已检测到 NVIDIA 显卡，GPU 加速可用";
    }
    if (nvidiaPresent()) {
        return "本机 GPU 检测：检测到 NVIDIA 显卡，但缺 CUDA 12 运行时，需装 CUDA Toolkit 12.x 才能启用";
    }
    return "本机 GPU 检测：未检测到 NVIDIA 显卡 → GPU 加速用不了，全部走 CPU（不影响使用）";
}

}  // namespace gpu
}  // namespace hs

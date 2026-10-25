#pragma once

#include <string>

namespace hs {
namespace gpu {

// 本机到底有没有 NVIDIA 显卡（不看 CUDA 运行时装没装）。
// 三重判断：ctranslate2 能枚举到设备 / nvidia-smi 存在 / 注册表有 NVIDIA 项。
// 结果缓存，多次调用只探测一次。
bool nvidiaPresent();

struct ProbeResult {
    bool usable = false;   // CUDA 是否真的能用
    bool hasCard = false;  // 有没有 NVIDIA 显卡（哪怕驱动不全）
    std::string msg;       // 给用户看的一句话说明
};

// 真实探测：先看设备数，再用一个小模型真跑一次推理。
// 只有真跑过才知道缺不缺 cublas64_12.dll / cudnn。
ProbeResult probe(const std::string& opusModelDir);

// 界面展示用，不做推理测试，只回答「有没有 N 卡、能不能用」。
std::string quickSummary();

}  // namespace gpu
}  // namespace hs

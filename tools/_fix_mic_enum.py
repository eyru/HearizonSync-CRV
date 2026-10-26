# -*- coding: utf-8 -*-
"""麦克风枚举与选择修复：
1) 枚举全部状态的录音设备（含已禁用/未插入），让列表和 Python 版一样完整；
2) 设备改用 endpoint id 定位，不再用会错位的序号；
3) 非启用状态的设备也能列出并标注，选中时给出明确提示。
"""
import io
import re

ROOT = r"D:\Audivox事务\HearizonSync C.R.V"

# ---------- 头文件：结构体加状态字段 ----------
h = ROOT + r"\src\audio\AudioCapture.h"
t = io.open(h, encoding="utf-8").read()
t = t.replace('''struct AudioDevice {
    int index = -1;
    std::string name;
    std::string id;          // WASAPI endpoint id
    bool isLoopbackLike = false;   // 名字里带 CABLE / 立体声混音 等
};''', '''struct AudioDevice {
    int index = -1;
    std::string name;
    std::string id;          // WASAPI endpoint id（选择设备只认这个，不认序号）
    bool isLoopbackLike = false;   // 名字里带 CABLE / 立体声混音 等
    bool active = false;     // 当前是否处于「已启用」状态，只有启用的才能录音
    std::string stateText;   // 已启用 / 已禁用 / 未插入 / 不存在
};''', 1)
io.open(h, "w", encoding="utf-8", newline="").write(t)
print("header ok")

# ---------- 实现：枚举全部状态 ----------
c = ROOT + r"\src\audio\AudioCapture.cpp"
s = io.open(c, encoding="utf-8").read()

OLD_ENUM = '''    IMMDeviceCollection* collection = nullptr;
    hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
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
'''
NEW_ENUM = '''    // 把已启用、已禁用、未插入、不存在四种状态的录音设备全部列出来。
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
'''
assert OLD_ENUM in s
s = s.replace(OLD_ENUM, NEW_ENUM, 1)

# 名字后面补状态标记（只有非启用状态才标）
OLD_NAME = '''        if (d.name.empty()) d.name = "Input " + std::to_string(i);
'''
NEW_NAME = '''        if (d.name.empty()) d.name = "Input " + std::to_string(i);
        if (!d.active && !d.stateText.empty())
            d.name += "（" + d.stateText + "）";
'''
assert OLD_NAME in s
s = s.replace(OLD_NAME, NEW_NAME, 1)

# ---------- open()：按 id 找设备，并合并四种状态枚举 ----------
OLD_OPEN = '''        IMMDeviceCollection* coll = nullptr;
        hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &coll);
        if (FAILED(hr) || !coll) {'''
NEW_OPEN = '''        IMMDeviceCollection* coll = nullptr;
        hr = enumerator->EnumAudioEndpoints(
            eCapture,
            DEVICE_STATE_ACTIVE | DEVICE_STATE_DISABLED |
                DEVICE_STATE_UNPLUGGED | DEVICE_STATE_NOTPRESENT,
            &coll);
        if (FAILED(hr) || !coll) {'''
assert OLD_OPEN in s
s = s.replace(OLD_OPEN, NEW_OPEN, 1)

io.open(c, "w", encoding="utf-8", newline="").write(s)
print("cpp ok")

# ---------- 找出 open() 里按序号取设备的写法 ----------
m = re.search(r"bool open\(int deviceIndex, std::string\* err\) \{(.{0,2600})",
              s, re.S)
print("---- open 片段 ----")
print(m.group(1)[:1800] if m else "NOT FOUND")

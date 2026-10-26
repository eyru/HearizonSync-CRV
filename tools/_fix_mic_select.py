# -*- coding: utf-8 -*-
"""让设备下拉的行为更符合直觉：
1) 默认只挑「已启用」的设备，不会落到已禁用或虚拟声卡上；
2) 记住用户上次选的设备 id，下次按 id 还原，热插拔后也不会错位；
3) 当前选中设备不可用时，状态栏直接说清楚。
"""
import io

ROOT = r"D:\Audivox事务\HearizonSync C.R.V"

# ---------------- MainWindow.cpp ----------------
P = ROOT + r"\src\ui\MainWindow.cpp"
s = io.open(P, encoding="utf-8").read()

OLD = '''void MainWindow::refreshDevices() {
    auto devices = enumerateInputDevices();
    std::vector<std::string> names;
    for (const auto& d : devices) {
        std::string n = "[" + std::to_string(d.index) + "] " + d.name;
        if (d.isLoopbackLike) n += "  【系统声音】";
        names.push_back(n);
    }
    deviceBox_.setItems(names);
    if (!names.empty()) {
        // 优先选中物理麦克风，别默认落到虚拟声卡上（虚拟声卡没有音频流时永远是静音）
        int pick = 0;
        for (size_t i = 0; i < devices.size(); ++i) {
            if (!devices[i].isLoopbackLike) { pick = static_cast<int>(i); break; }
        }
        deviceBox_.setSelected(pick);
    }
    setStatus("已刷新设备：" + std::to_string(names.size()) + " 个", theme().accent);
}'''

NEW = '''void MainWindow::refreshDevices() {
    devices_ = enumerateInputDevices();
    std::vector<std::string> names;
    for (const auto& d : devices_) {
        std::string n = "[" + std::to_string(d.index) + "] " + d.name;
        if (d.isLoopbackLike) n += "  【系统声音】";
        names.push_back(n);
    }
    deviceBox_.setItems(names);
    if (names.empty()) {
        setStatus("没找到任何录音设备", theme().danger);
        return;
    }
    // 先按上次保存的 id 还原，找不到再按「已启用 + 非虚拟声卡」的顺序挑
    int pick = -1;
    const std::string want = config().micDeviceId;
    if (!want.empty()) {
        for (size_t i = 0; i < devices_.size(); ++i) {
            if (devices_[i].id == want) { pick = static_cast<int>(i); break; }
        }
    }
    if (pick < 0) {
        for (size_t i = 0; i < devices_.size(); ++i) {
            if (devices_[i].active && !devices_[i].isLoopbackLike) {
                pick = static_cast<int>(i);
                break;
            }
        }
    }
    if (pick < 0) {
        for (size_t i = 0; i < devices_.size(); ++i) {
            if (devices_[i].active) { pick = static_cast<int>(i); break; }
        }
    }
    if (pick < 0) pick = 0;
    deviceBox_.setSelected(pick);

    int usable = 0;
    for (const auto& d : devices_) if (d.active) ++usable;
    std::string msg = "已刷新设备：" + std::to_string(names.size()) + " 个（可用 " +
                      std::to_string(usable) + "）";
    const AudioDevice& cur = devices_[static_cast<size_t>(pick)];
    if (!cur.active)
        msg += "　当前选中的不可用，请到系统声音设置里启用";
    setStatus(msg, theme().accent);
}'''

assert OLD in s, "refreshDevices 原文不匹配"
s = s.replace(OLD, NEW, 1)

# 补上设备切换的回调（原来根本没设，选了也不记）
OLD_CALL = "    // 设备\n    refreshDevices();"
NEW_CALL = '''    // 设备
    deviceBox_.setOnChange([this](int i) {
        if (i >= 0 && static_cast<size_t>(i) < devices_.size()) {
            config().micDeviceId = devices_[static_cast<size_t>(i)].id;
            saveConfig();
        }
    });
    refreshDevices();'''
assert OLD_CALL in s, "找不到 refreshDevices 调用点"
s = s.replace(OLD_CALL, NEW_CALL, 1)

io.open(P, "w", encoding="utf-8", newline="").write(s)
print("MainWindow.cpp ok")

# ---------------- MainWindow.h ----------------
H = ROOT + r"\src\ui\MainWindow.h"
ht = io.open(H, encoding="utf-8").read()
if "devices_" not in ht:
    ht = ht.replace("#pragma once", '#pragma once\n\n#include "audio/AudioCapture.h"', 1)
    # 插在第一个私有成员块之前
    for anchor in ("    bool running_ = false;", "    SUBTOPIC", "    HWND hwnd_ = nullptr;"):
        if anchor in ht:
            ht = ht.replace(anchor, "    std::vector<AudioDevice> devices_;  // 当前枚举到的输入设备\n" + anchor, 1)
            break
    else:
        ht = ht.replace("private:", "private:\n    std::vector<AudioDevice> devices_;", 1)
    io.open(H, "w", encoding="utf-8", newline="").write(ht)
    print("MainWindow.h ok")
else:
    print("MainWindow.h already ok")

# ---------------- Config.h ----------------
CP = ROOT + r"\src\core\Config.h"
ct = io.open(CP, encoding="utf-8").read()
if "micDeviceId" not in ct:
    ct = ct.replace("    std::string subPos;",
                    "    std::string subPos;\n    std::string micDeviceId;   // 上次选中的录音设备 endpoint id", 1)
    io.open(CP, "w", encoding="utf-8", newline="").write(ct)
    print("Config.h ok")

# ---------------- Config.cpp ----------------
CC = ROOT + r"\src\core\Config.cpp"
cc = io.open(CC, encoding="utf-8").read()
if "mic_device_id" not in cc:
    cc = cc.replace('    c.subPos = j.getString("sub_pos", "bottom");',
                    '    c.subPos = j.getString("sub_pos", "bottom");\n'
                    '    c.micDeviceId = j.getString("mic_device_id", "");', 1)
    cc = cc.replace('    j.set("sub_pos", c.subPos);',
                    '    j.set("sub_pos", c.subPos);\n'
                    '    j.set("mic_device_id", c.micDeviceId);', 1)
    io.open(CC, "w", encoding="utf-8", newline="").write(cc)
    print("Config.cpp ok")

print("done")

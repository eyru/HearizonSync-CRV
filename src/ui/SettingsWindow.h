// Hearizon Sync C.R.V. - 设置窗口
#pragma once

#include <windows.h>

namespace hs {

// 打开设置窗口（自带消息循环，返回 true 表示用户点了保存）
bool showSettings(HWND owner);

}  // namespace hs

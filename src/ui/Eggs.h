// Hearizon Sync C.R.V. - 独家模块（源码中不出现触发词明文）
#pragma once

#include <windows.h>

#include <string>

namespace hs {
namespace eggs {

// 触发词命中则弹出对应模块并返回 true（调用方负责清空输入框）
bool handleInput(HWND owner, const std::string& text);

// 四个模块
void showAbout(HWND owner);      // 模块 1 的载体：关于窗口（连点版本号 5 次）
void showThanks(HWND owner);     // 模块 2-2
void showPoem(HWND owner);       // 模块 2-1
void showDeveloper(HWND owner);  // 模块 2

// 首次运行引导（只弹一次）
void showWelcome(HWND owner);

}  // namespace eggs
}  // namespace hs

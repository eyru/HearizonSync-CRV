// Hearizon Sync C.R.V. - 单实例控制
#pragma once

#include <windows.h>

#include <string>

namespace hs {

class SingleInstance {
public:
    // 获取互斥量；返回 false 表示已有实例在运行
    static bool acquire(const wchar_t* name);
    static void release();

    // 激活已存在的主窗口
    static void activateExisting(const wchar_t* windowClass);

private:
    static HANDLE mutex_;
};

}  // namespace hs

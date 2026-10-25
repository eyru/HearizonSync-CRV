// 验证资源里的图标能否被加载（最简版本）
#include <windows.h>
#include <cstdio>

int wmain() {
    FILE* f = nullptr;
    _wfopen_s(&f, L"build/probe_icon.log", L"w, ccs=UTF-8");
    if (!f) return 1;

    HMODULE mod = GetModuleHandleW(nullptr);
    fwprintf(f, L"module=%p\n", (void*)mod);

    struct Item {
        LPCWSTR name;
        const wchar_t* label;
    };
    Item items[] = {
        {L"APPICON", L"name APPICON"},
        {L"APPLIVE", L"name APPLIVE"},
        {MAKEINTRESOURCEW(1), L"ordinal 1"},
        {MAKEINTRESOURCEW(2), L"ordinal 2"},
        {MAKEINTRESOURCEW(3), L"ordinal 3"},
    };
    for (const Item& it : items) {
        HICON a = (HICON)LoadImageW(mod, it.name, IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
        HICON b = LoadIconW(mod, it.name);
        fwprintf(f, L"%-16s LoadImage=%p LoadIcon=%p\n", it.label, (void*)a, (void*)b);
        if (a) DestroyIcon(a);
    }

    for (int type = 1; type <= 24; ++type) {
        int count = 0;
        EnumResourceNamesW(mod, MAKEINTRESOURCEW(type),
                           [](HMODULE, LPCWSTR, LPWSTR, LONG_PTR p) -> BOOL {
                               ++*reinterpret_cast<int*>(p);
                               return TRUE;
                           },
                           reinterpret_cast<LONG_PTR>(&count));
        if (count > 0) fwprintf(f, L"resource type %d: %d entries\n", type, count);
    }

    fclose(f);
    return 0;
}

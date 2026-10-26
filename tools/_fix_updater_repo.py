# -*- coding: utf-8 -*-
"""严重问题修复：C.R.V. 版一直在查 Python 版的更新接口。

Updater.cpp 里 apiUrl()/pageUrl() 指向 eyru/HearizonSync（Python 版仓库），
所以 C.R.V. 用户会被提示「发现新版本 v2.5.7」，点下去下载的是 Python 版安装包，
装完就把 C.R.V. 覆盖成 Python 版了。

修法：
1) 地址改到 C.R.V. 自己的仓库；
2) 资产筛选只认名字里带 crv 的，地址再写错也不会串味；
3) 下载落地文件名带 CRV，方便区分。
"""
import io

ROOT = r"D:\Audivox事务\HearizonSync C.R.V"
P = ROOT + r"\src\platform\Updater.cpp"
s = io.open(P, encoding="utf-8").read()

s = s.replace(
    'return "https://api.github.com/repos/eyru/HearizonSync/releases/latest";',
    '// 必须指向 C.R.V. 自己的仓库。写错成 Python 版仓库会让用户被引导去装 Python 版。\n'
    '    return "https://api.github.com/repos/eyru/HearizonSync-CRV/releases/latest";', 1)

s = s.replace(
    'return "https://github.com/eyru/HearizonSync/releases/latest";',
    'return "https://github.com/eyru/HearizonSync-CRV/releases/latest";', 1)

OLD_PICK = '''    const Json* assets = j.find("assets");
    if (assets) {
        for (size_t i = 0; i < assets->size(); ++i) {
            const Json* a = assets->at(i);
            if (!a) continue;
            std::string name = toLower(a->getString("name"));
            if (endsWith(name, ".exe")) {
                info.downloadUrl = a->getString("browser_download_url");
                break;
            }
        }
    }
'''
NEW_PICK = '''    // 只认 C.R.V. 自己的安装包。即使仓库地址将来又被写错，也不会把别的版本装进来。
    const Json* assets = j.find("assets");
    if (assets) {
        for (size_t i = 0; i < assets->size(); ++i) {
            const Json* a = assets->at(i);
            if (!a) continue;
            std::string name = toLower(a->getString("name"));
            if (!endsWith(name, ".exe")) continue;
            if (name.find("crv") == std::string::npos) continue;
            info.downloadUrl = a->getString("browser_download_url");
            break;
        }
    }
    if (info.downloadUrl.empty()) return;   // 没有对应的 C.R.V. 资产就不提示更新
'''
assert OLD_PICK in s, "资产选择段不匹配"
s = s.replace(OLD_PICK, NEW_PICK, 1)

s = s.replace('std::string path = joinPath(tempDir(), "HearizonSync-Setup-" + safeTag + ".exe");',
              'std::string path = joinPath(tempDir(), "HearizonSync-CRV-Setup-" + safeTag + ".exe");', 1)

io.open(P, "w", encoding="utf-8", newline="").write(s)
print("Updater.cpp ok")

# 顺带确认没有别处还硬编码 Python 仓库
import re
for f in ("src/platform/Updater.h", "src/main.cpp", "src/core/Strings.cpp"):
    t = io.open(ROOT + "\\" + f.replace("/", "\\"), encoding="utf-8").read()
    hits = [l for l in t.splitlines()
            if "github.com" in l and "CRV" not in l and "crv" not in l]
    if hits:
        print("注意", f, hits)
print("done")

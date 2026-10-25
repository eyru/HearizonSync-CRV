"""创建 Release 并上传安装包资产（分块 POST，避免一次性读入 312MB）。"""
import http.client
import json
import os
import subprocess
import sys
import time

REPO = "eyru/HearizonSync-CRV"
TAG = "v2.5.6"
ASSET = os.path.join("installer", "HearizonSync-CRV-Setup-2.5.6.exe")

BODY = """## Hearizon Sync C.R.V. v2.5.6

**CppRebuildVersion** —— 用 C++17 + Win32 完整重写的 Hearizon Sync。

功能、界面、版本号与原 Python 版保持一致，底层全部换为原生实现。

### 体积对比

| 指标 | Python 版 | C++ 版 | 变化 |
|---|---|---|---|
| 主程序 | 9,545,855 B | **964,608 B** | −89.9% |
| 安装包 | 366,698,564 B | **312,146,590 B** | −14.7% |
| 运行时依赖 | CPython + Tcl/Tk + numpy + PyAV | 无 | — |

### 构建方式

- 界面：Win32 GDI 自绘（圆角控件、自绘下拉、双栏字幕、DPI 缩放）
- 识别：sherpa-onnx C API（SenseVoice + Whisper 回退）
- 翻译：CTranslate2 C++ API（本地）/ WinHTTP（免费在线、DeepSeek、有道）
- 音频：WASAPI 环回采集 + 能量 VAD + 降噪 + 远场增益

### 功能

三种翻译模式互不降级：

| 模式 | 联网 | 成本 | 语言 |
|---|---|---|---|
| 本地翻译（离线） | 否 | ¥0 | 英 → 中 |
| 在线翻译（免费） | 是 | ¥0 | 中/英/日/韩/德/法/意/西 |
| API 模式（付费） | 是 | 按量计费 | 全部 |

- 免费在线：MyMemory 国内直连零成本，额度用尽切有道免费，再兜底本地模型
- API 模式：DeepSeek 流式 / 有道智云，可选引擎（DeepSeek / 有道 / 自动兜底）
- 识别增强：自适应噪声门限、远场增益归一、句尾降噪、自定义热词
- 界面：3 套字幕配色、贴底/贴顶、字号热调、系统托盘
- 更新：启动静默检查，弹窗介绍更新内容，可跳过，覆盖安装保留配置
- 独家模块：四个隐藏入口，触发词以码点拼装
- 界面细节：品牌区 logo 与矢量齿轮、首次运行引导、高 DPI 分数缩放、侧栏自适应压缩

### 已验证

| 测试 | 结果 |
|---|---|
| JSON / 工具 / HTTP / 配置 | 79 项全过（含 SHA256/MD5 标准向量、真实 HTTPS） |
| 端到端 | 11 项全过（识别加载 1.375s、本地翻译、免费在线真实联网） |

### 安装

下载 `HearizonSync-CRV-Setup-2.5.6.exe` 直接运行。需要 Windows 10/11 64 位。

配置保存在 `%USERPROFILE%\\.hearizon_sync.json`，与 Python 版共用同一份配置。
"""


def token():
    p = subprocess.run(
        ["git", "credential", "fill"],
        input="protocol=https\nhost=github.com\n\n",
        capture_output=True, text=True, cwd=os.getcwd(),
    )
    for line in p.stdout.splitlines():
        if line.startswith("password="):
            return line.split("=", 1)[1]
    raise SystemExit("no token")


def api(method, path, body=None, tok=None):
    conn = http.client.HTTPSConnection("api.github.com", timeout=60)
    headers = {
        "Authorization": "token " + tok,
        "Accept": "application/vnd.github+json",
        "User-Agent": "hs-crv",
    }
    data = None
    if body is not None:
        data = json.dumps(body)
        headers["Content-Type"] = "application/json"
    conn.request(method, path, data, headers)
    r = conn.getresponse()
    txt = r.read().decode()
    conn.close()
    try:
        return r.status, json.loads(txt)
    except Exception:
        return r.status, {"raw": txt[:500]}


def main():
    tok = token()
    cwd = os.getcwd()
    print("cwd:", cwd)

    # 1. 创建 release
    status, j = api("POST", f"/repos/{REPO}/releases", {
        "tag_name": TAG,
        "name": "v2.5.6 — C++ 完整重写",
        "body": BODY,
        "draft": False,
        "prerelease": False,
    }, tok)
    print("create release:", status)
    if status == 422:
        # 已存在，查一下
        status, j = api("GET", f"/repos/{REPO}/releases/tags/{TAG}", None, tok)
        print("existing release:", status)
    if status not in (200, 201):
        print(json.dumps(j, ensure_ascii=False)[:600])
        return 1

    rel_id = j["id"]
    upload_url = j["upload_url"].split("{")[0]
    print("release id:", rel_id)
    print("release url:", j.get("html_url"))

    # 2. 删除同名旧资产
    status, assets = api("GET", f"/repos/{REPO}/releases/{rel_id}/assets", None, tok)
    if status == 200:
        for a in assets:
            if a["name"] == os.path.basename(ASSET):
                st, _ = api("DELETE", f"/repos/{REPO}/releases/assets/{a['id']}", None, tok)
                print("delete old asset:", a["id"], st)

    # 3. 上传
    size = os.path.getsize(ASSET)
    name = os.path.basename(ASSET)
    print(f"uploading {name} ({size} B)...")
    t0 = time.time()
    conn = http.client.HTTPSConnection("uploads.github.com", timeout=600)
    conn.putrequest("POST", f"/repos/{REPO}/releases/{rel_id}/assets?name={name}")
    conn.putheader("Authorization", "token " + tok)
    conn.putheader("Accept", "application/vnd.github+json")
    conn.putheader("User-Agent", "hs-crv")
    conn.putheader("Content-Type", "application/octet-stream")
    conn.putheader("Content-Length", str(size))
    conn.endheaders()

    sent = 0
    with open(ASSET, "rb") as f:
        while True:
            chunk = f.read(4 * 1024 * 1024)
            if not chunk:
                break
            conn.send(chunk)
            sent += len(chunk)
            pct = sent * 100 // size
            if pct % 10 == 0:
                print(f"  {pct}% ({sent//1024//1024} MB)")
    r = conn.getresponse()
    txt = r.read().decode()
    conn.close()
    print("upload status:", r.status, f"elapsed {time.time()-t0:.1f}s")
    try:
        u = json.loads(txt)
        print("asset id:", u.get("id"))
        print("size:", u.get("size"))
        print("download:", u.get("browser_download_url"))
    except Exception:
        print(txt[:400])
    return 0


if __name__ == "__main__":
    sys.exit(main())

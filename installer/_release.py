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
| 主程序 | 9,545,855 B | **983,040 B** | −89.7% |
| 安装包 | 366,698,564 B | **312,179,849 B** | −14.9% |
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

### 本次修复

| 问题 | 原因 | 处理 |
|---|---|---|
| 提示的是 Python 版的更新 | 更新接口一直指向 `eyru/HearizonSync` | 改到 `eyru/HearizonSync-CRV`，并只认名字含 `crv` 的资产 |
| 录音设备只列出 2 个 | 只枚举了「已启用」的设备 | 四种状态全部枚举，本机从 2 个变 8 个 |
| 默认选中的麦克风不收音 | 默认挑到虚拟声卡（CABLE Output） | 优先选「已启用且非虚拟声卡」，并记住上次选择的设备 |
| 自动纠错偶尔出现乱码 | 模型返回的 markdown/解释被直接显示；长句被 max_tokens=300 截断 | 净化输出、按输入长度给 max_tokens、非法 UTF-8 直接退回原文 |
| 关于页版本号看不出能点 | 只是一行同色系小字 | 改成有边框底色的胶囊按钮，悬停描边点亮并补下划线 |

### 麦克风修复（本轮）

| 问题 | 原因 | 处理 |
|---|---|---|
| 麦克风完全不收音 | 采集线程按 480 帧（30ms）切块喂 VAD，但 WASAPI 单次 `read()` 重采样后只有 160~320 帧，永远凑不满一块，VAD 一次都没被调用过 | 改为跨 `read` 累积缓冲，凑满 480 帧再切，并加缓冲上限兜底 |
| 一按开始就说话仍然不收音 | 噪声直方图把语音本身也统计进去，噪声底被自己说话抬到人声之上，`threshold` 锁死，之后全程判静音 | 噪声直方图只在不判定为语音的块上更新；`noiseFloor()` 加 0.045 上限 |
| 启动失败被静默吞掉 | `AudioCapture::start()` 立刻返回成功，`open`/`Start` 的失败只写进后台线程 | 等后台完成启动，把真实错误同步带回界面弹窗 |
| 说话时界面卡顿、长时间不出字 | `onPartial`/`onSentence` 在采集线程里同步跑 ASR（每 0.32s）和整句翻译（1~3s），WASAPI 缓冲被拖爆 | 常驻 worker 线程 + 任务队列；采集线程只入队，句末结果优先，未执行的临时结果丢弃 |
| 看不到任何输入反馈 | 电平条是空实现，`paint` 只画一条固定宽度轨道 | 采集线程原子写电平，侧栏按实时值填充，过载变亮绿 |
| 记住的麦克风拔掉后每次都失败 | 保存的设备 id 无条件沿用 | 仅在与「已启用」匹配时才沿用；选中不可用设备直接提示原因，不再假装进入监听 |

### 翻译修复（本轮）

| 问题 | 原因 | 处理 |
|---|---|---|
| 免费在线翻译永远提示「今日额度已用完或网络不通」 | 系统代理指向一个已经不再监听的端口（代理软件没跑，注册表残留），WinHTTP 的 `AUTOMATIC_PROXY` 把每个请求都送去死端口，全部连接失败 | 经代理失败时自动直连重试一次；直连成功就记住「代理不可用」5 分钟，期间直接走直连 |
| 每次开机第一句翻译要白等 2 秒 | 第一次请求先撞死代理、再走直连，两次连接超时叠加 | 启动时后台线程预热一次，把惩罚挪到用户看不见的地方（实测 2.2s → 0.33s） |
| 有道免费接口形同虚设 | 只在目标语言是中文时才调用，且硬编码 `to=zh-CHS` | 无条件调用并传入真实目标语言；限频从 6 次/分放宽到 15 次/分，间隔从 4s 降到 0.6s |
| 译文后面永远挂着「— 本地模型」 | 每句定稿时把 provider 拼进显示文本 | 彻底去掉来源尾巴（所有模式）。模式是用户自己选的，每句追一个出处只是噪音 |
| MyMemory 在无 VPN 时每句都白等 8 秒 | 连接失败后只冷却 30 秒 | 冷却拉到 300 秒，超时从 8s 降到 5s |

### 字幕修复（本轮）

| 问题 | 原因 | 处理 |
|---|---|---|
| 一句话会弹两次翻译结果 | 译文区三个消息把同一份内容画了两遍：`DstBegin` 开行、`DstChunk` 追加、`DstEnd` 又整句新起一行。非流式引擎的 `DstChunk` 传的就是完整译文，于是画两遍；流式引擎则是「预览一遍 + 定稿一遍」 | `DstBegin` 只记时间戳不落行，`DstChunk` 用 TextView 的 partial 临时行做实时预览，`DstEnd` 先 `clearPartial()` 再 `appendLine()` 落成唯一一行 |
| 清空字幕后下一句接上半截 | `clearSubtitles()` 只清控件和统计，没清流式累积状态 | 一并清掉累积缓冲和时间戳 |

### 已验证

| 测试 | 结果 |
|---|---|
| JSON / 工具 / HTTP / 配置 | 79 项全过（含 SHA256/MD5 标准向量、真实 HTTPS） |
| 端到端 | 11 项全过（识别加载 1.375s、本地翻译、免费在线真实联网） |
| VAD 合成音频回归 | 静音 0.6s + 语音 1.6s + 静音 1.0s → 断出 1 句 2.34s（`levels=106 sentences=1`），PASS |
| 字幕渲染回归 | 10 项全过（流式/非流式/多句/清空，译文与时间戳各恰好出现 1 次） |
| 界面端到端（真点击） | 9 项全过（主窗口、设置页 5 个下拉、彩蛋、关于页、关闭） |
| 主程序冒烟 / 优雅退出 | 启动 5~6 秒无崩溃、`WM_CLOSE` 后无残留进程、配置正常写回 |

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

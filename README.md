<div align="center">

<img src="resources/app.ico" width="96" alt="Hearizon Sync C.R.V.">

# Hearizon Sync C.R.V.

**CppRebuildVersion —— 用 C++ 完整重写的 Hearizon Sync**

[![Release](https://img.shields.io/badge/release-v2.5.6-1f8a70)]()
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0b6e5a)]()
[![C++](https://img.shields.io/badge/C%2B%2B-17-00599c)]()
[![License](https://img.shields.io/badge/license-GPL--3.0%20%2B%20Commercial-2f4858)](LICENSE.txt)

</div>

---

## 这是什么

Hearizon Sync 的 **C++ 重写版**。功能、界面、版本号与 Python 版完全一致，但底层换成原生实现。

原项目用 Python + Tkinter 编写（约 4700 行），打包后 9.5 MB 的 exe 需要拖着一个 653 MB 的运行时目录。这个版本用 C++17 + Win32 从头实现，**exe 仅 703 KB**，安装包从 366 MB 降到 312 MB。

| 指标 | Python 版 | C++ 版 |
|---|---|---|
| 主程序体积 | 9,545,855 B | **702,976 B** |
| 安装包 | 366,698,564 B | **311,890,218 B** |
| 运行时依赖 | CPython + Tcl/Tk + numpy + PyAV 等 | 无（纯原生） |
| 界面 | Tkinter 自绘 | Win32 GDI 自绘 |
| 识别 | sherpa-onnx Python 包 | sherpa-onnx C API |
| 翻译 | CTranslate2 Python 包 | CTranslate2 C++ API |

---

## 功能（与原版一致）

**三种翻译模式，互不降级**

| 模式 | 联网 | 成本 | 可用语言 |
|---|---|---|---|
| 本地翻译（离线） | 否 | ¥0 | 英 → 中 |
| 在线翻译（免费） | 是 | ¥0 | 中/英/日/韩/德/法/意/西 |
| API 模式（付费） | 是 | 按量计费 | 全部 |

- **免费在线**：MyMemory 公共接口，国内直连、不用 Key、零成本；额度用尽自动切有道免费接口，再兜底本地模型
- **API 模式**：DeepSeek 流式翻译 / 有道智云；可选引擎（DeepSeek / 有道 / 自动兜底）
- **识别**：SenseVoice（中英粤），日韩德法意西走 Whisper
- **识别增强**：自适应噪声门限、远场增益归一、句尾降噪、自定义热词
- **界面**：DPI 自适应、3 套字幕配色、贴底/贴顶、字号热调、系统托盘
- **更新**：启动时静默检查新版本，弹窗介绍更新内容，可跳过，覆盖安装保留配置
- **独家模块**：四个隐藏入口，触发词以码点拼装，源码中无明文

---

## 构建

### 依赖

- Visual Studio 2022 Build Tools（MSVC 14.44+）与 Windows SDK 10.0.26100+
- CMake 3.20+
- 第三方库（已随仓库提供头文件与导入库）：
  - sherpa-onnx C API（`third_party/sherpa/`）
  - CTranslate2 4.8.3（`third_party/ctranslate2/`）
  - sentencepiece 0.2.0 静态库（`third_party/sentencepiece/`）

### 编译

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cl /std:c++17 /O2 /MD /utf-8 /DNOMINMAX /DUNICODE /D_UNICODE ^
   /Fe:build\HearizonSync.exe ^
   src\main.cpp src\ui\*.cpp src\core\*.cpp src\audio\*.cpp ^
   src\asr\*.cpp src\mt\*.cpp src\platform\*.cpp ^
   /I src /I third_party\sherpa /I third_party\ctranslate2 ^
   /I third_party\sentencepiece\include ^
   third_party\sherpa\sherpa-onnx-c-api.lib ^
   third_party\ctranslate2\ctranslate2.lib ^
   third_party\sentencepiece\lib\sentencepiece.lib ^
   winhttp.lib ole32.lib oleaut32.lib uuid.lib shell32.lib ^
   comdlg32.lib gdi32.lib user32.lib advapi32.lib comctl32.lib ^
   dwmapi.lib shcore.lib version.lib
```

### 模型

从原项目 `models/` 目录复制：

```
models/sv/model.int8.onnx          229 MB  SenseVoice 识别
models/sv/tokens.txt
models/opus_en_zh/model.bin        153 MB  opus-mt 本地翻译
models/opus_en_zh/source.spm
models/opus_en_zh/target.spm
models/opus_en_zh/shared_vocabulary.json
models/opus_en_zh/vocab.json
models/opus_en_zh/config.json
```

> **本地翻译模型必须放在纯 ASCII 路径**。sentencepiece 的 C++ 层打不开中文路径，会报 `NOT_FOUND`。
> 查找顺序：`SLM_OPUS_EN_ZH_DIR` → `exe/models/` → `exe/../models/` → `D:\slm-models\`

### 打包安装程序

```bat
makensis installer\HearizonSyncCRV.nsi
```

---

## 重写过程中的技术要点

**CTranslate2 没有 C API。** 它只导出 MSVC 修饰的 C++ 符号（13204 个），无法直接用 MinGW 链接。解决方案是从 DLL 导出表生成 `.def`，再用 `lib /def` 生成导入库，最后用 MSVC 链接。

**sentencepiece 需要单独构建。** Python 包只提供 `.pyd` 扩展，没有独立 DLL。从源码编译静态库（自带 protobuf-lite / absl / darts_clone，14.6 MB），并复现了中文路径限制。

**sherpa-onnx 的 ONNX Runtime 版本必须匹配。** `sherpa_onnx/lib/` 里附带的 `onnxruntime.dll` 是 1.17.1（API 17），而 sherpa-onnx 1.13.8 需要 API 28。必须改用 `onnxruntime` 包里的 1.31.0，否则运行时报 `The requested API version [28] is not available`。

**WASAPI GUID 需要显式定义。** MSVC 不会自动链接 `CLSID_MMDeviceEnumerator`、`IID_IAudioClient` 等，需要 `DEFINE_GUID` 手动实例化，否则链接期报未解析符号。

**Marian 模型必须在源端补 `</s>`。** 少这个 token 输出会陷入重复循环，翻出「你能听到你能听到你能听到」。

---

## 已验证

| 测试 | 结果 |
|---|---|
| JSON 解析 | 16 项（含 UTF-8、转义、unicode） |
| 工具层 | 27 项（SHA256/MD5 标准测试向量、路径、文件） |
| HTTP | 10 项（**真实 HTTPS**：GitHub API、MyMemory、有道免费） |
| 配置 | 26 项（语言矩阵、版本比较、热词解析、存取往返） |
| **端到端** | 11 项（**识别加载 1.375s + 本地翻译 + 免费在线真实联网**） |

端到端实测输出：

```
[asr] SenseVoice 已就绪（1.375000s）
识别结果: 'Yeah.' lang=en
译文: 你能听到这两个音调的区别吗?
免费译文: 声级计测量声压级。  [免费(MyMemory)]
本地译文: 请坐下来仔细听好  [本地模型]
```

---

## 许可

**GPL-3.0 + 商业许可** 双许可，与原项目一致。

软件按「现状」提供，不附带任何担保。语音识别在本机完成，音频不出本机；只有选择联网翻译时，识别出的文本才会发送至相应服务商。

---

<div align="center">

原项目：[eyru/hearizon-sync](https://github.com/eyru/hearizon-sync)

</div>

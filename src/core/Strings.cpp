#include "Strings.h"

namespace hs {
namespace str {

const char* kAppTitle = "Hearizon Sync";
const char* kSidebarTitle = "Hearizon\nSync";
const char* kSidebarTagline = "实时字幕翻译";

const char* kInputLang = "输入语言（识别）";
const char* kAudioDevice = "音频输入";
const char* kTargetLang = "译成（目标）";
const char* kTranslateMode = "翻译模式";
const char* kApiEngine = "API 引擎（仅API模式）";
const char* kAsrEngine = "识别引擎";
const char* kGpuAccel = "GPU 加速";
const char* kAutoCorrect = "自动纠错-beta";
const char* kRefreshDevice = "刷新设备";
const char* kSubtitleStyle = "字幕样式";
const char* kPosition = "字幕位置";
const char* kPositionBottom = "底部";
const char* kPositionTop = "顶部";
const char* kTheme = "字幕配色";

const char* kStart = "开始同传";
const char* kStop = "停止";
const char* kTestMic = "测麦克风";
const char* kSaveRecord = "保存记录";
const char* kExportSrt = "导出 SRT";
const char* kClearSubtitle = "清空字幕";
const char* kMinimizeToTray = "最小化到托盘";
const char* kAbout = "关于";

const char* kSourceTitle = "原文 · 识别结果";
const char* kTargetTitle = "译文";

const char* kNotStarted = "未开始";
const char* kListening = "监听中";
const char* kRecognizing = "识别中...";
const char* kTranslating = "翻译中...";
const char* kStarting = "启动中...";
const char* kStopped = "已停止";

const char* kApiModeStarted = "API模式启动｜";
const char* kCorrectConsumeTitle = "自动纠错会消耗 API 余额";
const char* kCorrectConsumeBody =
    "开启后，每识别一句话都会额外调用一次云端纠错，会从你的 API 账户扣钱。\n\n"
    "只有「API 模式（付费）」才必须开它\n"
    "「本地翻译」「在线翻译（免费）」都不需要开\n"
    "没填 API Key 时只做离线规则纠错，不花钱";
const char* kDoNotRemind = "不再提醒";
const char* kConfirmOn = "确定开启";
const char* kCancel = "取消";

const char* kSettings = "设置";
const char* kTranslateModeDesc = "翻译模式（三种互相独立，随时可换）";
const char* kDeepseekKey = "DeepSeek API Key";
const char* kDeepseekHint = "只有「API 模式（付费）」会用到这个 Key。没填也不影响另外两种模式。";
const char* kMymemoryEmail = "MyMemory 邮箱（可选）";
const char* kMymemoryHint = "邮箱只用来向 MyMemory 申请更高的免费额度（5000 → 50000 字符/天），不填也能正常用。";
const char* kRecognitionBoost = "识别增强";
const char* kHotwords = "热词";
const char* kHotwordsHint = "填写你常说的专业词或人名，用逗号分隔。识别会偏向这些词，并把高度相似的同音错字纠正回来。留空则不做任何词表干预。";
const char* kHotwordsExample = "例：张三，项目名，专业术语A，专业术语B";
const char* kYoudaoTitle = "有道智云（API 模式的第二个引擎）";
const char* kYoudaoHint = "到 ai.youdao.com 创建「文本翻译」应用，把 appKey / appSecret 填在这里。\n只有「API 模式（付费）」会用到；本地翻译 / 在线翻译（免费）都不需要。";
const char* kAppKey = "有道 appKey";
const char* kAppSecret = "有道 appSecret";
const char* kDomain = "领域";
const char* kUpdateCheck = "启动时检查更新（有新版本会弹窗提示，可跳过）";
const char* kSaveSettings = "保存设置";
const char* kClose = "关闭";
const char* kVbCableTitle = "想翻译电脑声音（网课 / 视频）？";
const char* kVbCableHint =
    "当前只能录麦克风。要录系统声音，装一个免费的虚拟声卡即可：\n"
    "1) 下载 VB-Cable（vb-audio.com/Cable）并安装\n"
    "2) 重启后在左侧「音频输入」里选「CABLE Output」\n"
    "3) 把系统播放设备设为「CABLE Input」";
const char* kGpuTitle = "GPU 加速说明";
const char* kGpuHint =
    "Whisper 识别、本地 opus-mt 翻译：可用 GPU（需 NVIDIA 显卡）\n"
    "SenseVoice 识别：官方包为 CPU 编译，无法 GPU，但它本身就极快\n"
    "开启后下次识别会重新加载模型";

const char* kGpuProbing = "正在检测 CUDA 运行时...";
const char* kGpuOnOk = "GPU 已开启（Whisper 识别 + 本地翻译生效）";
const char* kGpuOff = "已关闭 GPU，全部走 CPU";
const char* kGpuFailPrefix = "GPU 无法启用：";
const char* kGpuWarnTitle = "GPU 无法启用";
const char* kGpuWarnBodyHasCard =
    "说明：\n"
    "• 本机检测到 NVIDIA 显卡，但缺少 CUDA 12 运行时\n"
    "• 需要安装 CUDA Toolkit 12.x（约 3 GB）才能启用\n"
    "• SenseVoice 识别为 CPU 模型，本身已足够快（RTF 0.02）\n"
    "• 不装也不影响使用";
const char* kGpuInfoTitle = "本机没有 NVIDIA 显卡";
const char* kGpuInfoBodyNoCard =
    "说明：\n"
    "• 这台电脑没有可用的 NVIDIA 显卡，GPU 加速用不了（硬件限制）\n"
    "• AMD / Intel 核显不支持这里用的 CUDA，不用再试\n"
    "• 所有功能照常运行，只是走 CPU：SenseVoice 识别 RTF 0.02、本地翻译约 0.03s/句\n"
    "• 不需要做任何设置，直接开始同传即可";

const char* kSrcPlaceholder = "点左侧「开始同传」开始监听";
const char* kDstPlaceholder = "译文会显示在这里";
const char* kNoSubtitle = "还没有字幕内容";
const char* kLangChanged = "语言已切换，重新开始同传后生效";
const char* kEngineChanged = "已切换引擎为 ";
const char* kEngineChangedTail = "，下次开始会重新加载";
const char* kTrayUnavailable = "托盘不可用：";
const char* kModelReady = "识别模型已就绪，请开始说话";

const char* kUpdateAvailable = "发现新版本";
const char* kUpdateLater = "稍后提醒";
const char* kUpdateDownload = "下载并安装";
const char* kUpdateSkip = "跳过这个版本（下次不再提示）";
const char* kUpdateDownloading = "正在下载更新";
const char* kUpdateDone = "更新已下载";
const char* kUpdateFailed = "下载失败";

const char* kAboutTagline = "麦克风实时字幕翻译 —— 说完一句，屏幕出原文和译文";
const char* kAuthorName = "何瑞程";
const char* kLicenseLine = "GPL-3.0 + 商业许可（双许可）";
const char* kAsrLine = "SenseVoice 本地推理 · 语音不上传";
const char* kMtLine = "本地 opus-mt / DeepSeek / 有道";
const char* kNotMedical = "本软件不是医疗器械，不得作为助听器或听力诊断设备使用。";
const char* kModuleThanksTitle = "Hearizon Sync 致谢名单";
const char* kModuleThanksNote = "以下参与内测人员名单不分先后次序";
const char* kModulePoemTitle = "山有木兮";
const char* kModuleDeveloperTitle = "关于作者";
const char* kModuleHorizon = "听见地平线之外的声音。\n—— Hear + Horizon + Sync";

const char* kTrayTip = "Hearizon Sync";
const char* kTrayShow = "显示主窗口";
const char* kTrayToggle = "开始/停止";
const char* kTrayQuit = "退出";

const char* kNoModel = "找不到模型文件";
const char* kMicError = "麦克风错误";
const char* kNoSpeech = "未识别到内容";
const char* kPunctOnly = "识别结果无实义（纯标点），已跳过";

const char* kThanksFooter = "谢谢你们听我讲完这个想法，也谢谢你们帮我把它做出来。";
const char* kPoemLine1 = "山有木兮木有枝";
const char* kPoemLine2 = "心悦君兮君不知";
const char* kPoemSource = "——《越人歌》";
const char* kAuthorSchool = "北京语言大学 · 海南国际学院";
const char* kAuthorMajor = "听力与言语康复学（本科生）";
const char* kAuthorDesc = "Hearizon Sync 作者";
const char* kPersonalSite = "个人网站";
const char* kLinkNote = "（点击才会打开浏览器，不会自动跳转）";

}  // namespace str
}  // namespace hs

// Hearizon Sync C.R.V. - 主窗口
#pragma once

#include "audio/AudioCapture.h"

#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "asr/Recognizer.h"
#include "audio/AudioCapture.h"
#include "core/Config.h"
#include "core/Gpu.h"
#include "mt/Translator.h"
#include "ui/Widgets.h"

namespace hs {

// 新版本提示窗口（模态）：返回 true = 下载并安装；outSkip = 勾选了跳过该版本
bool showUpdateDialogModal(HWND owner, const std::string& tag, const std::string& body,
                           bool* outSkip);

class TrayIcon;
class Updater;

// SRT 用的真实时间轴记录（每句的起止时间 + 原文/译文）
struct Segment {
    double start = 0.0;   // 相对会话开始的秒数
    double end = 0.0;
    std::string src;
    std::string dst;
};

// 界面消息
struct UiMessage {
    enum class Kind {
        Status,        // 状态栏
        PartialSrc,    // 流式临时原文
        Src,           // 正式原文
        DstBegin,      // 译文开始占位
        DstChunk,      // 译文增量
        DstEnd,        // 译文结束
        Stats,         // 统计
        GpuResult,     // GPU 探测结果
        UpdateAvailable,
        UpdateProgress,
        UpdateDone,
        UpdateFailed,
    };
    Kind kind = Kind::Status;
    std::string s1;
    std::string s2;
    std::string s3;
    long long n1 = 0;
    long long n2 = 0;
    bool b1 = false;
};

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool create(HINSTANCE hInst);
    int run();

    // 供托盘/更新模块回调
    HWND hwnd() const { return hwnd_; }
    void postMessage(UiMessage msg);

private:
    // 窗口过程
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handleMessage(UINT msg, WPARAM wp, LPARAM lp);

    // 初始化与布局
    void initWidgets();
    void layout();
    void paint(HDC dc);
    void paintSidebar(HDC dc);
    void paintPanels(HDC dc);
    void paintStatusBar(HDC dc);
    void updateScrollbars();

    // 鼠标/键盘
    void onMouseMove(int x, int y);
    void onMouseDown(int x, int y);
    void onMouseUp(int x, int y);
    void onMouseWheel(int delta, int x, int y);
    void onKeyDown(WPARAM key);
    void applyFontStep(int delta);
    void onTimer(UINT_PTR id);

    // 业务
    void startCapture();
    void stopCapture();
    void onSentence(const std::vector<float>& audio);
    void onPartial(const std::vector<float>& audio);
    void onLevel(double rms);
    void pumpUiMessages();
    void applyMessage(const UiMessage& msg);

    // 识别/翻译工作线程：采集线程只负责把音频丢进队列，绝不在这里跑 ASR，
    // 否则一句话卡 1~3 秒会让 WASAPI 缓冲溢出、听感上就是"不收音"。
    void enqueueAudio(bool isFinal, const std::vector<float>& audio);
    void startWorker();
    void stopWorker();
    void workerLoop();
    void processPartial(const std::vector<float>& audio);
    void processSentence(const std::vector<float>& audio);
    // 把控件状态镜像到后台线程可读的字段（由 UI 线程定期调用）
    void syncRuntimeCfg();

    // 侧栏动作
    void onModeChanged(int index);
    void onApiEngineChanged(int index);
    void onInputLangChanged(int index);
    void onTargetLangChanged(int index);
    void onEngineChanged(int index);
    void onGpuToggled(bool on);
    void onCorrectToggled(bool on);
    void onThemeChanged(int index);
    void onPositionChanged(bool top);
    void updatePosButtons();
    void refreshDevices();
    void applyModeLanguages();

    // 弹窗
    void openSettings();
    void openAbout();
    void testMic();
    void saveRecord();
    void exportSrt();
    void clearSubtitles();
    void minimizeToTray();

    // 状态
    void setStatus(const std::string& text, COLORREF color);
    void setStatusThreadSafe(const std::string& text);

    // 更新
    void checkUpdateAsync();
    void showUpdateDialog(const std::string& tag, const std::string& body,
                          const std::string& url, const std::string& page);
    void startDownload(const std::string& url, const std::string& tag);

    // 独家模块

    HINSTANCE hInst_ = nullptr;
    std::vector<AudioDevice> devices_;  // 当前枚举到的输入设备
    HWND hwnd_ = nullptr;

    // 侧栏控件
    ComboBox inLangBox_;
    ComboBox deviceBox_;
    ComboBox targetLangBox_;
    ComboBox modeBox_;
    ComboBox apiEngineBox_;
    ComboBox engineBox_;
    ComboBox themeBox_;
    CheckBox gpuCheck_;
    CheckBox correctCheck_;
    Button btnStart_;
    Button btnTestMic_;
    Button btnSave_;
    Button btnExport_;
    Button btnClear_;
    Button btnTray_;
    Button btnAbout_;
    Button btnRefresh_;
    Button btnSettings_;
    Button btnGear_;      // 品牌区右上角的设置齿轮
    Button btnTheme_;
    Button btnPosBottom_;
    Button btnPosTop_;

    // 字幕区
    TextView srcView_;
    TextView dstView_;
    ScrollBar srcScroll_;
    ScrollBar dstScroll_;

    // 状态栏
    std::string statusText_ = "未开始";
    COLORREF statusColor_ = RGB(0x6E, 0x88, 0x80);

    // 布局缓存
    RECT rcSidebar_{};
    RECT rcSrcPanel_{};
    RECT rcDstPanel_{};
    RECT rcStatus_{};
    RECT rcVersion_{};   // 状态栏版本号命中区（连点彩蛋）
    int labelH_ = 0;     // 侧栏字段标签高度（随窗口自适应）
    int labelGap_ = 0;   // 标签与控件的间隙
    int dividerY_ = 0;   // 字幕分区上方的分隔线 y（布局时写入）
    int sideTop_ = 0;     // 侧栏可滚动区域上沿
    int sideBottom_ = 0;  // 侧栏可滚动区域下沿
    int sideScroll_ = 0;  // 侧栏当前滚动量（像素）
    int sideMax_ = 0;     // 侧栏最大滚动量，0 表示无需滚动

    // 业务对象
    std::unique_ptr<AudioCapture> capture_;
    std::unique_ptr<Recognizer> recognizer_;
    std::unique_ptr<Translator> translator_;
    std::unique_ptr<TrayIcon> tray_;
    std::unique_ptr<Updater> updater_;

    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};

    // 后台识别任务：句末结果（final）优先，未执行的 partial 会被丢弃
    struct AudioJob {
        bool final_ = false;
        std::vector<float> audio;
    };
    std::thread worker_;
    std::mutex workerLock_;
    std::condition_variable workerCv_;
    std::deque<AudioJob> jobs_;
    bool workerStop_ = false;

    // 后台线程不能碰控件：由 UI 线程定期同步的配置镜像
    std::mutex cfgMutex_;
    std::string cfgTargetLang_;
    bool cfgCorrect_ = false;

    // 实时电平（0..1000），采集线程写、paint 读
    std::atomic<int> levelPermille_{0};

    // UI 消息队列
    std::mutex queueLock_;
    std::queue<UiMessage> queue_;

    // 统计
    long long sentences_ = 0;
    long long charsIn_ = 0;
    long long charsOut_ = 0;
    DWORD startTick_ = 0;

    // 字幕样式
    int subSizeDst_ = 18;
    int subSizeSrc_ = 14;
    bool subTop_ = false;
    int themeIndex_ = 0;

    // 关于页彩蛋计数
    std::string horizonText_;

    // 拖拽与弹层
    bool dragging_ = false;
    POINT dragStart_{};
    RECT dragWindowRect_{};
    ComboBox* openPopup_ = nullptr;

    // 定时器
    static constexpr UINT_PTR kTimerPump = 1;
    static constexpr UINT_PTR kTimerUpdate = 2;
    static constexpr UINT_PTR kTimerWelcome = 3;
    // 静音提醒走 kTimerPump，这里只是保留一个不冲突的 id（原来与 kTimerWelcome 同值）
    static constexpr UINT_PTR kTimerSilence = 4;

    DWORD lastSpeechTick_ = 0;
    bool silenceWarned_ = false;

    // 真实时间轴（导出 SRT 用）
    std::vector<Segment> segments_;
    double sessionStart_ = 0.0;

    // GPU 探测（后台线程，结果经 WM_APP+2 回到 UI 线程）
    std::thread gpuThread_;
    std::mutex gpuMtx_;
    gpu::ProbeResult gpuResult_;
    bool gpuProbePending_ = false;
    void onGpuProbeDone();
};

// 全局窗口指针（窗口过程需要）
extern MainWindow* g_mainWindow;

}  // namespace hs

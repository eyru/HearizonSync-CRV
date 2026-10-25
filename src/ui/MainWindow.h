// Hearizon Sync C.R.V. - 主窗口
#pragma once

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
#include "mt/Translator.h"
#include "ui/Widgets.h"

namespace hs {

class TrayIcon;
class Updater;

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
    void onTimer(UINT_PTR id);

    // 业务
    void startCapture();
    void stopCapture();
    void onSentence(const std::vector<float>& audio);
    void onPartial(const std::vector<float>& audio);
    void onLevel(double rms);
    void pumpUiMessages();
    void applyMessage(const UiMessage& msg);

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
    void onVersionClicked();
    void checkEggInput(const std::string& text, int ctrlId);
    void showModuleThanks();
    void showModulePoem();
    void showModuleDeveloper();

    HINSTANCE hInst_ = nullptr;
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

    // 业务对象
    std::unique_ptr<AudioCapture> capture_;
    std::unique_ptr<Recognizer> recognizer_;
    std::unique_ptr<Translator> translator_;
    std::unique_ptr<TrayIcon> tray_;
    std::unique_ptr<Updater> updater_;

    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
    std::thread worker_;
    std::mutex workerLock_;

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
    int versionClicks_ = 0;
    DWORD lastVersionClick_ = 0;
    std::string horizonText_;

    // 拖拽与弹层
    bool dragging_ = false;
    POINT dragStart_{};
    RECT dragWindowRect_{};
    ComboBox* openPopup_ = nullptr;

    // 定时器
    static constexpr UINT_PTR kTimerPump = 1;
    static constexpr UINT_PTR kTimerUpdate = 2;
    static constexpr UINT_PTR kTimerSilence = 3;
    static constexpr UINT_PTR kTimerVersionReset = 4;

    DWORD lastSpeechTick_ = 0;
    bool silenceWarned_ = false;
};

// 全局窗口指针（窗口过程需要）
extern MainWindow* g_mainWindow;

}  // namespace hs

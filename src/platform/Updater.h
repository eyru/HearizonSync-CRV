// Hearizon Sync C.R.V. - 在线更新
#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace hs {

struct UpdateInfo {
    std::string tag;
    std::string body;
    std::string downloadUrl;
    std::string pageUrl;
};

class Updater {
public:
    using AvailableCallback = std::function<void(const UpdateInfo&)>;
    using ProgressCallback = std::function<void(long long got, long long total)>;
    using DoneCallback = std::function<void(const std::string& path)>;
    using FailedCallback = std::function<void(const std::string& error)>;

    Updater();
    ~Updater();

    void setCallbacks(AvailableCallback onAvail, ProgressCallback onProgress,
                      DoneCallback onDone, FailedCallback onFailed);

    // 异步检查；发现新版本时回调
    void checkAsync();

    // 异步下载
    void downloadAsync(const std::string& url, const std::string& tag);

    void cancel();

    static std::string apiUrl();
    static std::string pageUrl();

private:
    void checkWorker();
    void downloadWorker(std::string url, std::string tag);

    std::thread thread_;
    std::atomic<bool> cancel_{false};

    AvailableCallback onAvail_;
    ProgressCallback onProgress_;
    DoneCallback onDone_;
    FailedCallback onFailed_;
};

}  // namespace hs

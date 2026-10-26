#include "Updater.h"

#include <windows.h>

#include "core/Config.h"
#include "core/Http.h"
#include "core/Json.h"
#include "core/Util.h"

namespace hs {

Updater::Updater() = default;

Updater::~Updater() {
    cancel();
    if (thread_.joinable()) thread_.join();
}

void Updater::setCallbacks(AvailableCallback onAvail, ProgressCallback onProgress,
                           DoneCallback onDone, FailedCallback onFailed) {
    onAvail_ = std::move(onAvail);
    onProgress_ = std::move(onProgress);
    onDone_ = std::move(onDone);
    onFailed_ = std::move(onFailed);
}

std::string Updater::apiUrl() {
    // 必须指向 C.R.V. 自己的仓库。写错成 Python 版仓库会让用户被引导去装 Python 版。
    return "https://api.github.com/repos/eyru/HearizonSync-CRV/releases/latest";
}

std::string Updater::pageUrl() {
    return "https://github.com/eyru/HearizonSync-CRV/releases/latest";
}

void Updater::cancel() { cancel_.store(true); }

void Updater::checkAsync() {
    if (thread_.joinable()) thread_.join();
    cancel_.store(false);
    thread_ = std::thread([this]() { checkWorker(); });
}

void Updater::checkWorker() {
    if (cancel_.load()) return;

    HttpResult r = httpGet(apiUrl(),
                           {{"User-Agent", std::string("HearizonSync/") + appVersion()},
                            {"Accept", "application/vnd.github+json"}},
                           8);
    if (cancel_.load() || r.failed() || r.status != 200) return;

    Json j = Json::parse(r.body);
    std::string tag = j.getString("tag_name");
    if (tag.empty()) return;
    if (compareVersion(tag, appVersion()) <= 0) return;
    if (config().updateSkipTag == tag) return;

    UpdateInfo info;
    info.tag = tag;
    info.body = j.getString("body");
    info.pageUrl = j.getString("html_url", pageUrl());

    // 只认 C.R.V. 自己的安装包。即使仓库地址将来又被写错，也不会把别的版本装进来。
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

    if (cancel_.load()) return;
    if (onAvail_) onAvail_(info);
}

void Updater::downloadAsync(const std::string& url, const std::string& tag) {
    if (thread_.joinable()) thread_.join();
    cancel_.store(false);
    thread_ = std::thread([this, url, tag]() { downloadWorker(url, tag); });
}

namespace {
struct ProgressCtx {
    Updater::ProgressCallback cb;
};

bool progressTrampoline(long long got, long long total, void* user) {
    auto* ctx = static_cast<ProgressCtx*>(user);
    if (ctx && ctx->cb) ctx->cb(got, total);
    return true;
}
}  // namespace

void Updater::downloadWorker(std::string url, std::string tag) {
    std::string safeTag = tag;
    if (!safeTag.empty() && (safeTag[0] == 'v' || safeTag[0] == 'V')) safeTag.erase(0, 1);
    std::string path = joinPath(tempDir(), "HearizonSync-CRV-Setup-" + safeTag + ".exe");

    ProgressCtx ctx{onProgress_};
    long long total = 0;
    std::string err;
    bool ok = httpDownload(url, path, &total, &progressTrampoline, &ctx, &err);
    if (cancel_.load()) return;
    if (!ok) {
        if (onFailed_) onFailed_(err.empty() ? "下载失败" : err);
        return;
    }
    if (onDone_) onDone_(path);
}

}  // namespace hs

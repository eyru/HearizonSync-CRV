// Hearizon Sync C.R.V. - HTTP 客户端（WinHTTP，无第三方依赖）
#pragma once

#include <map>
#include <string>
#include <vector>

namespace hs {

struct HttpResult {
    int status = 0;          // 0 表示连接失败
    std::string body;
    std::string error;
    bool ok() const { return status >= 200 && status < 300; }
    bool failed() const { return status == 0; }
};

struct HttpRequest {
    std::string url;
    std::string method = "GET";
    std::map<std::string, std::string> headers;
    std::string body;
    int timeoutSec = 30;
};

HttpResult httpRequest(const HttpRequest& req);

// 便捷方法
HttpResult httpGet(const std::string& url,
                   const std::map<std::string, std::string>& headers = {},
                   int timeoutSec = 30);

HttpResult httpPostJson(const std::string& url, const std::string& json,
                        const std::map<std::string, std::string>& headers = {},
                        int timeoutSec = 60);

// 流式下载：onChunk 返回 false 可中断。返回是否成功。
bool httpDownload(const std::string& url, const std::string& savePath,
                  long long* totalBytes,
                  bool (*onProgress)(long long got, long long total, void* user),
                  void* user, std::string* errorOut);

// 流式 POST（SSE）：每收到一行 data: 回调一次
bool httpPostStream(const std::string& url, const std::string& json,
                    const std::map<std::string, std::string>& headers,
                    bool (*onLine)(const std::string& line, void* user), void* user,
                    int timeoutSec, std::string* errorOut);

}  // namespace hs

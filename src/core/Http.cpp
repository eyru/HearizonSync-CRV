#include "Http.h"

#include <windows.h>
#include <winhttp.h>

#include <cstdio>

#include "Util.h"

namespace hs {

namespace {

struct UrlParts {
    bool https = true;
    std::wstring host;
    INTERNET_PORT port = 0;
    std::wstring path;
};

bool crackUrl(const std::string& url, UrlParts& out) {
    std::wstring w = toWide(url);
    if (w.empty()) return false;

    const wchar_t* p = w.c_str();
    if (wcsncmp(p, L"https://", 8) == 0) {
        out.https = true;
        p += 8;
        out.port = INTERNET_DEFAULT_HTTPS_PORT;
    } else if (wcsncmp(p, L"http://", 7) == 0) {
        out.https = false;
        p += 7;
        out.port = INTERNET_DEFAULT_HTTP_PORT;
    } else {
        return false;
    }

    const wchar_t* slash = wcschr(p, L'/');
    std::wstring hostPort = slash ? std::wstring(p, slash) : std::wstring(p);
    out.path = slash ? std::wstring(slash) : std::wstring(L"/");

    size_t colon = hostPort.find(L':');
    if (colon != std::wstring::npos) {
        out.host = hostPort.substr(0, colon);
        int port = _wtoi(hostPort.c_str() + colon + 1);
        if (port > 0) out.port = static_cast<INTERNET_PORT>(port);
    } else {
        out.host = hostPort;
    }
    return !out.host.empty();
}

int statusCode(HINTERNET req) {
    DWORD code = 0;
    DWORD size = sizeof(code);
    if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX, &code, &size,
                             WINHTTP_NO_HEADER_INDEX)) {
        return 0;
    }
    return static_cast<int>(code);
}

}  // namespace

HttpResult httpRequest(const HttpRequest& reqIn) {
    HttpResult out;
    UrlParts u;
    if (!crackUrl(reqIn.url, u)) {
        out.error = "bad url";
        return out;
    }

    // session/connect 生命周期需要覆盖整个请求，这里展开写
    HINTERNET session = WinHttpOpen(L"HearizonSync/2.5.6",
                                    WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        out.error = "WinHttpOpen failed";
        return out;
    }
    int ms = reqIn.timeoutSec * 1000;
    WinHttpSetTimeouts(session, ms, ms, ms, ms);

    HINTERNET connect = WinHttpConnect(session, u.host.c_str(), u.port, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        out.error = "connect failed";
        return out;
    }

    DWORD flags = u.https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(connect, toWide(reqIn.method).c_str(),
                                       u.path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        out.error = "open request failed";
        return out;
    }

    for (const auto& kv : reqIn.headers) {
        std::wstring line = toWide(kv.first + ": " + kv.second);
        WinHttpAddRequestHeaders(req, line.c_str(), static_cast<DWORD>(-1),
                                 WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
    }

    BOOL sent = WinHttpSendRequest(
        req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        reqIn.body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)reqIn.body.data(),
        static_cast<DWORD>(reqIn.body.size()), static_cast<DWORD>(reqIn.body.size()), 0);

    if (!sent || !WinHttpReceiveResponse(req, nullptr)) {
        out.error = "send/receive failed: " + std::to_string(GetLastError());
        WinHttpCloseHandle(req);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return out;
    }

    out.status = statusCode(req);

    DWORD avail = 0;
    do {
        avail = 0;
        if (!WinHttpQueryDataAvailable(req, &avail)) break;
        if (avail == 0) break;
        std::string chunk(avail, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(req, &chunk[0], avail, &read)) break;
        chunk.resize(read);
        out.body += chunk;
    } while (avail > 0);

    WinHttpCloseHandle(req);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return out;
}

HttpResult httpGet(const std::string& url,
                   const std::map<std::string, std::string>& headers, int timeoutSec) {
    HttpRequest r;
    r.url = url;
    r.method = "GET";
    r.headers = headers;
    r.timeoutSec = timeoutSec;
    return httpRequest(r);
}

HttpResult httpPostJson(const std::string& url, const std::string& json,
                        const std::map<std::string, std::string>& headers,
                        int timeoutSec) {
    HttpRequest r;
    r.url = url;
    r.method = "POST";
    r.body = json;
    r.headers = headers;
    r.headers["Content-Type"] = "application/json";
    r.timeoutSec = timeoutSec;
    return httpRequest(r);
}

bool httpDownload(const std::string& url, const std::string& savePath,
                  long long* totalBytes,
                  bool (*onProgress)(long long, long long, void*), void* user,
                  std::string* errorOut) {
    UrlParts u;
    if (!crackUrl(url, u)) {
        if (errorOut) *errorOut = "bad url";
        return false;
    }

    HINTERNET session = WinHttpOpen(L"HearizonSync/2.5.6",
                                    WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        if (errorOut) *errorOut = "WinHttpOpen failed";
        return false;
    }
    int ms = 60 * 1000;
    WinHttpSetTimeouts(session, ms, ms, ms, ms);

    HINTERNET connect = WinHttpConnect(session, u.host.c_str(), u.port, 0);
    DWORD flags = u.https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = connect ? WinHttpOpenRequest(connect, L"GET", u.path.c_str(),
                                                 nullptr, WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES, flags)
                            : nullptr;
    bool ok = false;
    FILE* f = nullptr;
    do {
        if (!req) break;
        // 跟随重定向
        DWORD redir = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
        WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &redir, sizeof(redir));

        if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
            break;
        if (!WinHttpReceiveResponse(req, nullptr)) break;

        int code = statusCode(req);
        if (code < 200 || code >= 300) {
            if (errorOut) *errorOut = "http " + std::to_string(code);
            break;
        }

        // 内容长度
        wchar_t lenBuf[64];
        DWORD lenSize = sizeof(lenBuf);
        long long total = 0;
        if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH,
                                WINHTTP_HEADER_NAME_BY_INDEX, lenBuf, &lenSize,
                                WINHTTP_NO_HEADER_INDEX)) {
            total = _wtoi64(lenBuf);
        }
        if (totalBytes) *totalBytes = total;

        f = _wfopen(toWide(savePath).c_str(), L"wb");
        if (!f) {
            if (errorOut) *errorOut = "cannot write file";
            break;
        }

        long long got = 0;
        DWORD avail = 0;
        bool aborted = false;
        do {
            avail = 0;
            if (!WinHttpQueryDataAvailable(req, &avail)) break;
            if (avail == 0) break;
            std::string chunk(avail, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(req, &chunk[0], avail, &read)) break;
            if (read == 0) break;
            if (fwrite(chunk.data(), 1, read, f) != read) break;
            got += read;
            if (onProgress && !onProgress(got, total, user)) {
                aborted = true;
                break;
            }
        } while (avail > 0);

        ok = !aborted && got > 0;
        if (aborted && errorOut) *errorOut = "aborted";
    } while (false);

    if (f) fclose(f);
    if (req) WinHttpCloseHandle(req);
    if (connect) WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok;
}

bool httpPostStream(const std::string& url, const std::string& json,
                    const std::map<std::string, std::string>& headers,
                    bool (*onLine)(const std::string&, void*), void* user,
                    int timeoutSec, std::string* errorOut) {
    UrlParts u;
    if (!crackUrl(url, u)) {
        if (errorOut) *errorOut = "bad url";
        return false;
    }

    HINTERNET session = WinHttpOpen(L"HearizonSync/2.5.6",
                                    WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        if (errorOut) *errorOut = "WinHttpOpen failed";
        return false;
    }
    int ms = timeoutSec * 1000;
    WinHttpSetTimeouts(session, ms, ms, ms, ms);

    HINTERNET connect = WinHttpConnect(session, u.host.c_str(), u.port, 0);
    DWORD flags = u.https ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = connect ? WinHttpOpenRequest(connect, L"POST", u.path.c_str(),
                                                 nullptr, WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES, flags)
                            : nullptr;

    bool ok = false;
    do {
        if (!req) break;
        for (const auto& kv : headers) {
            std::wstring line = toWide(kv.first + ": " + kv.second);
            WinHttpAddRequestHeaders(req, line.c_str(), static_cast<DWORD>(-1),
                                     WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
        }
        if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                (LPVOID)json.data(), static_cast<DWORD>(json.size()),
                                static_cast<DWORD>(json.size()), 0))
            break;
        if (!WinHttpReceiveResponse(req, nullptr)) break;

        int code = statusCode(req);
        if (code < 200 || code >= 300) {
            if (errorOut) *errorOut = "http " + std::to_string(code);
            break;
        }

        std::string pending;
        bool stopped = false;
        DWORD avail = 0;
        do {
            avail = 0;
            if (!WinHttpQueryDataAvailable(req, &avail)) break;
            if (avail == 0) break;
            std::string chunk(avail, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(req, &chunk[0], avail, &read)) break;
            if (read == 0) break;
            pending.append(chunk.data(), read);

            size_t pos;
            while ((pos = pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, pos);
                pending.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (onLine && !onLine(line, user)) {
                    ok = true;
                    stopped = true;
                    break;
                }
            }
            if (stopped) break;
        } while (avail > 0);
        ok = true;
    } while (false);

    if (req) WinHttpCloseHandle(req);
    if (connect) WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return ok;
}

}  // namespace hs

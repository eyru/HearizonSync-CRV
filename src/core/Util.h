// Hearizon Sync C.R.V. - 通用工具：UTF-8 与 UTF-16 转换、路径、时间、字符串
#pragma once

#include <string>
#include <vector>

namespace hs {

// UTF-8 <-> UTF-16（Windows 原生宽字符）
std::wstring toWide(const std::string& utf8);
std::string toUtf8(const std::wstring& wide);

// 字符串工具
bool startsWith(const std::string& s, const std::string& prefix);
bool endsWith(const std::string& s, const std::string& suffix);
std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, const std::string& sep);
std::string join(const std::vector<std::string>& parts, const std::string& sep);
std::string replaceAll(std::string s, const std::string& from, const std::string& to);
std::string toLower(std::string s);

// URL 编码（RFC 3986）
std::string urlEncode(const std::string& s);

// 路径
std::string exeDir();
std::string userHome();
std::string joinPath(const std::string& a, const std::string& b);
bool fileExists(const std::string& path);
bool dirExists(const std::string& path);
std::string tempDir();
bool writeTextFile(const std::string& path, const std::string& content);
std::string readTextFile(const std::string& path);
std::string appVersion();

// 时间
std::string nowTimeString();          // HH:MM:SS
std::string nowDateString();          // YYYY-MM-DD
std::string fileTimestamp();          // YYYYmmdd_HHMMSS（文件名用）

// 保存/导出目录：优先「文档\Hearizon Sync」，逐个探测可写位置，
// 保证装在 Program Files 时也能落地（对应 Python 版 _output_dir()）。
std::string outputDir();

// 秒 -> SRT 时间戳 HH:MM:SS,mmm
std::string fmtSrtTime(double seconds);
double nowSeconds();                  // 单调时钟秒

// SHA256（有道 v3 签名需要）
std::string sha256Hex(const std::string& input);

// MD5（有道 v1 签名备用）
std::string md5Hex(const std::string& input);

}  // namespace hs

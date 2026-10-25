#include "../src/core/Util.h"
#include <cstdio>
#include <string>

static int fails = 0, oks = 0;

static void check(const char* name, bool cond, const std::string& extra = "") {
    if (cond) { ++oks; std::printf("[PASS] %s\n", name); }
    else { ++fails; std::printf("[FAIL] %s %s\n", name, extra.c_str()); }
}

int main() {
    // SHA256 标准测试向量
    check("sha256 empty",
          hs::sha256Hex("") ==
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          hs::sha256Hex(""));
    check("sha256 abc",
          hs::sha256Hex("abc") ==
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          hs::sha256Hex("abc"));
    check("sha256 long",
          hs::sha256Hex(std::string(1000, 'a')) ==
              "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3",
          hs::sha256Hex(std::string(1000, 'a')));

    // MD5 标准测试向量
    check("md5 empty", hs::md5Hex("") == "d41d8cd98f00b204e9800998ecf8427e",
          hs::md5Hex(""));
    check("md5 abc", hs::md5Hex("abc") == "900150983cd24fb0d6963f7d28e17f72",
          hs::md5Hex("abc"));

    // UTF-8 / UTF-16
    std::string zh = "中文测试";
    check("utf8->wide->utf8", hs::toUtf8(hs::toWide(zh)) == zh);

    // 字符串工具
    check("trim", hs::trim("  a b  ") == "a b", hs::trim("  a b  "));
    check("startsWith", hs::startsWith("hello", "he"));
    check("endsWith", hs::endsWith("hello", "lo"));
    check("replaceAll", hs::replaceAll("a-b-c", "-", "+") == "a+b+c");
    check("toLower", hs::toLower("AbC") == "abc");

    auto parts = hs::split("a,bb,ccc", ",");
    check("split size", parts.size() == 3);
    check("split join", hs::join(parts, "|") == "a|bb|ccc");

    // URL 编码（中文）
    std::string enc = hs::urlEncode("你好 a");
    check("urlEncode", enc == "%E4%BD%A0%E5%A5%BD%20a", enc);

    // 路径
    check("joinPath", hs::joinPath("C:\\a", "b") == "C:\\a\\b");
    check("joinPath trailing", hs::joinPath("C:\\a\\", "b") == "C:\\a\\b");
    check("exeDir not empty", !hs::exeDir().empty());
    check("userHome not empty", !hs::userHome().empty());
    check("tempDir not empty", !hs::tempDir().empty());

    // 版本与时间
    check("version", hs::appVersion() == "2.5.6", hs::appVersion());
    std::string ts = hs::nowTimeString();
    check("time format", ts.size() == 8 && ts[2] == ':' && ts[5] == ':', ts);
    std::string ds = hs::nowDateString();
    check("date format", ds.size() == 10 && ds[4] == '-' && ds[7] == '-', ds);

    // 文件读写
    std::string tmp = hs::joinPath(hs::tempDir(), "hs_test_util.txt");
    check("write file", hs::writeTextFile(tmp, "hello 中文"));
    check("read file", hs::readTextFile(tmp) == "hello 中文");
    check("fileExists", hs::fileExists(tmp));
    check("dirExists temp", hs::dirExists(hs::tempDir()));
    check("dirExists bogus", !hs::dirExists("Z:\\nope_xyz"));
    std::remove(tmp.c_str());

    std::printf("\n==== %d passed, %d failed ====\n", oks, fails);
    return fails ? 1 : 0;
}

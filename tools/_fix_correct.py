# -*- coding: utf-8 -*-
"""自动纠错出现的「乱码」有三类来源，逐个堵掉：
1) 模型返回被 markdown 或解释文字包住，直接显示就成了乱码样；
2) max_tokens 写死 300，长句被截断成半截；
3) 返回含替换字符或非法 UTF-8 字节时没有兜底。
原则：宁可不错，也不要把坏文本塞给用户。
"""
import io

ROOT = r"D:\Audivox事务\HearizonSync C.R.V"

# ---------------- Util：加 UTF-8 合法性校验 ----------------
UH = ROOT + r"\src\core\Util.h"
uh = io.open(UH, encoding="utf-8").read()
if "bool isValidUtf8" not in uh:
    uh = uh.replace("std::string toUtf8(const std::wstring& w);",
                    "std::string toUtf8(const std::wstring& w);\n"
                    "bool isValidUtf8(const std::string& s);", 1)
    io.open(UH, "w", encoding="utf-8", newline="").write(uh)
    print("Util.h ok")

UC = ROOT + r"\src\core\Util.cpp"
uc = io.open(UC, encoding="utf-8").read()
if "bool isValidUtf8" not in uc:
    body = '''
bool isValidUtf8(const std::string& s) {
    size_t i = 0;
    const size_t n = s.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        int need = 0;
        if (c < 0x80) {
            ++i;
            continue;
        } else if ((c & 0xE0) == 0xC0) {
            need = 1;
            if (c < 0xC2) return false;   // 过长编码
        } else if ((c & 0xF0) == 0xE0) {
            need = 2;
        } else if ((c & 0xF8) == 0xF0) {
            need = 3;
            if (c > 0xF4) return false;   // 超出 Unicode 范围
        } else {
            return false;
        }
        if (i + static_cast<size_t>(need) >= n) return false;
        for (int k = 1; k <= need; ++k) {
            unsigned char cc = static_cast<unsigned char>(s[i + static_cast<size_t>(k)]);
            if ((cc & 0xC0) != 0x80) return false;
        }
        i += static_cast<size_t>(need) + 1;
    }
    return true;
}
'''
    anchor = "std::string toUtf8(const std::wstring& w) {"
    idx = uc.find(anchor)
    assert idx > 0, "找不到 toUtf8 实现位置"
    end = uc.find("\n}\n", idx)
    assert end > 0
    uc = uc[:end + 3] + body + uc[end + 3:]
    io.open(UC, "w", encoding="utf-8", newline="").write(uc)
    print("Util.cpp ok")

# ---------------- Translator：纠错结果做净化 ----------------
P = ROOT + r"\src\mt\Translator.cpp"
s = io.open(P, encoding="utf-8").read()

# 1) max_tokens 按输入长度给，别写死 300
s = s.replace('    body.set("max_tokens", 300);\n\n    HttpResult r = httpPostJson(kDeepseekApi',
              '    // 写死 300 会让长句被截成半截，看着就像乱码\n'
              '    int cap = static_cast<int>(text.size()) * 3 + 160;\n'
              '    if (cap < 300) cap = 300;\n'
              '    if (cap > 2000) cap = 2000;\n'
              '    body.set("max_tokens", cap);\n\n'
              '    HttpResult r = httpPostJson(kDeepseekApi', 1)

# 2) 结果净化
OLD_RET = '''    if (!msg) return text;
    std::string out = trim(msg->getString("content"));
    return out.empty() ? text : out;
}'''
NEW_RET = '''    if (!msg) return text;
    std::string raw = msg->getString("content");

    // 模型偶尔会用 markdown 包一层，或者加一句「修正后：」，直接显示就是乱码样
    std::string out = markdownToPlain(raw, raw.size() + 32);
    out = trim(out);

    // 去掉常见前缀标签
    static const char* labels[] = {"修正后：", "修正后:", "纠错后：", "纠错后:",
                                   "结果：", "结果:", "输出：", "输出:"};
    for (const char* lb : labels) {
        size_t n = std::strlen(lb);
        if (out.compare(0, n, lb) == 0) {
            out = trim(out.substr(n));
            break;
        }
    }
    // 去掉整体包裹的引号
    if (out.size() >= 2) {
        static const char* qs[] = {"\\"", "\\u201c", "「", "『"};
        static const char* qe[] = {"\\"", "\\u201d", "」", "』"};
        for (int k = 0; k < 4; ++k) {
            size_t a = std::strlen(qs[k]);
            size_t b = std::strlen(qe[k]);
            if (out.size() > a + b && out.compare(0, a, qs[k]) == 0 &&
                out.compare(out.size() - b, b, qe[k]) == 0) {
                out = trim(out.substr(a, out.size() - a - b));
                break;
            }
        }
    }
    if (out.empty()) return text;
    // 输出比原文长太多，说明它在解释而不是纠错
    if (out.size() > text.size() * 3 + 60) return text;
    // 含替换字符或非法 UTF-8 字节，宁可不错
    if (!isValidUtf8(out)) return text;
    if (out.find("\\xEF\\xBF\\xBD") != std::string::npos) return text;
    return out;
}'''
assert OLD_RET in s, "correct 返回值原文不匹配"
s = s.replace(OLD_RET, NEW_RET, 1)

io.open(P, "w", encoding="utf-8", newline="").write(s)
print("Translator.cpp ok")

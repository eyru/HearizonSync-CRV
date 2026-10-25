#include "Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace hs {

Json::Json(bool b) : type_(Type::Bool), bool_(b) {}
Json::Json(double d) : type_(Type::Number), num_(d) {}
Json::Json(int i) : type_(Type::Number), num_(static_cast<double>(i)) {}
Json::Json(const char* s) : type_(Type::String), str_(s ? s : "") {}
Json::Json(const std::string& s) : type_(Type::String), str_(s) {}

Json Json::array() {
    Json j;
    j.type_ = Type::Array;
    return j;
}

Json Json::object() {
    Json j;
    j.type_ = Type::Object;
    return j;
}

bool Json::asBool(bool def) const {
    if (type_ == Type::Bool) return bool_;
    if (type_ == Type::Number) return num_ != 0.0;
    return def;
}

double Json::asNumber(double def) const {
    if (type_ == Type::Number) return num_;
    if (type_ == Type::Bool) return bool_ ? 1.0 : 0.0;
    return def;
}

int Json::asInt(int def) const { return static_cast<int>(asNumber(def)); }

std::string Json::asString(const std::string& def) const {
    if (type_ == Type::String) return str_;
    return def;
}

Json& Json::operator[](const std::string& key) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        arr_.clear();
    }
    return obj_[key];
}

const Json* Json::find(const std::string& key) const {
    if (type_ != Type::Object) return nullptr;
    auto it = obj_.find(key);
    return it == obj_.end() ? nullptr : &it->second;
}

bool Json::has(const std::string& key) const { return find(key) != nullptr; }

std::string Json::getString(const std::string& key, const std::string& def) const {
    const Json* p = find(key);
    return p ? p->asString(def) : def;
}

bool Json::getBool(const std::string& key, bool def) const {
    const Json* p = find(key);
    return p ? p->asBool(def) : def;
}

int Json::getInt(const std::string& key, int def) const {
    const Json* p = find(key);
    return p ? p->asInt(def) : def;
}

double Json::getNumber(const std::string& key, double def) const {
    const Json* p = find(key);
    return p ? p->asNumber(def) : def;
}

void Json::push(const Json& v) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        obj_.clear();
    }
    arr_.push_back(v);
}

size_t Json::size() const {
    if (type_ == Type::Array) return arr_.size();
    if (type_ == Type::Object) return obj_.size();
    return 0;
}

const Json* Json::at(size_t i) const {
    if (type_ != Type::Array || i >= arr_.size()) return nullptr;
    return &arr_[i];
}

// ---------------------------------------------------------------- 序列化
static void escapeTo(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

static void dumpTo(std::string& out, const Json& j, int indent, int depth) {
    auto nl = [&](int d) {
        if (indent < 0) return;
        out += '\n';
        out.append(static_cast<size_t>(indent * d), ' ');
    };
    switch (j.type()) {
        case Json::Type::Null: out += "null"; break;
        case Json::Type::Bool: out += j.asBool() ? "true" : "false"; break;
        case Json::Type::Number: {
            double d = j.asNumber();
            if (std::isfinite(d) && d == static_cast<double>(static_cast<long long>(d)) &&
                std::fabs(d) < 1e15) {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(d));
                out += buf;
            } else {
                char buf[40];
                std::snprintf(buf, sizeof(buf), "%.10g", d);
                out += buf;
            }
            break;
        }
        case Json::Type::String: escapeTo(out, j.asString()); break;
        case Json::Type::Array: {
            if (j.size() == 0) { out += "[]"; break; }
            out += '[';
            for (size_t i = 0; i < j.size(); ++i) {
                if (i) out += ',';
                nl(depth + 1);
                dumpTo(out, *j.at(i), indent, depth + 1);
            }
            nl(depth);
            out += ']';
            break;
        }
        case Json::Type::Object: {
            if (j.size() == 0) { out += "{}"; break; }
            out += '{';
            bool first = true;
            // 需要遍历 key：借助 dump 顺序稳定的 map
            j.forEachObject([&](const std::string& k, const Json& v) {
                if (!first) out += ',';
                first = false;
                nl(depth + 1);
                escapeTo(out, k);
                out += indent < 0 ? ":" : ": ";
                dumpTo(out, v, indent, depth + 1);
            });
            nl(depth);
            out += '}';
            break;
        }
    }
}

std::string Json::dump(int indent) const {
    std::string out;
    dumpTo(out, *this, indent, 0);
    return out;
}

// ---------------------------------------------------------------- 解析
namespace {
struct Parser {
    const std::string& s;
    size_t i = 0;
    explicit Parser(const std::string& t) : s(t) {}

    void ws() {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
            ++i;
    }
    bool eof() const { return i >= s.size(); }
    char peek() const { return i < s.size() ? s[i] : '\0'; }

    bool lit(const char* t) {
        size_t n = std::char_traits<char>::length(t);
        if (s.compare(i, n, t) == 0) { i += n; return true; }
        return false;
    }

    static void appendUtf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool parseString(std::string& out) {
        if (peek() != '"') return false;
        ++i;
        out.clear();
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (i >= s.size()) return false;
            char e = s[i++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    if (i + 4 > s.size()) return false;
                    unsigned cp = 0;
                    for (int k = 0; k < 4; ++k) {
                        char h = s[i++];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                        else return false;
                    }
                    // 代理对
                    if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 <= s.size() &&
                        s[i] == '\\' && s[i + 1] == 'u') {
                        size_t save = i;
                        i += 2;
                        unsigned lo = 0;
                        bool ok = true;
                        for (int k = 0; k < 4; ++k) {
                            char h = s[i++];
                            lo <<= 4;
                            if (h >= '0' && h <= '9') lo |= static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f') lo |= static_cast<unsigned>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') lo |= static_cast<unsigned>(h - 'A' + 10);
                            else { ok = false; break; }
                        }
                        if (ok && lo >= 0xDC00 && lo <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        } else {
                            i = save;
                        }
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }

    bool parseValue(Json& out, int depth) {
        if (depth > 200) return false;
        ws();
        if (eof()) return false;
        char c = peek();
        if (c == '{') {
            ++i;
            out = Json::object();
            ws();
            if (peek() == '}') { ++i; return true; }
            while (true) {
                ws();
                std::string key;
                if (!parseString(key)) return false;
                ws();
                if (peek() != ':') return false;
                ++i;
                Json v;
                if (!parseValue(v, depth + 1)) return false;
                out[key] = v;
                ws();
                if (peek() == ',') { ++i; continue; }
                if (peek() == '}') { ++i; return true; }
                return false;
            }
        }
        if (c == '[') {
            ++i;
            out = Json::array();
            ws();
            if (peek() == ']') { ++i; return true; }
            while (true) {
                Json v;
                if (!parseValue(v, depth + 1)) return false;
                out.push(v);
                ws();
                if (peek() == ',') { ++i; continue; }
                if (peek() == ']') { ++i; return true; }
                return false;
            }
        }
        if (c == '"') {
            std::string v;
            if (!parseString(v)) return false;
            out = Json(v);
            return true;
        }
        if (lit("true")) { out = Json(true); return true; }
        if (lit("false")) { out = Json(false); return true; }
        if (lit("null")) { out = Json(); return true; }
        // 数字
        size_t start = i;
        if (peek() == '-' || peek() == '+') ++i;
        bool any = false;
        while (!eof() && ((peek() >= '0' && peek() <= '9') || peek() == '.' || peek() == 'e' ||
                          peek() == 'E' || peek() == '-' || peek() == '+')) {
            any = true;
            ++i;
        }
        if (!any) return false;
        try {
            out = Json(std::stod(s.substr(start, i - start)));
        } catch (...) {
            return false;
        }
        return true;
    }
};
}  // namespace

Json Json::parse(const std::string& text) {
    Parser p(text);
    Json out;
    if (!p.parseValue(out, 0)) return Json();
    return out;
}

}  // namespace hs
